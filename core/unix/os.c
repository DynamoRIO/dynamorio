/* *******************************************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * *******************************************************************************/

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
 * os.c - Linux specific routines
 */

/* Easiest to match kernel stat struct by using 64-bit.
 * This limits us to 2.4+ kernel but that's ok.
 * I don't really want to get into requiring kernel headers to build
 * general release packages, though that would be fine for targeted builds.
 * There are 3 different stat syscalls (SYS_oldstat, SYS_stat, and SYS_stat64)
 * and using _LARGEFILE64_SOURCE with SYS_stat64 is the best match.
 */
#ifndef _LARGEFILE64_SOURCE
#    define _LARGEFILE64_SOURCE
#endif
/* for mmap-related #defines */
#include <sys/types.h>
#include <sys/mman.h>
/* in case MAP_32BIT is missing */
#ifndef MAP_32BIT
#    define MAP_32BIT 0x40
#endif
#ifndef MAP_ANONYMOUS
#    define MAP_ANONYMOUS MAP_ANON /* MAP_ANON on Mac */
#endif
/* for open */
#include <sys/stat.h>
#include <fcntl.h>
#include "../globals.h"
#include "../hashtable.h"
#include "../native_exec.h"
#include <unistd.h> /* for write and usleep and _exit */
#include <limits.h>

#ifdef MACOS
#    include <sys/sysctl.h> /* for sysctl */
#    ifndef SYS___sysctl
/* The name was changed on Yosemite */
#        define SYS___sysctl SYS_sysctl
#    endif
#    include <mach/mach_traps.h> /* for swtch_pri */
#    include "include/syscall_mach.h"
#endif

#ifdef LINUX
#    include <sys/vfs.h> /* for statfs */
#elif defined(MACOS)
#    include <sys/mount.h> /* for statfs */
#    include <mach/mach.h>
#    include <mach/task.h>
#    include <mach/semaphore.h>
#    include <mach/sync_policy.h>
#endif

#include <dirent.h>

/* for getrlimit */
#include <sys/time.h>
#include <sys/resource.h>
#ifndef X64
struct compat_rlimit {
    uint rlim_cur;
    uint rlim_max;
};
#endif
#ifdef MACOS
typedef struct rlimit rlimit64_t;
#else
typedef struct rlimit64 rlimit64_t;
#endif

#ifdef LINUX
/* For clone and its flags, the manpage says to include sched.h with _GNU_SOURCE
 * defined.  _GNU_SOURCE brings in unwanted extensions and causes name
 * conflicts.  Instead, we include unix/sched.h which comes from the Linux
 * kernel headers.
 */
#    include <linux/sched.h>
#endif

#include "module.h" /* elf */
#include "tls.h"

#if defined(X86) && defined(DEBUG)
#    include "os_asm_defines.asm" /* for TLS_SELF_OFFSET_ASM */
#endif

#ifndef F_DUPFD_CLOEXEC /* in linux 2.6.24+ */
#    define F_DUPFD_CLOEXEC 1030
#endif

/* This is not always sufficient to identify a syscall return value.
 * For example, MacOS has some 32-bit syscalls that return 64-bit
 * values in xdx:xax.
 */
#define MCXT_SYSCALL_RES(mc) ((mc)->IF_X86_ELSE(xax, IF_RISCV64_ELSE(a0, r0)))
#if defined(DR_HOST_AARCH64)
#    if defined(MACOS)
#        define READ_TP_TO_R3_DISP_IN_R2      \
            "mrs " ASM_R3 ", tpidrro_el0\n\t" \
            "ldr " ASM_R3 ", [" ASM_R3 ", " ASM_R2 "] \n\t"
#    else
#        define READ_TP_TO_R3_DISP_IN_R2    \
            "mrs " ASM_R3 ", tpidr_el0\n\t" \
            "ldr " ASM_R3 ", [" ASM_R3 ", " ASM_R2 "] \n\t"
#    endif
#elif defined(DR_HOST_ARM)
#    define READ_TP_TO_R3_DISP_IN_R2                                           \
        "mrc p15, 0, " ASM_R3                                                  \
        ", c13, c0, " STRINGIFY(USR_TLS_REG_OPCODE) " \n\t"                    \
                                                    "ldr " ASM_R3 ", [" ASM_R3 \
                                                    ", " ASM_R2 "] \n\t"
#endif /* ARM */

/* Prototype for all functions in .init_array. */
typedef int (*init_fn_t)(int argc, char **argv, char **envp);

/* For STATIC_LIBRARY we do not cache environ so the app can change it. */
#ifndef STATIC_LIBRARY
/* i#46: Private __environ pointer.  Points at the environment variable array
 * on the stack, which is different from what libc __environ may point at.  We
 * use the environment for following children and setting options, so its OK
 * that we don't see what libc says.
 */
char **our_environ;
#endif

#include <errno.h>
/* avoid problems with use of errno as var name in rest of file */
#if !defined(STANDALONE_UNIT_TEST) && !defined(MACOS)
#    undef errno
#endif
/* we define __set_errno below */

/* must be prior to <link.h> => <elf.h> => INT*_{MIN,MAX} */
#include "instr.h" /* for get_app_segment_base() */

#include "decode_fast.h" /* decode_cti: maybe os_handle_mov_seg should be ifdef X86? */

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h> /* vsyslog */
#include "../vmareas.h"
#ifdef RCT_IND_BRANCH
#    include "../rct.h"
#endif
#ifdef LINUX
#    include "include/syscall.h" /* our own local copy */
#    include "include/clone3.h"
#    include "include/close_range.h"
#else
#    include <sys/syscall.h>
#endif
#include "../module_shared.h"
#include "os_private.h"
#include "../synch.h"
#include "memquery.h"
#include "ksynch.h"
#include "dr_tools.h" /* dr_syscall_result_info_t */

#ifndef HAVE_MEMINFO_QUERY
#    include "memcache.h"
#endif

#include "instrument.h"

#ifdef LINUX
#    include "rseq_linux.h"
#endif

#ifdef MACOS
#    define SYSNUM_EXIT_PROCESS SYS_exit
#    define SYSNUM_EXIT_THREAD SYS_bsdthread_terminate
#else
#    define SYSNUM_EXIT_PROCESS SYS_exit_group
#    define SYSNUM_EXIT_THREAD SYS_exit
#endif

#ifdef ANDROID
/* Custom prctl flags specific to Android (xref i#1861) */
#    define PR_SET_VMA 0x53564d41
#    define PR_SET_VMA_ANON_NAME 0
#endif

/* Guards data written by os_set_app_thread_area(). */
DECLARE_CXTSWPROT_VAR(static mutex_t set_thread_area_lock,
                      INIT_LOCK_FREE(set_thread_area_lock));

static bool first_thread_tls_initialized;
static bool last_thread_tls_exited;

tls_type_t tls_global_type;

#ifndef HAVE_TLS
/* We use a table lookup to find a thread's dcontext */
/* Our only current no-TLS target, VMKernel (VMX86_SERVER), doesn't have apps with
 * tons of threads anyway
 */
#    define MAX_THREADS 512
typedef struct _tls_slot_t {
    thread_id_t tid;
    dcontext_t *dcontext;
} tls_slot_t;
/* Stored in heap for self-prot */
static tls_slot_t *tls_table;
/* not static so deadlock_avoidance_unlock() can look for it */
DECLARE_CXTSWPROT_VAR(mutex_t tls_lock, INIT_LOCK_FREE(tls_lock));
#endif

/* Should we place this in a client header?  Currently mentioned in
 * dr_raw_tls_calloc() docs.
 */
static bool client_tls_allocated[MAX_NUM_CLIENT_TLS];
DECLARE_CXTSWPROT_VAR(static mutex_t client_tls_lock, INIT_LOCK_FREE(client_tls_lock));

#include <stddef.h> /* for offsetof */

#include <sys/utsname.h> /* for struct utsname */

/* forward decl */
static void
handle_execve_post(dcontext_t *dcontext);
static bool
os_switch_lib_tls(dcontext_t *dcontext, bool to_app);
static bool
os_switch_seg_to_context(dcontext_t *dcontext, reg_id_t seg, bool to_app);
#ifdef X86
static bool
os_set_dr_tls_base(dcontext_t *dcontext, os_local_state_t *tls, byte *base);
#endif
#ifdef LINUX
static bool
handle_app_mremap(dcontext_t *dcontext, byte *base, size_t size, byte *old_base,
                  size_t old_size, uint old_prot, uint old_type);
static void
handle_app_brk(dcontext_t *dcontext, byte *lowest_brk /*if known*/, byte *old_brk,
               byte *new_brk);
#endif

/* full path to our own library, used for execve */
static char dynamorio_library_path[MAXIMUM_PATH]; /* just dir */
static char dynamorio_library_filepath[MAXIMUM_PATH];
/* Shared between get_dynamo_library_bounds() and get_alt_dynamo_library_bounds(). */
static char dynamorio_libname_buf[MAXIMUM_PATH];
static const char *dynamorio_libname = dynamorio_libname_buf;
/* Issue 20: path to other architecture */
static char dynamorio_alt_arch_path[MAXIMUM_PATH]; /* just dir */
static char dynamorio_alt_arch_filepath[MAXIMUM_PATH];
/* Makefile passes us LIBDIR_X{86,64} defines */
#define DR_LIBDIR_X86 STRINGIFY(LIBDIR_X86)
#define DR_LIBDIR_X64 STRINGIFY(LIBDIR_X64)

static void
get_dynamo_library_bounds(void);
static void
get_alt_dynamo_library_bounds(void);

/* pc values delimiting dynamo dll image */
static app_pc dynamo_dll_start = NULL;
static app_pc dynamo_dll_end = NULL; /* open-ended */

/* pc values delimiting the app, equal to the "dll" bounds for static DR */
static app_pc executable_start = NULL;
static app_pc executable_end = NULL;

/* Used by get_application_name(). */
static char executable_path[MAXIMUM_PATH];
static char *executable_basename;

/* Pointers to arguments. Refers to the main stack set up by the kernel.
 * These are only written once during process init and we can live with
 * the non-guaranteed-delay until they are visible to other cores.
 */
static int *app_argc = NULL;
static char **app_argv = NULL;

/* does the kernel provide tids that must be used to distinguish threads in a group? */
static bool kernel_thread_groups;

static bool kernel_64bit;

pid_t pid_cached;

static bool fault_handling_initialized;

#ifdef PROFILE_RDTSC
uint kilo_hertz; /* cpu clock speed */
#endif

/* Xref PR 258731, dup of STDOUT/STDERR in case app wants to close them. */
DR_API file_t our_stdout = STDOUT_FILENO;
DR_API file_t our_stderr = STDERR_FILENO;
DR_API file_t our_stdin = STDIN_FILENO;

/* we steal fds from the app */
static rlimit64_t app_rlimit_nofile; /* cur rlimit set by app */
static int min_dr_fd;

/* we store all DR files so we can prevent the app from changing them,
 * and so we can close them in a child of fork.
 * the table key is the fd and the payload is the set of DR_FILE_* flags.
 */
static generic_table_t *fd_table;
#define INIT_HTABLE_SIZE_FD 6 /* should remain small */
/* DR needs to open some files before the fd_table is allocated by d_r_os_init.
 * This is due to constraints on the order of invoking various init routines in
 * the dynamorio_app_init_part_* routines.
 * - dynamorio_app_init_part_one_options opens the global log file when logging
 *   is enabled in the debug build.
 * - vmm_heap_unit_init opens the dual_map_file when -satisfy_w_xor_x is set.

 * For these files, fd_table_add would not be able to really add the FD.
 * Therefore, we have to remember them so that we can add it to fd_table later
 * when we create it.
 */
#define MAX_FD_ADD_PRE_HEAP 2
static int fd_add_pre_heap[MAX_FD_ADD_PRE_HEAP];
static int fd_add_pre_heap_flags[MAX_FD_ADD_PRE_HEAP];
static int num_fd_add_pre_heap;

#ifdef LINUX
/* i#1004: brk emulation */
static byte *app_brk_map;
static byte *app_brk_cur;
static byte *app_brk_end;
#endif

#ifdef MACOS
static int macos_version;
#endif

static bool
is_readable_without_exception_internal(const byte *pc, size_t size, bool query_os);

static bool
mmap_check_for_module_overlap(app_pc base, size_t size, bool readable, uint64 inode,
                              bool at_map);

#ifdef LINUX
static char *
read_proc_self_exe(bool ignore_cache);
#endif

/* Libc independent directory iterator, similar to readdir.  If we ever need
 * this on Windows we should generalize it and export it to clients.
 */
typedef struct _dir_iterator_t {
    file_t fd;
    int off;
    int end;
    const char *name;           /* Name of the current entry. */
    char buf[4 * MAXIMUM_PATH]; /* Expect stack alloc, so not too big. */
} dir_iterator_t;

static void
os_dir_iterator_start(dir_iterator_t *iter, file_t fd);
static bool
os_dir_iterator_next(dir_iterator_t *iter);
/* XXX: If we generalize to Windows, will we need os_dir_iterator_stop()? */

/* vsyscall page.  hardcoded at 0xffffe000 in earlier kernels, but
 * randomly placed since fedora2.
 * marked rx then: FIXME: should disallow this guy when that's the case!
 * random vsyscall page is identified in maps files as "[vdso]"
 * (kernel-provided fake shared library or Virt Dyn Shared Object).
 */
/* i#1583: vdso is now 2 pages, yet we assume vsyscall is on 1st page. */
/* i#2945: vdso is now 3 pages and vsyscall is not on the 1st page. */
app_pc vsyscall_page_start = NULL;
/* pc of the end of the syscall instr itself */
app_pc vsyscall_syscall_end_pc = NULL;
/* pc where kernel returns control after sysenter vsyscall */
app_pc vsyscall_sysenter_return_pc = NULL;
/* pc where our hook-displaced code was copied */
app_pc vsyscall_sysenter_displaced_pc = NULL;
#define VSYSCALL_PAGE_START_HARDCODED ((app_pc)(ptr_uint_t)0xffffe000)
#ifdef X64
/* i#430, in Red Hat Enterprise Server 5.6, vsyscall region is marked
 * not executable
 * ffffffffff600000-ffffffffffe00000 ---p 00000000 00:00 0  [vsyscall]
 */
#    define VSYSCALL_REGION_MAPS_NAME "[vsyscall]"
#endif
/* i#1908: vdso and vsyscall are now split */
app_pc vdso_page_start = NULL;
size_t vdso_size = 0;

#if !defined(STANDALONE_UNIT_TEST) && !defined(STATIC_LIBRARY)
/* The pthreads library keeps errno in its pthread_descr data structure,
 * which it looks up by dispatching on the stack pointer.  This doesn't work
 * when within dynamo.  Thus, we define our own __errno_location() for use both
 * by us and the app, to prevent pthreads looking at the stack pointer when
 * out of the code cache.
 */

/* FIXME: maybe we should create 1st dcontext earlier so we don't need init_errno?
 * any problems with init_errno being set and then dcontext->errno being read?
 * FIXME: if a thread issues a dr_app_stop, then we don't want to use
 * this errno slot?  But it may later do a start...probably ok to keep using
 * the slot.  But, when threads die, they'll all use the same init_errno!
 */
static int init_errno; /* errno until 1st dcontext created */

int *
__errno_location(void)
{
    /* Each dynamo thread should have a separate errno */
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return &init_errno;
    else {
        /* WARNING: init_errno is in data segment so can be RO! */
        return &(dcontext->upcontext_ptr->dr_errno);
    }
}
#endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */

#ifdef HAVE_TLS
/* i#598
 * (gdb) x/20i (*(errno_loc_t)0xf721e413)
 * 0xf721e413 <__errno_location>:       push   %ebp
 * 0xf721e414 <__errno_location+1>:     mov    %esp,%ebp
 * 0xf721e416 <__errno_location+3>:     call   <__x86.get_pc_thunk.cx>
 * 0xf721e41b <__errno_location+8>:     add    $0x166bd9,%ecx
 * 0xf721e421 <__errno_location+14>:    mov    -0x1c(%ecx),%eax
 * 0xf721e427 <__errno_location+20>:    add    %gs:0x0,%eax
 * 0xf721e42e <__errno_location+27>:    pop    %ebp
 * 0xf721e42f <__errno_location+28>:    ret
 *
 * __errno_location calcuates the errno location by adding
 * TLS's base with errno's offset in TLS.
 * However, because the TLS has been switched in os_tls_init,
 * the calculated address is wrong.
 * We first get the errno offset in TLS at init time and
 * calculate correct address by adding the app's tls base.
 */
/* __errno_location on ARM:
 * 0xb6f0b290 <__errno_location>:    ldr r3, [pc, #12]
 * 0xb6f0b292 <__errno_location+2>:  mrc 15, 0, r0, cr13, cr0, {3}
 * 0xb6f0b296 <__errno_location+6>:  add r3, pc
 * 0xb6f0b298 <__errno_location+8>:  ldr r3, [r3, #0]
 * 0xb6f0b29a <__errno_location+10>: adds r0, r0, r3
 * 0xb6f0b29c <__errno_location+12>: bx lr
 * It uses the predefined offset to get errno location in TLS,
 * and we should be able to reuse the code here.
 */
static int libc_errno_tls_offs;
static int *
our_libc_errno_loc(void)
{
    void *app_tls = os_get_app_tls_base(NULL, TLS_REG_LIB);
    if (app_tls == NULL)
        return NULL;
    return (int *)(app_tls + libc_errno_tls_offs);
}
#endif

/* i#238/PR 499179: libc errno preservation
 *
 * Errno location is per-thread so we store the
 * function globally and call it each time.  Note that pthreads seems
 * to be the one who provides per-thread errno: using raw syscalls to
 * create threads, we end up with a global errno:
 *
 *   > for i in linux.thread.*0/log.*; do grep 'libc errno' $i | head -1; done
 *   libc errno loc: 0x00007f153de26698
 *   libc errno loc: 0x00007f153de26698
 *   > for i in pthreads.pthreads.*0/log.*; do grep 'libc errno' $i | head -1; done
 *   libc errno loc: 0x00007fc24d1ce698
 *   libc errno loc: 0x00007fc24d1cd8b8
 *   libc errno loc: 0x00007fc24c7cc8b8
 */
typedef int *(*errno_loc_t)(void);

#ifdef LINUX
/* Stores whether certain syscalls are unsupported on the system we're running on. */
static bool is_clone3_enosys = false;
static bool is_sigqueueinfo_enosys = false;
#endif

int suspend_signum;

static errno_loc_t
get_libc_errno_location(bool do_init)
{
    static errno_loc_t libc_errno_loc;

    if (do_init) {
        module_iterator_t *mi = module_iterator_start();
        while (module_iterator_hasnext(mi)) {
            module_area_t *area = module_iterator_next(mi);
            const char *modname = GET_MODULE_NAME(&area->names);
            /* We ensure matches start to avoid matching "libgolibc.so".
             * GET_MODULE_NAME never includes the path: i#138 will add path.
             */
            if (modname != NULL && strstr(modname, "libc.so") == modname) {
                bool found = true;
                /* called during init when .data is writable */
                libc_errno_loc =
                    (errno_loc_t)d_r_get_proc_address(area->start, "__errno_location");
                ASSERT(libc_errno_loc != NULL);
                LOG(GLOBAL, LOG_THREADS, 2, "libc errno loc func: " PFX "\n",
                    libc_errno_loc);
                /* Currently, the DR is loaded by system loader and hooked up
                 * to app's libc.  So right now, we still need this routine.
                 * we can remove this after libc independency and/or
                 * early injection
                 */
                if (INTERNAL_OPTION(private_loader)) {
                    acquire_recursive_lock(&privload_lock);
                    if (privload_lookup_by_base(area->start) != NULL)
                        found = false;
                    release_recursive_lock(&privload_lock);
                }
                if (found)
                    break;
            }
        }
        module_iterator_stop(mi);
#ifdef HAVE_TLS
        /* i#598: init the libc errno's offset.  If we didn't find libc above,
         * then we don't need to do this.
         */
        if (INTERNAL_OPTION(private_loader) && libc_errno_loc != NULL) {
            void *priv_lib_tls_base = os_get_priv_tls_base(NULL, TLS_REG_LIB);
            ASSERT(priv_lib_tls_base != NULL);
            libc_errno_tls_offs = (void *)libc_errno_loc() - priv_lib_tls_base;
            libc_errno_loc = &our_libc_errno_loc;
        }
#endif
    }
    return libc_errno_loc;
}

/* i#238/PR 499179: our __errno_location isn't affecting libc so until
 * we have libc independence or our own private isolated libc we need
 * to preserve the app's libc's errno
 */
int
get_libc_errno(void)
{
#if defined(STANDALONE_UNIT_TEST) && (defined(MACOS) || defined(ANDROID))
    return errno;
#else
#    ifdef STANDALONE_UNIT_TEST
    errno_loc_t func = __errno_location;
#    else
    errno_loc_t func = get_libc_errno_location(false);
#    endif
    if (func == NULL) {
        /* libc hasn't been loaded yet or we're doing early injection. */
        return 0;
    } else {
        int *loc = (*func)();
        ASSERT(loc != NULL);
        LOG(THREAD_GET, LOG_THREADS, 5, "libc errno loc: " PFX "\n", loc);
        if (loc != NULL)
            return *loc;
    }
    return 0;
#endif
}

/* N.B.: pthreads has two other locations it keeps on a per-thread basis:
 * h_errno and res_state.  See glibc-2.2.4/linuxthreads/errno.c.
 * If dynamo ever modifies those we'll need to do to them what we now do to
 * errno.
 */

/* The environment vars exhibit totally messed up behavior when someone
 * does an execve of /bin/sh -- not sure what's going on, but using our
 * own implementation of unsetenv fixes all our problems.  If we use
 * libc's, unsetenv either does nothing or ends up having getenv return
 * NULL for other vars that are obviously set (by iterating through environ).
 * FIXME: find out the real story here.
 */
int
our_unsetenv(const char *name)
{
    /* FIXME: really we should have some kind of synchronization */
    size_t name_len;
    char **env = our_environ;
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
        return -1;
    }
    ASSERT(our_environ != NULL);
    if (our_environ == NULL)
        return -1;
    name_len = strlen(name);
    while (*env != NULL) {
        if (strncmp(*env, name, name_len) == 0 && (*env)[name_len] == '=') {
            /* We have a match.  Shift the subsequent entries.  Keep going to
             * handle later matches.
             */
            char **e;
            for (e = env; *e != NULL; e++)
                *e = *(e + 1);
        } else {
            env++;
        }
    }
    return 0;
}

/* Clobbers the name rather than shifting, to preserve auxv (xref i#909). */
bool
disable_env(const char *name)
{
    size_t name_len;
    char **env = our_environ;
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
        return false;
    }
    ASSERT(our_environ != NULL);
    if (our_environ == NULL)
        return false;
    name_len = strlen(name);
    while (*env != NULL) {
        if (strncmp(*env, name, name_len) == 0 && (*env)[name_len] == '=') {
            /* We have a match.  If we shift subsequent entries we'll mess
             * up access to auxv, which is after the env block, so we instead
             * disable the env var by changing its name.
             * We keep going to handle later matches.
             */
            snprintf(*env, name_len, "__disabled__");
        }
        env++;
    }
    return true;
}

/* i#46: Private getenv.
 */
char *
our_getenv(const char *name)
{
    char **env = our_environ;
    size_t i;
    size_t name_len;
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        return NULL;
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS,
                   "our_environ is missing.  _init() or "
                   "dynamorio_set_envp() were not called",
                   our_environ != NULL);
    if (our_environ == NULL)
        return NULL;
    name_len = strlen(name);
    for (i = 0; env[i] != NULL; i++) {
        if (strncmp(env[i], name, name_len) == 0 && env[i][name_len] == '=') {
            return env[i] + name_len + 1;
        }
    }
    return NULL;
}

bool
is_our_environ_followed_by_auxv(void)
{
#ifdef STATIC_LIBRARY
    /* Since we initialize late, our_environ is likely no longer pointed at
     * the stack (i#2122).
     */
    return false;
#else
    return true;
#endif
}

/* Work around drpreload's _init going first.  We can get envp in our own _init
 * routine down below, but drpreload.so comes first and calls
 * dynamorio_app_init before our own _init routine gets called.  Apps using the
 * app API are unaffected because our _init routine will have run by then.  For
 * STATIC_LIBRARY, we used to set our_environ in our_init(), but to support
 * the app setting DYNAMORIO_OPTIONS after our_init() runs, we now just use environ.
 */
DYNAMORIO_EXPORT
void
dynamorio_set_envp(char **envp)
{
    our_environ = envp;
}

/* shared library init */
static int
our_init(int argc, char **argv, char **envp)
{
    /* If we do not want to use drpreload.so, we can take over here: but when using
     * drpreload, this is called *after* we have already taken over.
     */
    extern void dynamorio_app_take_over(void);
    bool takeover = false;
#ifdef INIT_TAKE_OVER
    takeover = true;
#endif
#ifdef VMX86_SERVER
    /* PR 391765: take over here instead of using preload */
    takeover = os_in_vmkernel_classic();
#endif
#ifndef STATIC_LIBRARY
    if (our_environ != NULL) {
        /* Set by dynamorio_set_envp above.  These should agree. */
        ASSERT(our_environ == envp);
    } else {
        our_environ = envp;
    }
#endif
    /* if using preload, no -early_inject */
#ifdef STATIC_LIBRARY
    if (!takeover) {
        const char *takeover_env = getenv("DYNAMORIO_TAKEOVER_IN_INIT");
        if (takeover_env != NULL && strcmp(takeover_env, "1") == 0) {
            takeover = true;
        }
    }
#endif
    if (takeover) {
        if (dynamorio_app_init() == 0 /* success */) {
            dynamorio_app_take_over();
        }
    }
    return 0;
}

#if defined(STATIC_LIBRARY) || defined(STANDALONE_UNIT_TEST) || defined(RISCV64)
/* If we're getting linked into a binary that already has an _init definition
 * like the app's exe or unit_tests, we add a pointer to our_init() to the
 * .init_array section.  We can't use the constructor attribute because not all
 * toolchains pass the args and environment to the constructor.
 *
 * RISC-V, as a new ISA, does not support obsolete .init section, so we always use
 * .init_array section for RISC-V.
 */
static init_fn_t
#    ifdef MACOS
    __attribute__((section("__DATA,__mod_init_func"), aligned(sizeof(void *)), used))
#    else
    __attribute__((section(".init_array"), aligned(sizeof(void *)), used))
#    endif
    init_array[] = { our_init };
#else
/* If we're a normal shared object, then we override _init.
 */
int
_init(int argc, char **argv, char **envp)
{
#    ifdef ANDROID
    /* i#1862: the Android loader passes *nothing* to lib init routines.  We
     * rely on DR being listed before libc so we can read the TLS slot the
     * kernel set up.
     */
    if (!get_kernel_args(&argc, &argv, &envp)) {
        /* XXX: scan the stack and look for known auxv patterns or sthg. */
        argc = 0;
        argv = NULL;
        envp = NULL;
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS, "failed to find envp", envp != NULL);
#    endif
    return our_init(argc, argv, envp);
}
#endif

bool
kernel_is_64bit(void)
{
    return kernel_64bit;
}

#ifdef MACOS
/* XXX: if we get enough of these, move to os_macos.c or sthg */
static bool
sysctl_query(int level0, int level1, void *buf, size_t bufsz)
{
    int res;
    int name[2];
    size_t len = bufsz;
    name[0] = level0;
    name[1] = level1;
    res = dynamorio_syscall(SYS___sysctl, 6, &name, 2, buf, &len, NULL, 0);
    return (res >= 0);
}

int
os_get_version(void)
{
    return macos_version;
}
#endif

static void
get_uname(void)
{
    /* assumption: only called at init, so we don't need any synch
     * or .data unprot
     */
    static struct utsname uinfo; /* can be large, avoid stack overflow */
#ifdef MACOS
    if (!sysctl_query(CTL_KERN, KERN_OSTYPE, &uinfo.sysname, sizeof(uinfo.sysname)) ||
        !sysctl_query(CTL_KERN, KERN_HOSTNAME, &uinfo.nodename, sizeof(uinfo.nodename)) ||
        !sysctl_query(CTL_KERN, KERN_OSRELEASE, &uinfo.release, sizeof(uinfo.release)) ||
        !sysctl_query(CTL_KERN, KERN_VERSION, &uinfo.version, sizeof(uinfo.version)) ||
        !sysctl_query(CTL_HW, HW_MACHINE, &uinfo.machine, sizeof(uinfo.machine))) {
        ASSERT(false && "sysctl queries failed");
        return;
    }
#else
    DEBUG_DECLARE(int res =)
    dynamorio_syscall(SYS_uname, 1, (ptr_uint_t)&uinfo);
    ASSERT(res >= 0);
#endif
    LOG(GLOBAL, LOG_TOP, 1, "uname:\n\tsysname: %s\n", uinfo.sysname);
    LOG(GLOBAL, LOG_TOP, 1, "\tnodename: %s\n", uinfo.nodename);
    LOG(GLOBAL, LOG_TOP, 1, "\trelease: %s\n", uinfo.release);
    LOG(GLOBAL, LOG_TOP, 1, "\tversion: %s\n", uinfo.version);
    LOG(GLOBAL, LOG_TOP, 1, "\tmachine: %s\n", uinfo.machine);
    if (strncmp(uinfo.machine, "x86_64", sizeof("x86_64")) == 0)
        kernel_64bit = true;
#ifdef MACOS
    /* XXX: I would skip these checks for standalone so we don't have to set env
     * vars for frontends to see the options but I'm still afraid of some syscall
     * crash with no output: I'd rather have two messages than silent crashing.
     */
    if (DYNAMO_OPTION(max_supported_os_version) != 0) { /* 0 disables */
        /* We only support OSX 10.7.5+.  That means kernels 11.x+. */
#    define MIN_DARWIN_VERSION_SUPPORTED 11
        int kernel_major;
        if (sscanf(uinfo.release, "%d", &kernel_major) != 1 ||
            kernel_major > DYNAMO_OPTION(max_supported_os_version) ||
            kernel_major < MIN_DARWIN_VERSION_SUPPORTED) {
            /* We make this non-fatal as it's likely DR will work */
            SYSLOG(SYSLOG_WARNING, UNSUPPORTED_OS_VERSION, 3, get_application_name(),
                   get_application_pid(), uinfo.release);
        }
        macos_version = kernel_major;
    }
#endif
}

#if defined(LINUX)
/* For some syscalls, detects whether they are unsupported by the system
 * we're running on. Particularly, we are interested in detecting missing
 * support early-on for syscalls that require complex pre-syscall handling
 * by DR. We use this information to fail early for those syscalls.
 *
 * XXX: Move other logic for detecting unsupported syscalls from their
 * respective locations to here at init time, like that for
 * SYS_memfd_create in os_create_memory_file.
 *
 */
static void
detect_unsupported_syscalls()
{
    /* We know that when clone3 is available, it fails with EINVAL with
     * these args.
     */
    int clone3_errno =
        dynamorio_syscall(SYS_clone3, 2, NULL /*clone_args*/, 0 /*clone_args_size*/);
    ASSERT(clone3_errno == -ENOSYS || clone3_errno == -EINVAL);
    is_clone3_enosys = clone3_errno == -ENOSYS;
    /* We expect sigqueueinfo to fail with EFAULT on the NULL but we allow EINVAL
     * on the signal number to support kernel variation.
     */
    int sigqueue_errno = dynamorio_syscall(SYS_rt_tgsigqueueinfo, 4, get_process_id(),
                                           get_sys_thread_id(), -1, NULL);
    ASSERT(sigqueue_errno == -ENOSYS || sigqueue_errno == -EINVAL ||
           sigqueue_errno == -EFAULT);
    is_sigqueueinfo_enosys = sigqueue_errno == -ENOSYS;
    if (!IS_STRING_OPTION_EMPTY(xarch_root)) {
        /* XXX i#5651: QEMU clears si_errno when we send our payload!
         * For now we pretend this syscall doesn't work, to get basic apps
         * working under QEMU.
         */
        is_sigqueueinfo_enosys = true;
    }
}
#endif

bool
is_sigqueue_supported(void)
{
    return IF_LINUX_ELSE(!is_sigqueueinfo_enosys, false);
}

/* os-specific initializations */
void
d_r_os_init(void)
{
    ksynch_init();

    get_uname();

    /* Populate global data caches. */
    get_application_name();
    get_application_base();
    get_dynamo_library_bounds();
    get_alt_dynamo_library_bounds();

    /* determine whether gettid is provided and needed for threads,
     * or whether getpid suffices.  even 2.4 kernels have gettid
     * (maps to getpid), don't have an old enough target to test this.
     */
#ifdef MACOS
    kernel_thread_groups = (dynamorio_syscall(SYS_thread_selfid, 0) >= 0);
#else
    kernel_thread_groups = (dynamorio_syscall(SYS_gettid, 0) >= 0);
#endif
    LOG(GLOBAL, LOG_TOP | LOG_STATS, 1, "thread id is from %s\n",
        kernel_thread_groups ? "gettid" : "getpid");
#ifdef MACOS
    /* SYS_thread_selfid was added in 10.6.  We have no simple way to get the
     * thread id on 10.5, so we don't support it.
     */
    if (!kernel_thread_groups) {
        SYSLOG(SYSLOG_WARNING, UNSUPPORTED_OS_VERSION, 3, get_application_name(),
               get_application_pid(), "Mac OSX 10.5 or earlier");
    }
#else
    ASSERT_CURIOSITY(kernel_thread_groups);
#endif

    pid_cached = get_process_id();

#ifdef VMX86_SERVER
    vmk_init();
#endif

#if defined(LINUX)
    detect_unsupported_syscalls();
#endif

    /* The signal we use to suspend threads.
     * We choose a normally-synchronous signal for a lower chance that the app has
     * blocked it when we attach to an already-running app.
     * On Linux where we have SYS_rt_sigqueueinfo we use
     * SIGILL and share with nudges, distinguishing via the NUDGE_IS_SUSPEND flag.
     * (For pre-2.6.31 Linux kernels without SYS_rt_sigqueueinfo nudges are not
     * supported so there are no collisions there).
     * (We initially used SIGSTKFLT but gdb has poor support for it.)
     * Unfortunately, QEMU crashes when we send SIGILL or SIGFPE to its thread trying
     * to take it over, so we are forced to dynamically vary the number and we switch
     * to SIGSTKFLT for QEMU where we live with the lack of gdb support.
     */
    suspend_signum = IF_MACOS_ELSE(SIGFPE, NUDGESIG_SIGNUM);
#ifdef LINUX
    if (!IS_STRING_OPTION_EMPTY(xarch_root)) {
        /* We assume we're under QEMU. */
        LOG(GLOBAL, LOG_TOP | LOG_ASYNCH, 1, "switching suspend signal to SIGSTKFLT\n");
        suspend_signum = SIGSTKFLT;
    }
#endif

    d_r_signal_init();
    /* We now set up an early fault handler for d_r_safe_read() (i#350) */
    fault_handling_initialized = true;

    memquery_init();

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        ASSERT_NOT_TESTED();
        kilo_hertz = get_timer_frequency();
        LOG(GLOBAL, LOG_TOP | LOG_STATS, 1, "CPU MHz is %d\n", kilo_hertz / 1000);
    }
#endif /* PROFILE_RDTSC */
    /* Needs to be after heap_init */
    IF_NO_MEMQUERY(memcache_init());

    /* we didn't have heap in os_file_init() so create and add global logfile now */
    fd_table = generic_hash_create(
        GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_FD, 80 /* load factor: not perf-critical */,
        HASHTABLE_SHARED | HASHTABLE_PERSISTENT, NULL _IF_DEBUG("fd table"));
    /* Add to fd_table the entries that we could not add before as fd_table was
     * not initialized.
     */
    while (num_fd_add_pre_heap > 0) {
        num_fd_add_pre_heap--;
        fd_table_add(fd_add_pre_heap[num_fd_add_pre_heap],
                     fd_add_pre_heap_flags[num_fd_add_pre_heap]);
    }

    /* Ensure initialization */
    get_dynamorio_dll_start();

#ifdef LINUX
    if (DYNAMO_OPTION(emulate_brk))
        init_emulated_brk(NULL);
#endif

#ifdef ANDROID
    /* This must be set up earlier than privload_tls_init, and must be set up
     * for non-client-interface as well, as this initializes DR_TLS_BASE_OFFSET
     * (i#1931).
     */
    init_android_version();
#endif
#ifdef LINUX
    if (!standalone_library)
        d_r_rseq_init();
#endif
#ifdef MACOS64
    tls_process_init();
#endif
}

/* called before any logfiles are opened */
void
os_file_init(void)
{
    /* We steal fds from the app for better transparency.  We lower the max file
     * descriptor limit as viewed by the app, and block SYS_dup{2,3} and
     * SYS_fcntl(F_DUPFD*) from creating a file explicitly in our space.  We do
     * not try to stop incremental file opening from extending into our space:
     * if the app really is running out of fds, we'll give it some of ours:
     * after all we probably don't need all -steal_fds, and if we really need fds
     * we typically open them at startup.  We also don't bother watching all
     * syscalls that take in fds from affecting our fds.
     */
    if (DYNAMO_OPTION(steal_fds) > 0) {
        struct rlimit rlimit_nofile;
        /* SYS_getrlimit uses an old 32-bit-field struct so we want SYS_ugetrlimit */
        if (dynamorio_syscall(
                IF_MACOS_ELSE(SYS_getrlimit, IF_X64_ELSE(SYS_getrlimit, SYS_ugetrlimit)),
                2, RLIMIT_NOFILE, &rlimit_nofile) != 0) {
            /* linux default is 1024 */
            SYSLOG_INTERNAL_WARNING("getrlimit RLIMIT_NOFILE failed"); /* can't LOG yet */
            rlimit_nofile.rlim_cur = 1024;
            rlimit_nofile.rlim_max = 1024;
        }
        /* pretend the limit is lower and reserve the top spots for us.
         * for simplicity and to give as much room as possible to app,
         * raise soft limit to equal hard limit.
         * if an app really depends on a low soft limit, they can run
         * with -steal_fds 0.
         */
        if (rlimit_nofile.rlim_max > DYNAMO_OPTION(steal_fds)) {
            int res;
            min_dr_fd = rlimit_nofile.rlim_max - DYNAMO_OPTION(steal_fds);
            app_rlimit_nofile.rlim_max = min_dr_fd;
            app_rlimit_nofile.rlim_cur = app_rlimit_nofile.rlim_max;

            rlimit_nofile.rlim_cur = rlimit_nofile.rlim_max;
            res = dynamorio_syscall(SYS_setrlimit, 2, RLIMIT_NOFILE, &rlimit_nofile);
            if (res != 0) {
                SYSLOG_INTERNAL_WARNING("unable to raise RLIMIT_NOFILE soft limit: %d",
                                        res);
            }
        } else /* not fatal: we'll just end up using fds in app space */
            SYSLOG_INTERNAL_WARNING("unable to reserve fds");
    }

    /* we don't have heap set up yet so we init fd_table in os_init */
}

/* we need to re-cache after a fork */
static char *
get_application_pid_helper(bool ignore_cache)
{
    static char pidstr[16];

    if (!pidstr[0] || ignore_cache) {
        int pid = get_process_id();
        snprintf(pidstr, sizeof(pidstr) - 1, "%d", pid);
    }
    return pidstr;
}

/* get application pid, (cached), used for event logging */
char *
get_application_pid()
{
    return get_application_pid_helper(false);
}

/* The OSX kernel used to place the bare executable path above envp.
 * On recent XNU versions, the kernel now prefixes the executable path
 * with the string executable_path= so it can be parsed getenv style.
 */
#ifdef MACOS
#    define EXECUTABLE_KEY "executable_path="
#endif
/* i#189: we need to re-cache after a fork */
static char *
get_application_name_helper(bool ignore_cache, bool full_path)
{
    if (!executable_path[0] || ignore_cache) {
#ifdef VMX86_SERVER
        if (os_in_vmkernel_userworld()) {
            vmk_getnamefrompid(pid, executable_path, sizeof(executable_path));
        } else
#endif
            if (DYNAMO_OPTION(early_inject)) {
            ASSERT(executable_path[0] != '\0' &&
                   "i#907: Can't read /proc/self/exe for early injection");
        } else {
#ifdef LINUX
            /* Populate cache from /proc/self/exe link. */
            strncpy(executable_path, read_proc_self_exe(ignore_cache),
                    BUFFER_SIZE_ELEMENTS(executable_path));
#else
            /* OSX kernel puts full app exec path above envp */
            char *c, **env = our_environ;
            do {
                env++;
            } while (*env != NULL);
            env++; /* Skip the NULL separating the envp array from exec_path */
            c = *env;
            if (strncmp(EXECUTABLE_KEY, c, strlen(EXECUTABLE_KEY)) == 0) {
                c += strlen(EXECUTABLE_KEY);
            }
            /* If our frontends always absolute-ize paths prior to exec,
             * this should usually be absolute -- but we go ahead and
             * handle relative just in case (and to handle child processes).
             * We add the cur dir, but note that the resulting path can
             * still contain . or .. so it's not normalized (but it is a
             * correct absolute path).  Xref i#1402, i#1406, i#1407.
             */
            if (*c != '/') {
                int len;
                if (!os_get_current_dir(executable_path,
                                        BUFFER_SIZE_ELEMENTS(executable_path)))
                    len = 0;
                else
                    len = strlen(executable_path);
                snprintf(executable_path + len,
                         BUFFER_SIZE_ELEMENTS(executable_path) - len, "%s%s",
                         len > 0 ? "/" : "", c);
            } else
                strncpy(executable_path, c, BUFFER_SIZE_ELEMENTS(executable_path));
#endif
            NULL_TERMINATE_BUFFER(executable_path);
            /* FIXME: Fall back on /proc/self/cmdline and maybe argv[0] from
             * _init().
             */
            ASSERT(strlen(executable_path) > 0 && "readlink /proc/self/exe failed");
        }
    }

    /* Get basename. */
    if (executable_basename == NULL || ignore_cache) {
        executable_basename = strrchr(executable_path, '/');
        executable_basename =
            (executable_basename == NULL ? executable_path : executable_basename + 1);
    }
    return (full_path ? executable_path : executable_basename);
}

/* get application name, (cached), used for event logging */
char *
get_application_name(void)
{
    return get_application_name_helper(false, true /* full path */);
}

/* i#907: Called during early injection before data section protection to avoid
 * issues with /proc/self/exe.
 */
void
set_executable_path(const char *exe_path)
{
    strncpy(executable_path, exe_path, BUFFER_SIZE_ELEMENTS(executable_path));
    NULL_TERMINATE_BUFFER(executable_path);
    /* Re-compute the basename in case the full path changed. */
    get_application_name_helper(true /* re-compute */, false /* basename */);
}

/* Note: this is exported so that libdrpreload.so (preload.c) can use it to
 * get process names to do selective process following (PR 212034).  The
 * alternative is to duplicate or compile in this code into libdrpreload.so,
 * which is messy.  Besides, libdynamorio.so is already loaded into the process
 * and avaiable, so cleaner to just use functions from it.
 */
DYNAMORIO_EXPORT const char *
get_application_short_name(void)
{
    return get_application_name_helper(false, false /* short name */);
}

/* Sets pointers to the application's command-line arguments. These pointers are then used
 * by get_app_args().
 */
void
set_app_args(IN int *app_argc_in, IN char **app_argv_in)
{
    app_argc = app_argc_in;
    app_argv = app_argv_in;
}

/* Returns the number of application's command-line arguments. */
int
num_app_args()
{
    if (!DYNAMO_OPTION(early_inject)) {
        set_client_error_code(NULL, DR_ERROR_NOT_IMPLEMENTED);
        return -1;
    }

    return *app_argc;
}

/* Returns the application's command-line arguments. */
int
get_app_args(OUT dr_app_arg_t *args_array, int args_count)
{
    if (args_array == NULL || args_count < 0) {
        set_client_error_code(NULL, DR_ERROR_INVALID_PARAMETER);
        return -1;
    }

    if (!DYNAMO_OPTION(early_inject)) {
        set_client_error_code(NULL, DR_ERROR_NOT_IMPLEMENTED);
        return -1;
    }

    int num_args = num_app_args();
    int min = (args_count < num_args) ? args_count : num_args;
    for (int i = 0; i < min; i++) {
        args_array[i].start = (void *)app_argv[i];
        args_array[i].size = strlen(app_argv[i]) + 1 /* consider NULL byte */;
        args_array[i].encoding = DR_APP_ARG_CSTR_COMPAT;
    }
    return min;
}

/* Processor information provided by kernel */
#define PROC_CPUINFO "/proc/cpuinfo"
#define CPUMHZ_LINE_LENGTH 64
#define CPUMHZ_LINE_FORMAT "cpu MHz\t\t: %lu.%03lu\n"
/* printed in /usr/src/linux-2.4/arch/i386/kernel/setup.c calibrated in time.c */
/* seq_printf(m, "cpu MHz\t\t: %lu.%03lu\n", cpu_khz / 1000, (cpu_khz % 1000)) */
/* e.g. cpu MHz           : 1594.851 */
static timestamp_t
get_timer_frequency_cpuinfo(void)
{
    file_t cpuinfo;
    ssize_t nread;
    char *buf;
    char *mhz_line;
    ulong cpu_mhz = 1000;
    ulong cpu_khz = 0;

    cpuinfo = os_open(PROC_CPUINFO, OS_OPEN_READ);

    /* This can happen in a chroot or if /proc is disabled. */
    if (cpuinfo == INVALID_FILE)
        return 1000 * 1000; /* 1 GHz */

    /* cpu MHz is typically in the first 4096 bytes.  If not, or we get a short
     * or interrupted read, our timer frequency estimate will be off, but it's
     * not the end of the world.
     * FIXME: Factor a buffered file reader out of our maps iterator if we want
     * to do this the right way.
     */
    buf = global_heap_alloc(PAGE_SIZE HEAPACCT(ACCT_OTHER));
    nread = os_read(cpuinfo, buf, PAGE_SIZE - 1);
    if (nread > 0) {
        buf[nread] = '\0';
        mhz_line = strstr(buf, "cpu MHz\t\t:");
        if (mhz_line != NULL &&
            sscanf(mhz_line, CPUMHZ_LINE_FORMAT, &cpu_mhz, &cpu_khz) == 2) {
            LOG(GLOBAL, LOG_ALL, 2, "Processor speed exactly %lu.%03luMHz\n", cpu_mhz,
                cpu_khz);
        }
    }
    global_heap_free(buf, PAGE_SIZE HEAPACCT(ACCT_OTHER));
    os_close(cpuinfo);

    return cpu_mhz * 1000 + cpu_khz;
}

timestamp_t
get_timer_frequency()
{
#ifdef VMX86_SERVER
    if (os_in_vmkernel_userworld()) {
        return vmk_get_timer_frequency();
    }
#endif
    return get_timer_frequency_cpuinfo();
}

/* DR has standardized on UTC time which counts from since Jan 1, 1601.
 * That's the Windows standard.  But Linux uses the Epoch of Jan 1, 1970.
 */
#define UTC_TO_EPOCH_SECONDS 11644473600

/* seconds since 1601 */
uint
query_time_seconds(void)
{
    struct timeval current_time;
#if defined(MACOS) && defined(AARCH64)
    /* TODO i#5383: Replace with a system call (unless we're sure this library call
     * is re-entrant, just a simple load from commpage).
     */
#    undef gettimeofday /* Remove "gettimeofday_forbidden_function". */
    uint64 val = gettimeofday(&current_time, NULL);
#else
    uint64 val = dynamorio_syscall(SYS_gettimeofday, 2, &current_time, NULL);
#    ifdef MACOS
    /* MacOS before Sierra returns usecs:secs and does not set the timeval struct. */
    if (macos_version < MACOS_VERSION_SIERRA) {
        if ((int)val < 0)
            return 0;
        return (uint)val + UTC_TO_EPOCH_SECONDS;
    }
#    endif
#endif
    if ((int)val >= 0) {
        return current_time.tv_sec + UTC_TO_EPOCH_SECONDS;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
}

#if defined(MACOS) && defined(AARCH64)
#    include <sys/time.h>
#endif

/* milliseconds since 1601 */
uint64
query_time_millis()
{
    struct timeval current_time;
#if !(defined(MACOS) && defined(AARCH64))
    uint64 val = dynamorio_syscall(SYS_gettimeofday, 2, &current_time, NULL);
#else
    /* TODO i#5383: Replace with a system call (unless we're sure this library call
     * is re-entrant, just a simple load from commpage).
     */
#    undef gettimeofday /* Remove "gettimeofday_forbidden_function". */
    uint64 val = gettimeofday(&current_time, NULL);
#endif

#ifdef MACOS
    /* MacOS before Sierra returns usecs:secs and does not set the timeval struct. */
    if (macos_version < MACOS_VERSION_SIERRA) {
        if ((int)val > 0) {
            current_time.tv_sec = (uint)val;
            current_time.tv_usec = (uint)(val >> 32);
        }
    }
#endif
    if ((int)val >= 0) {
        uint64 res =
            (((uint64)current_time.tv_sec) * 1000) + (current_time.tv_usec / 1000);
        res += UTC_TO_EPOCH_SECONDS * 1000;
        return res;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
}

/* microseconds since 1601 */
uint64
query_time_micros()
{
    struct timeval current_time;
    uint64 val = dynamorio_syscall(SYS_gettimeofday, 2, &current_time, NULL);
#ifdef MACOS
    /* MacOS before Sierra returns usecs:secs and does not set the timeval struct. */
    if (macos_version < MACOS_VERSION_SIERRA) {
        if ((int)val > 0) {
            current_time.tv_sec = (uint)val;
            current_time.tv_usec = (uint)(val >> 32);
        }
    }
#endif
    if ((int)val >= 0) {
        uint64 res = (((uint64)current_time.tv_sec) * 1000000) + current_time.tv_usec;
        res += UTC_TO_EPOCH_SECONDS * 1000000;
        return res;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
}

#ifdef RETURN_AFTER_CALL
/* Finds the bottom of the call stack, presumably at program startup. */
/* This routine is a copycat of internal_dump_callstack and makes
   assumptions about program state, i.e. that frame pointers are valid
   and should be used only in well known points for release build.
*/
static app_pc
find_stack_bottom()
{
    app_pc retaddr = 0;
    int depth = 0;
    reg_t *fp;
    /* from dump_dr_callstack() */
    asm("mov  %%" ASM_XBP ", %0" : "=m"(fp));

    LOG(THREAD_GET, LOG_ALL, 3, "Find stack bottom:\n");
    while (fp != NULL && is_readable_without_exception((byte *)fp, sizeof(reg_t) * 2)) {
        retaddr = (app_pc) * (fp + 1); /* presumably also readable */
        LOG(THREAD_GET, LOG_ALL, 3,
            "\tframe ptr " PFX " => parent " PFX ", ret = " PFX "\n", fp, *fp, retaddr);
        depth++;
        /* yes I've seen weird recursive cases before */
        if (fp == (reg_t *)*fp || depth > 100)
            break;
        fp = (reg_t *)*fp;
    }
    return retaddr;
}
#endif /* RETURN_AFTER_CALL */

/* os-specific atexit cleanup */
void
os_slow_exit(void)
{
#ifdef MACOS64
    tls_process_exit();
#endif
#ifdef LINUX
    if (!standalone_library)
        d_r_rseq_exit();
#endif
    d_r_signal_exit();
    memquery_exit();
    ksynch_exit();

    generic_hash_destroy(GLOBAL_DCONTEXT, fd_table);
    fd_table = NULL;

    if (doing_detach) {
        vsyscall_page_start = NULL;
        ASSERT(num_fd_add_pre_heap == 0);
    }

    DELETE_LOCK(set_thread_area_lock);
    DELETE_LOCK(client_tls_lock);
    IF_NO_MEMQUERY(memcache_exit());
}

/* Helper function that calls cleanup_and_terminate after blocking most signals
 *(i#2921).
 */
void
block_cleanup_and_terminate(dcontext_t *dcontext, int sysnum, ptr_uint_t sys_arg1,
                            ptr_uint_t sys_arg2, bool exitproc,
                            /* these 2 args are only used for Mac thread exit */
                            ptr_uint_t sys_arg3, ptr_uint_t sys_arg4)
{
    /* This thread is on its way to exit. We are blocking all signals since any
     * signal that reaches us now can be delayed until after the exit is complete.
     * We may still receive a suspend signal for synchronization that we may need
     * to reply to (i#2921).
     */
    if (sysnum == SYS_kill)
        block_all_noncrash_signals_except(NULL, 2, dcontext->sys_param0, SUSPEND_SIGNAL);
    else
        block_all_noncrash_signals_except(NULL, 1, SUSPEND_SIGNAL);
    cleanup_and_terminate(dcontext, sysnum, sys_arg1, sys_arg2, exitproc, sys_arg3,
                          sys_arg4);
}

/* os-specific atexit cleanup */
void
os_fast_exit(void)
{
    /* nothing */
}

void
os_terminate_with_code(dcontext_t *dcontext, terminate_flags_t flags, int exit_code)
{
    /* i#1319: we support a signal via 2nd byte */
    bool use_signal = exit_code > 0x00ff;
    /* XXX: TERMINATE_THREAD not supported */
    ASSERT_NOT_IMPLEMENTED(TEST(TERMINATE_PROCESS, flags));
    if (use_signal) {
        int sig = (exit_code & 0xff00) >> 8;
        os_terminate_via_signal(dcontext, flags, sig);
        ASSERT_NOT_REACHED();
    }
    if (TEST(TERMINATE_CLEANUP, flags)) {
        /* we enter from several different places, so rewind until top-level kstat */
        KSTOP_REWIND_UNTIL(thread_measured);
        block_cleanup_and_terminate(dcontext, SYSNUM_EXIT_PROCESS, exit_code, 0,
                                    true /*whole process*/, 0, 0);
    } else {
        /* clean up may be impossible - just terminate */
        d_r_config_exit(); /* delete .1config file */
        exit_process_syscall(exit_code);
    }
}

void
os_terminate(dcontext_t *dcontext, terminate_flags_t flags)
{
    os_terminate_with_code(dcontext, flags, -1);
}

int
os_timeout(int time_in_milliseconds)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

/************************************************************************
 * SEGMENT STEALING
 *
 * Not easy to make truly transparent -- but the alternative of dispatch
 * by thread id on global memory has performance implications.
 * Pull the non-STEAL_SEGMENT code out of the cvs attic for a base if
 * transparency becomes more of a problem.
 */

#define TLS_LOCAL_STATE_OFFSET (offsetof(os_local_state_t, state))

/* offset from top of page */
#define TLS_OS_LOCAL_STATE 0x00

#define TLS_SELF_OFFSET (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, self))
#define TLS_THREAD_ID_OFFSET (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, tid))
#define TLS_DCONTEXT_OFFSET (TLS_OS_LOCAL_STATE + TLS_DCONTEXT_SLOT)
#ifdef X86
#    define TLS_MAGIC_OFFSET (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, magic))
#endif

/* they should be used with os_tls_offset, so do not need add TLS_OS_LOCAL_STATE here
 */
#define TLS_APP_LIB_TLS_BASE_OFFSET (offsetof(os_local_state_t, app_lib_tls_base))
#define TLS_APP_ALT_TLS_BASE_OFFSET (offsetof(os_local_state_t, app_alt_tls_base))
#define TLS_APP_LIB_TLS_REG_OFFSET (offsetof(os_local_state_t, app_lib_tls_reg))
#define TLS_APP_ALT_TLS_REG_OFFSET (offsetof(os_local_state_t, app_alt_tls_reg))

/* N.B.: imm and offs are ushorts!
 * We use %c[0-9] to get gcc to emit an integer constant without a leading $ for
 * the segment offset.  See the documentation here:
 * http://gcc.gnu.org/onlinedocs/gccint/Output-Template.html#Output-Template
 * Also, var needs to match the pointer size, or else we'll get stack corruption.
 * XXX: This is marked volatile prevent gcc from speculating this code before
 * checks for is_thread_tls_initialized(), but if we could find a more
 * precise constraint, then the compiler would be able to optimize better.  See
 * glibc comments on THREAD_SELF.
 */
#ifdef DR_HOST_NOT_TARGET
#    define WRITE_TLS_SLOT_IMM(imm, var) var = 0, ASSERT_NOT_REACHED()
#    define READ_TLS_SLOT_IMM(imm, var) var = 0, ASSERT_NOT_REACHED()
#    define WRITE_TLS_INT_SLOT_IMM(imm, var) var = 0, ASSERT_NOT_REACHED()
#    define READ_TLS_INT_SLOT_IMM(imm, var) var = 0, ASSERT_NOT_REACHED()
#    define WRITE_TLS_SLOT(offs, var) offs = var ? 0 : 1, ASSERT_NOT_REACHED()
#    define READ_TLS_SLOT(offs, var) var = (void *)(ptr_uint_t)offs, ASSERT_NOT_REACHED()
#elif defined(MACOS64) && !defined(AARCH64)
/* For now we have both a directly-addressable os_local_state_t and a pointer to
 * it in slot 6.  If we settle on always doing the full os_local_state_t in slots,
 * we would probably get rid of the indirection here and directly access slot fields.
 */
#    define WRITE_TLS_SLOT_IMM(imm, var)                                             \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                                       \
        ASSERT(sizeof(var) == sizeof(void *));                                       \
        __asm__ __volatile__(                                                        \
            "mov %%gs:%1, %%" ASM_XAX " \n\t"                                        \
            "movq %0, %c2(%%" ASM_XAX ") \n\t"                                       \
            :                                                                        \
            : "r"(var), "m"(*(void **)(DR_TLS_BASE_SLOT * sizeof(void *))), "i"(imm) \
            : "memory", ASM_XAX);

#    define READ_TLS_SLOT_IMM(imm, var)                                            \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                                     \
        ASSERT(sizeof(var) == sizeof(void *));                                     \
        __asm__ __volatile__("mov %%gs:%1, %%" ASM_XAX " \n\t"                     \
                             "movq %c2(%%" ASM_XAX "), %0 \n\t"                    \
                             : "=r"(var)                                           \
                             : "m"(*(void **)(DR_TLS_BASE_SLOT * sizeof(void *))), \
                               "i"(imm)                                            \
                             : ASM_XAX);

#    define WRITE_TLS_SLOT(offs, var)                                              \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                                     \
        __asm__ __volatile__("mov %%gs:%0, %%" ASM_XAX " \n\t"                     \
                             "movzwq %1, %%" ASM_XDX " \n\t"                       \
                             "movq %2, (%%" ASM_XAX ", %%" ASM_XDX ") \n\t"        \
                             :                                                     \
                             : "m"(*(void **)(DR_TLS_BASE_SLOT * sizeof(void *))), \
                               "m"(offs), "r"(var)                                 \
                             : "memory", ASM_XAX, ASM_XDX);

#    define READ_TLS_SLOT(offs, var)                                               \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                                     \
        ASSERT(sizeof(var) == sizeof(void *));                                     \
        __asm__ __volatile__("mov %%gs:%1, %%" ASM_XAX " \n\t"                     \
                             "movzwq %2, %%" ASM_XDX " \n\t"                       \
                             "movq (%%" ASM_XAX ", %%" ASM_XDX "), %0 \n\t"        \
                             : "=r"(var)                                           \
                             : "m"(*(void **)(DR_TLS_BASE_SLOT * sizeof(void *))), \
                               "m"(offs)                                           \
                             : "memory", ASM_XAX, ASM_XDX);

#elif defined(X86)
#    define WRITE_TLS_SLOT_IMM(imm, var)       \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED()); \
        ASSERT(sizeof(var) == sizeof(void *)); \
        asm volatile("mov %0, %" ASM_SEG ":%c1" : : "r"(var), "i"(imm));

#    define READ_TLS_SLOT_IMM(imm, var)        \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED()); \
        ASSERT(sizeof(var) == sizeof(void *)); \
        asm volatile("mov %" ASM_SEG ":%c1, %0" : "=r"(var) : "i"(imm));

#    define WRITE_TLS_INT_SLOT_IMM(imm, var)   \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED()); \
        ASSERT(sizeof(var) == sizeof(int));    \
        asm volatile("movl %0, %" ASM_SEG ":%c1" : : "r"(var), "i"(imm));

#    define READ_TLS_INT_SLOT_IMM(imm, var)    \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED()); \
        ASSERT(sizeof(var) == sizeof(int));    \
        asm volatile("movl %" ASM_SEG ":%c1, %0" : "=r"(var) : "i"(imm));

/* FIXME: need dedicated-storage var for _TLS_SLOT macros, can't use expr */
#    define WRITE_TLS_SLOT(offs, var)                                                   \
        IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                                          \
        ASSERT(sizeof(var) == sizeof(void *));                                          \
        ASSERT(sizeof(offs) == 2);                                                      \
        asm("mov %0, %%" ASM_XAX : : "m"((var)) : ASM_XAX);                             \
        asm("movzw" IF_X64_ELSE("q", "l") " %0, %%" ASM_XDX : : "m"((offs)) : ASM_XDX); \
        asm("mov %%" ASM_XAX ", %" ASM_SEG ":(%%" ASM_XDX ")" : : : ASM_XAX, ASM_XDX);

#    define READ_TLS_SLOT(offs, var)                                                    \
        ASSERT(sizeof(var) == sizeof(void *));                                          \
        ASSERT(sizeof(offs) == 2);                                                      \
        asm("movzw" IF_X64_ELSE("q", "l") " %0, %%" ASM_XAX : : "m"((offs)) : ASM_XAX); \
        asm("mov %" ASM_SEG ":(%%" ASM_XAX "), %%" ASM_XAX : : : ASM_XAX);              \
        asm("mov %%" ASM_XAX ", %0" : "=m"((var)) : : ASM_XAX);
#elif defined(AARCHXX) && !defined(MACOS)
/* Android needs indirection through a global.  The Android toolchain has
 * trouble with relocations if we use a global directly in asm, so we convert to
 * a local variable in these macros.  We pay the cost of the extra instructions
 * for Linux ARM to share the code.
 */
#    define WRITE_TLS_SLOT_IMM(imm, var)                                            \
        do {                                                                        \
            uint _base_offs = DR_TLS_BASE_OFFSET;                                   \
            __asm__ __volatile__("mov " ASM_R2 ", %0 \n\t" READ_TP_TO_R3_DISP_IN_R2 \
                                 "str %1, [" ASM_R3 ", %2] \n\t"                    \
                                 :                                                  \
                                 : "r"(_base_offs), "r"(var), "i"(imm)              \
                                 : "memory", ASM_R2, ASM_R3);                       \
        } while (0)
#    define READ_TLS_SLOT_IMM(imm, var)                                             \
        do {                                                                        \
            uint _base_offs = DR_TLS_BASE_OFFSET;                                   \
            __asm__ __volatile__("mov " ASM_R2 ", %1 \n\t" READ_TP_TO_R3_DISP_IN_R2 \
                                 "ldr %0, [" ASM_R3 ", %2] \n\t"                    \
                                 : "=r"(var)                                        \
                                 : "r"(_base_offs), "i"(imm)                        \
                                 : ASM_R2, ASM_R3);                                 \
        } while (0)
#    define WRITE_TLS_INT_SLOT_IMM WRITE_TLS_SLOT_IMM /* b/c 32-bit */
#    define READ_TLS_INT_SLOT_IMM READ_TLS_SLOT_IMM   /* b/c 32-bit */
#    define WRITE_TLS_SLOT(offs, var)                                               \
        do {                                                                        \
            uint _base_offs = DR_TLS_BASE_OFFSET;                                   \
            __asm__ __volatile__("mov " ASM_R2 ", %0 \n\t" READ_TP_TO_R3_DISP_IN_R2 \
                                 "add " ASM_R3 ", " ASM_R3 ", %2 \n\t"              \
                                 "str %1, [" ASM_R3 "]   \n\t"                      \
                                 :                                                  \
                                 : "r"(_base_offs), "r"(var), "r"(offs)             \
                                 : "memory", ASM_R2, ASM_R3);                       \
        } while (0)
#    define READ_TLS_SLOT(offs, var)                                                \
        do {                                                                        \
            uint _base_offs = DR_TLS_BASE_OFFSET;                                   \
            __asm__ __volatile__("mov " ASM_R2 ", %1 \n\t" READ_TP_TO_R3_DISP_IN_R2 \
                                 "add " ASM_R3 ", " ASM_R3 ", %2 \n\t"              \
                                 "ldr %0, [" ASM_R3 "]   \n\t"                      \
                                 : "=r"(var)                                        \
                                 : "r"(_base_offs), "r"(offs)                       \
                                 : ASM_R2, ASM_R3);                                 \
        } while (0)
#elif defined(AARCH64) && defined(MACOS)

#    define WRITE_TLS_SLOT_IMM(imm, var) WRITE_TLS_SLOT(imm, var)
#    define READ_TLS_SLOT_IMM(imm, var) READ_TLS_SLOT(imm, var)
#    define WRITE_TLS_INT_SLOT_IMM(imm, var) WRITE_TLS_SLOT(imm, var)
#    define READ_TLS_INT_SLOT_IMM(imm, var) READ_TLS_SLOT(imm, var)
#    define WRITE_TLS_SLOT(offs, var) \
        *((__typeof__(var) *)(tls_get_dr_addr() + offs)) = var;
#    define READ_TLS_SLOT(offs, var) \
        var = *((__typeof__(var) *)(tls_get_dr_addr() + offs));

#elif defined(RISCV64)
#    define WRITE_TLS_SLOT_IMM(imm, var)                                       \
        do {                                                                   \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                             \
            ASSERT(sizeof(var) == sizeof(void *));                             \
            __asm__ __volatile__("ld t0, %0(tp) \n\t"                          \
                                 "sd %1, %2(t0) \n\t"                          \
                                 :                                             \
                                 : "i"(DR_TLS_BASE_OFFSET), "r"(var), "i"(imm) \
                                 : "memory", "t0");                            \
        } while (0)
#    define READ_TLS_SLOT_IMM(imm, var)                                \
        do {                                                           \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                     \
            ASSERT(sizeof(var) == sizeof(void *));                     \
            __asm__ __volatile__("ld %0, %1(tp) \n\t"                  \
                                 "ld %0, %2(%0) \n\t"                  \
                                 : "+r"(var)                           \
                                 : "i"(DR_TLS_BASE_OFFSET), "i"(imm)); \
        } while (0)
#    define WRITE_TLS_INT_SLOT_IMM(imm, var)                                   \
        do {                                                                   \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                             \
            ASSERT(sizeof(var) == sizeof(void *));                             \
            __asm__ __volatile__("lw t0, %0(tp) \n\t"                          \
                                 "sw %1, %2(t0) \n\t"                          \
                                 :                                             \
                                 : "i"(DR_TLS_BASE_OFFSET), "r"(var), "i"(imm) \
                                 : "memory", "t0");                            \
        } while (0)
#    define READ_TLS_INT_SLOT_IMM(imm, var)                            \
        do {                                                           \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                     \
            ASSERT(sizeof(var) == sizeof(void *));                     \
            __asm__ __volatile__("lw %0, %1(tp) \n\t"                  \
                                 "lw %0, %2(%0) \n\t"                  \
                                 : "=r"(var)                           \
                                 : "i"(DR_TLS_BASE_OFFSET), "i"(imm)); \
        } while (0)
#    define WRITE_TLS_SLOT(offs, var)                                           \
        do {                                                                    \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                              \
            ASSERT(sizeof(var) == sizeof(void *));                              \
            __asm__ __volatile__("ld t0, %0(tp) \n\t"                           \
                                 "add t0, t0, %2\n\t"                           \
                                 "sd %1, 0(t0) \n\t"                            \
                                 :                                              \
                                 : "i"(DR_TLS_BASE_OFFSET), "r"(var), "r"(offs) \
                                 : "memory", "t0");                             \
        } while (0)
#    define READ_TLS_SLOT(offs, var)                                    \
        do {                                                            \
            IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                      \
            ASSERT(sizeof(var) == sizeof(void *));                      \
            __asm__ __volatile__("ld %0, %1(tp) \n\t"                   \
                                 "add %0, %0, %2\n\t"                   \
                                 "ld %0, 0(%0) \n\t"                    \
                                 : "=r"(var)                            \
                                 : "i"(DR_TLS_BASE_OFFSET), "r"(offs)); \
        } while (0)
#endif /* X86/ARM/RISCV64 */

#ifdef X86
/* We use this at thread init and exit to make it easy to identify
 * whether TLS is initialized (i#2089).
 * We assume alignment does not matter.
 */
static os_local_state_t uninit_tls; /* has .magic == 0 */
#endif

static bool
is_thread_tls_initialized(void)
{
#if defined(MACOS) && defined(AARCH64)
    os_local_state_t *v = (void *)tls_get_dr_addr();
    return v != NULL && v->tls_type == TLS_TYPE_SLOT;
#elif defined(MACOS64) && !defined(AARCH64)
    /* For now we have both a directly-addressable os_local_state_t and a pointer to
     * it in slot 6.  If we settle on always doing the full os_local_state_t in slots,
     * we would probably get rid of the indirection here and directly read the magic
     * field from its slot.
     */
    byte **tls_swap_slot;
    tls_swap_slot = (byte **)get_app_tls_swap_slot_addr();
    if (tls_swap_slot == NULL || *tls_swap_slot == NULL ||
        *tls_swap_slot == TLS_SLOT_VAL_EXITED)
        return false;
    return true;
#elif defined(X86)
    if (INTERNAL_OPTION(safe_read_tls_init)) {
        /* Avoid faults during early init or during exit when we have no handler.
         * It's not worth extending the handler as the faults are a perf hit anyway.
         * For standalone_library, first_thread_tls_initialized will always be false,
         * so we'll return false here and use our check in get_thread_private_dcontext().
         */
        if (!first_thread_tls_initialized || last_thread_tls_exited)
            return false;
        /* i#3535: Avoid races between removing DR's SIGSEGV signal handler and
         * detached threads being passed native signals.  The detaching thread is
         * the one doing all the real cleanup, so we simply avoid any safe reads
         * or TLS for detaching threads.  This var is not cleared until re-init,
         * so we have no race with the end of detach.
         */
        if (detacher_tid != INVALID_THREAD_ID && detacher_tid != get_sys_thread_id())
            return false;
        /* To handle WSL (i#1986) where fs and gs start out equal to ss (0x2b),
         * and when the MSR is used having a zero selector, and other complexities,
         * we just do a blind safe read as the simplest solution once we're past
         * initial init and have a fault handler.
         *
         * i#2089: to avoid the perf cost of syscalls to verify the tid, and to
         * distinguish a fork child from a separate-group thread, we no longer read
         * the tid field and check that the TLS belongs to this particular thread:
         * instead we rely on clearing the .magic field for child threads and at
         * thread exit (to avoid a fault) and we simply check the field here.
         * A native app thread is very unlikely to match this.
         */
        return safe_read_tls_magic() == TLS_MAGIC_VALID;
    } else {
        /* XXX i#2089: we're keeping this legacy code around until
         * we're confident that the safe read code above is safer, more
         * performant, and more robust.
         */
        os_local_state_t *os_tls = NULL;
        ptr_uint_t cur_seg = read_thread_register(SEG_TLS);
        /* Handle WSL (i#1986) where fs and gs start out equal to ss (0x2b) */
        if (cur_seg != 0 && cur_seg != read_thread_register(SEG_SS)) {
            /* XXX: make this a safe read: but w/o dcontext we need special asm support */
            READ_TLS_SLOT_IMM(TLS_SELF_OFFSET, os_tls);
        }
#    if defined(X64) && defined(X86)
        if (os_tls == NULL && tls_dr_using_msr()) {
            /* When the MSR is used, the selector in the register remains 0.
             * We can't clear the MSR early in a new thread and then look for
             * a zero base here b/c if kernel decides to use GDT that zeroing
             * will set the selector, unless we want to assume we know when
             * the kernel uses the GDT.
             * Instead we make a syscall to get the tid.  This should be ok
             * perf-wise b/c the common case is the non-zero above.
             */
            byte *base = tls_get_fs_gs_segment_base(SEG_TLS);
            ASSERT(tls_global_type == TLS_TYPE_ARCH_PRCTL);
            if (base != (byte *)POINTER_MAX && base != NULL) {
                os_tls = (os_local_state_t *)base;
            }
        }
#    endif
        if (os_tls != NULL) {
            return (os_tls->tid == get_sys_thread_id() ||
                    /* The child of a fork will initially come here */
                    os_tls->state.spill_space.dcontext->owning_process ==
                        get_parent_id());
        } else
            return false;
    }
#elif defined(AARCHXX) || defined(RISCV64)
    byte **dr_tls_base_addr;
    if (tls_global_type == TLS_TYPE_NONE)
        return false;
    dr_tls_base_addr = (byte **)get_dr_tls_base_addr();
    if (dr_tls_base_addr == NULL || *dr_tls_base_addr == NULL ||
        /* We use the TLS slot's value to identify a now-exited thread (i#1578) */
        *dr_tls_base_addr == TLS_SLOT_VAL_EXITED)
        return false;
    /* We would like to ASSERT is_dynamo_address(*tls_swap_slot) but that leads
     * to infinite recursion for an address not in the vm_reserve area, as
     * dynamo_vm_areas_start_reading() ending up calling
     * deadlock_avoidance_unlock() which calls get_thread_private_dcontext()
     * which comes here.
     */
    return true;
#endif
    return true;
}

bool
is_DR_segment_reader_entry(app_pc pc)
{
    /* This routine is used to avoid problems with dr_prepopulate_cache() building
     * bbs for DR code that reads DR segments when DR is a static library.
     * It's a little ugly but it's not clear there's a better solution.
     * See the discussion in i#2463 c#2.
     */
#ifdef X86
    if (INTERNAL_OPTION(safe_read_tls_init)) {
        return pc == (app_pc)safe_read_tls_magic || pc == (app_pc)safe_read_tls_self;
    }
#endif
    /* XXX i#2463: for ARM and for -no_safe_read_tls_init it may be
     * more complicated as the PC may not be a function entry but the
     * start of a bb after a branch in our C code that uses inline asm
     * to read the TLS.
     */
    return false;
}

#if defined(X86) || defined(DEBUG)
static bool
is_thread_tls_allocated(void)
{
#    if defined(X86) && !defined(MACOS64)
    if (INTERNAL_OPTION(safe_read_tls_init)) {
        /* We use this routine to allow currently-native threads, for which
         * is_thread_tls_initialized() (and thus is_thread_initialized()) will
         * return false.
         * Caution: this will also return true on a fresh clone child.
         */
        uint magic;
        if (!first_thread_tls_initialized || last_thread_tls_exited)
            return false;
        magic = safe_read_tls_magic();
        return magic == TLS_MAGIC_VALID || magic == TLS_MAGIC_INVALID;
    }
#    endif
    return is_thread_tls_initialized();
}
#endif

/* converts a local_state_t offset to a segment offset */
ushort
os_tls_offset(ushort tls_offs)
{
    /* no ushort truncation issues b/c TLS_LOCAL_STATE_OFFSET is 0 */
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    return (TLS_LOCAL_STATE_OFFSET + tls_offs IF_X86(IF_MACOS64(+tls_get_dr_offs())));
}

/* converts a segment offset to a local_state_t offset */
ushort
os_local_state_offset(ushort seg_offs)
{
    /* no ushort truncation issues b/c TLS_LOCAL_STATE_OFFSET is 0 */
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    return (seg_offs - TLS_LOCAL_STATE_OFFSET IF_MACOS64(-tls_get_dr_offs()));
}

/* XXX: Will return NULL if called before os_thread_init(), which sets
 * ostd->dr_fs/gs_base.
 */
void *
os_get_priv_tls_base(dcontext_t *dcontext, reg_id_t reg)
{
    os_thread_data_t *ostd;

    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(reg == TLS_REG_ALT || reg == TLS_REG_LIB);

    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return NULL;
    ostd = (os_thread_data_t *)dcontext->os_field;
    if (reg == TLS_REG_LIB)
        return ostd->priv_lib_tls_base;
    else if (reg == TLS_REG_ALT)
        return ostd->priv_alt_tls_base;
    ASSERT_NOT_REACHED();
    return NULL;
}

os_local_state_t *
get_os_tls(void)
{
    os_local_state_t *os_tls = NULL;
    ASSERT(is_thread_tls_initialized());
    READ_TLS_SLOT_IMM(TLS_SELF_OFFSET, os_tls);
    return os_tls;
}

/* Obtain TLS from dcontext directly, which succeeds in pre-thread-init
 * situations where get_os_tls() fails.
 */
static os_local_state_t *
get_os_tls_from_dc(dcontext_t *dcontext)
{
    byte *local_state;
    ASSERT(dcontext != NULL);
    local_state = (byte *)dcontext->local_state;
    if (local_state == NULL)
        return NULL;
    return (os_local_state_t *)(local_state - offsetof(os_local_state_t, state));
}

#ifdef AARCHXX
bool
os_set_app_tls_base(dcontext_t *dcontext, reg_id_t reg, void *base)
{
    os_local_state_t *os_tls;
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(reg == TLS_REG_LIB || reg == TLS_REG_ALT);
    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    /* we will be called only if TLS is initialized */
    ASSERT(dcontext != NULL);
    os_tls = get_os_tls_from_dc(dcontext);
    if (reg == TLS_REG_LIB) {
        os_tls->app_lib_tls_base = base;
        LOG(THREAD, LOG_THREADS, 1, "TLS app lib base  =" PFX "\n", base);
        return true;
    } else if (reg == TLS_REG_ALT) {
        os_tls->app_alt_tls_base = base;
        LOG(THREAD, LOG_THREADS, 1, "TLS app alt base  =" PFX "\n", base);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void *
os_get_app_tls_base(dcontext_t *dcontext, reg_id_t reg)
{
    os_local_state_t *os_tls;
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(reg == TLS_REG_LIB || reg == TLS_REG_ALT);
    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL) {
        /* No dcontext means we haven't initialized TLS, so we haven't replaced
         * the app's segments.  get_segment_base is expensive, but this should
         * be rare.  Re-examine if it pops up in a profile.
         */
        return get_segment_base(reg);
    }
    os_tls = get_os_tls_from_dc(dcontext);
    if (reg == TLS_REG_LIB)
        return os_tls->app_lib_tls_base;
    else if (reg == TLS_REG_ALT)
        return os_tls->app_alt_tls_base;
    ASSERT_NOT_REACHED();
    return NULL;
}

ushort
os_get_app_tls_base_offset(reg_id_t reg)
{
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    if (reg == TLS_REG_LIB)
        return TLS_APP_LIB_TLS_BASE_OFFSET;
    else if (reg == TLS_REG_ALT)
        return TLS_APP_ALT_TLS_BASE_OFFSET;
    ASSERT_NOT_REACHED();
    return 0;
}

#ifdef X86
ushort
os_get_app_tls_reg_offset(reg_id_t reg)
{
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    if (reg == TLS_REG_LIB)
        return TLS_APP_LIB_TLS_REG_OFFSET;
    else if (reg == TLS_REG_ALT)
        return TLS_APP_ALT_TLS_REG_OFFSET;
    ASSERT_NOT_REACHED();
    return 0;
}
#endif

void *
d_r_get_tls(ushort tls_offs)
{
    void *val;
    READ_TLS_SLOT(tls_offs, val);
    return val;
}

void
d_r_set_tls(ushort tls_offs, void *value)
{
    WRITE_TLS_SLOT(tls_offs, value);
}

/* Returns POINTER_MAX on failure.
 * Assumes that cs, ss, ds, and es are flat.
 * Should we export this to clients?  For now they can get
 * this information via opnd_compute_address().
 */
byte *
get_segment_base(uint seg)
{
#if defined(MACOS64) && defined(X86)
    ptr_uint_t *pthread_self = (ptr_uint_t *)read_thread_register(seg);
    return (byte *)&pthread_self[SEG_TLS_BASE_SLOT];
#elif defined(X86)
    if (seg == SEG_CS || seg == SEG_SS || seg == SEG_DS || seg == SEG_ES)
        return NULL;
#    ifdef HAVE_TLS
    return tls_get_fs_gs_segment_base(seg);
#    else
    return (byte *)POINTER_MAX;
#    endif /* HAVE_TLS */
#elif defined(AARCHXX) || defined(RISCV64)
    /* XXX i#1551: should we rename/refactor to avoid "segment"? */
    return (byte *)read_thread_register(seg);
#endif
}

/* i#572: handle opnd_compute_address to return the application
 * segment base value.
 */
byte *
get_app_segment_base(uint seg)
{
#ifdef X86
    if (seg == SEG_CS || seg == SEG_SS || seg == SEG_DS || seg == SEG_ES)
        return NULL;
#endif /* X86 */
    if (INTERNAL_OPTION(private_loader) && first_thread_tls_initialized &&
        !last_thread_tls_exited) {
        return d_r_get_tls(os_get_app_tls_base_offset(seg));
    }
    return get_segment_base(seg);
}

local_state_extended_t *
get_local_state_extended()
{
    os_local_state_t *os_tls = NULL;
    ASSERT(is_thread_tls_initialized());
    READ_TLS_SLOT_IMM(TLS_SELF_OFFSET, os_tls);
    return &(os_tls->state);
}

local_state_t *
get_local_state()
{
#ifdef HAVE_TLS
    return (local_state_t *)get_local_state_extended();
#else
    return NULL;
#endif
}

#ifdef DEBUG
void
os_enter_dynamorio(void)
{
#    if (defined(AARCHXX) || defined(RISCV64)) && !defined(MACOS)
    /* i#1578: check that app's tls value doesn't match our sentinel */
    ASSERT(*(byte **)get_dr_tls_base_addr() != TLS_SLOT_VAL_EXITED);
#    endif
}
#endif

/* i#107: handle segment register usage conflicts between app and dr:
 * os_handle_mov_seg updates the app's tls selector maintained by DR.
 * It is called before entering code cache in dispatch_enter_fcache.
 */
void
os_handle_mov_seg(dcontext_t *dcontext, byte *pc)
{
#ifdef X86
    instr_t instr;
    opnd_t opnd;
    reg_id_t seg;
    ushort sel = 0;
    our_modify_ldt_t *desc;
    int desc_idx;
    os_local_state_t *os_tls;
    os_thread_data_t *ostd;

    instr_init(dcontext, &instr);
    decode_cti(dcontext, pc, &instr);
    /* the first instr must be mov seg */
    ASSERT(instr_get_opcode(&instr) == OP_mov_seg);
    opnd = instr_get_dst(&instr, 0);
    ASSERT(opnd_is_reg(opnd));
    seg = opnd_get_reg(opnd);
    ASSERT(reg_is_segment(seg));

    ostd = (os_thread_data_t *)dcontext->os_field;
    desc = (our_modify_ldt_t *)ostd->app_thread_areas;
    os_tls = get_os_tls();

    /* get the selector value */
    opnd = instr_get_src(&instr, 0);
    if (opnd_is_reg(opnd)) {
        sel = (ushort)reg_get_value_priv(opnd_get_reg(opnd), get_mcontext(dcontext));
    } else {
        void *ptr;
        ptr = (ushort *)opnd_compute_address_priv(opnd, get_mcontext(dcontext));
        ASSERT(ptr != NULL);
        if (!d_r_safe_read(ptr, sizeof(sel), &sel)) {
            /* FIXME: if invalid address, should deliver a signal to user. */
            ASSERT_NOT_IMPLEMENTED(false);
        }
    }
    /* calculate the entry_number */
    desc_idx = SELECTOR_INDEX(sel) - tls_min_index();
    if (seg == TLS_REG_LIB) {
        os_tls->app_lib_tls_reg = sel;
        os_tls->app_lib_tls_base = (void *)(ptr_uint_t)desc[desc_idx].base_addr;
    } else {
        os_tls->app_alt_tls_reg = sel;
        os_tls->app_alt_tls_base = (void *)(ptr_uint_t)desc[desc_idx].base_addr;
    }
    instr_free(dcontext, &instr);
    LOG(THREAD_GET, LOG_THREADS, 2,
        "thread " TIDFMT " segment change %s to selector 0x%x => "
        "app lib tls base: " PFX ", alt tls base: " PFX "\n",
        d_r_get_thread_id(), reg_names[seg], sel, os_tls->app_lib_tls_base,
        os_tls->app_alt_tls_base);
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
#endif /* X86/ARM */
}

/* Initialization for TLS mangling (-mangle_app_seg on x86).
 * Must be called before DR setup its own segment.
 */
static void
os_tls_app_seg_init(os_local_state_t *os_tls, void *segment)
{
    app_pc app_lib_tls_base, app_alt_tls_base;
#if defined(X86) && !defined(MACOS64)
    int i, index;
    our_modify_ldt_t *desc;

    os_tls->app_lib_tls_reg = read_thread_register(TLS_REG_LIB);
    os_tls->app_alt_tls_reg = read_thread_register(TLS_REG_ALT);
#endif
    app_lib_tls_base = get_segment_base(TLS_REG_LIB);
    app_alt_tls_base = get_segment_base(TLS_REG_ALT);

    /* If we're a non-initial thread, tls will be set to the parent's value,
     * or to &uninit_tls (i#2089), both of which will be is_dynamo_address().
     */
    os_tls->app_lib_tls_base =
        is_dynamo_address(app_lib_tls_base) ? NULL : app_lib_tls_base;
    os_tls->app_alt_tls_base =
        is_dynamo_address(app_alt_tls_base) ? NULL : app_alt_tls_base;

#if defined(X86) && !defined(MACOS64)
    /* get all TLS thread area value */
    /* XXX: is get_thread_area supported in 64-bit kernel?
     * It has syscall number 211.
     * It works for a 32-bit application running in a 64-bit kernel.
     * It returns error value -38 for a 64-bit app in a 64-bit kernel.
     */
    desc = &os_tls->os_seg_info.app_thread_areas[0];
    tls_initialize_indices(os_tls);
    index = tls_min_index();
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        tls_get_descriptor(i + index, &desc[i]);
    }
#endif /* X86 */
    os_tls->os_seg_info.dr_tls_base = segment;
    os_tls->os_seg_info.priv_alt_tls_base = IF_X86_ELSE(segment, NULL);

    /* now allocate the tls segment for client libraries */
    if (INTERNAL_OPTION(private_loader)) {
        os_tls->os_seg_info.priv_lib_tls_base = IF_UNIT_TEST_ELSE(
            os_tls->app_lib_tls_base, privload_tls_init(os_tls->app_lib_tls_base));
    }
#if defined(X86) && !defined(MACOSX64)
    LOG(THREAD_GET, LOG_THREADS, 1,
        "thread " TIDFMT " app lib tls reg: 0x%x, alt tls reg: 0x%x\n",
        d_r_get_thread_id(), os_tls->app_lib_tls_reg, os_tls->app_alt_tls_reg);
#endif
    LOG(THREAD_GET, LOG_THREADS, 1,
        "thread " TIDFMT " app lib tls base: " PFX ", alt tls base: " PFX "\n",
        d_r_get_thread_id(), os_tls->app_lib_tls_base, os_tls->app_alt_tls_base);
    LOG(THREAD_GET, LOG_THREADS, 1,
        "thread " TIDFMT " priv lib tls base: " PFX ", alt tls base: " PFX ", "
        "DR's tls base: " PFX "\n",
        d_r_get_thread_id(), os_tls->os_seg_info.priv_lib_tls_base,
        os_tls->os_seg_info.priv_alt_tls_base, os_tls->os_seg_info.dr_tls_base);
}

void
os_tls_init(void)
{
#ifdef X86
    ASSERT(TLS_MAGIC_OFFSET_ASM == TLS_MAGIC_OFFSET);
    ASSERT(TLS_SELF_OFFSET_ASM == TLS_SELF_OFFSET);
#endif
#ifdef HAVE_TLS
    /* We create a 1-page segment with an LDT entry for each thread and load its
     * selector into fs/gs.
     * FIXME PR 205276: this whole scheme currently does not check if app is using
     * segments need to watch modify_ldt syscall
     */
#    ifdef MACOS64
    /* Today we're allocating enough contiguous TLS slots to hold os_local_state_t.
     * We also store a pointer to it in TLS slot 6.
     */
    byte *segment = tls_get_dr_addr();
#    else
    byte *segment = heap_mmap(PAGE_SIZE, MEMPROT_READ | MEMPROT_WRITE,
                              VMM_SPECIAL_MMAP | VMM_PER_THREAD);
#    endif
    os_local_state_t *os_tls = (os_local_state_t *)segment;

    LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init for thread " TIDFMT "\n",
        d_r_get_thread_id());
    ASSERT(!is_thread_tls_initialized());

    /* MUST zero out dcontext slot so uninit access gets NULL */
    memset(segment, 0, IF_MACOSA64_ELSE(sizeof(os_local_state_t), PAGE_SIZE));
    /* store key data in the tls itself */
    os_tls->self = os_tls;
    os_tls->tid = get_sys_thread_id();
    os_tls->tls_type = TLS_TYPE_NONE;
#    ifdef X86
    os_tls->magic = TLS_MAGIC_VALID;
#    endif
    /* We save DR's TLS segment base here so that os_get_dr_tls_base() will work
     * even when -no_mangle_app_seg is set.  If -mangle_app_seg is set, this
     * will be overwritten in os_tls_app_seg_init().
     */
    os_tls->os_seg_info.dr_tls_base = segment;
    ASSERT(proc_is_cache_aligned(os_tls->self + TLS_LOCAL_STATE_OFFSET));
    /* Verify that local_state_extended_t should indeed be used. */
    ASSERT(DYNAMO_OPTION(ibl_table_in_tls));

    /* initialize DR TLS seg base before replacing app's TLS in tls_thread_init */
    if (MACHINE_TLS_IS_DR_TLS)
        os_tls_app_seg_init(os_tls, segment);

    tls_thread_init(os_tls, segment);
    ASSERT(os_tls->tls_type != TLS_TYPE_NONE);
    /* store type in global var for convenience: should be same for all threads */
    tls_global_type = os_tls->tls_type;

    /* FIXME: this should be a SYSLOG fatal error?  Should fall back on !HAVE_TLS?
     * Should have create_ldt_entry() return failure instead of asserting, then.
     */
#else
    tls_table = (tls_slot_t *)global_heap_alloc(MAX_THREADS *
                                                sizeof(tls_slot_t) HEAPACCT(ACCT_OTHER));
    memset(tls_table, 0, MAX_THREADS * sizeof(tls_slot_t));
#endif
    if (!first_thread_tls_initialized) {
        first_thread_tls_initialized = true;
        if (last_thread_tls_exited) /* re-attach */
            last_thread_tls_exited = false;
    }
    ASSERT(is_thread_tls_initialized());
}

static bool
should_zero_tls_at_thread_exit()
{
#ifdef X86
    /* i#2089: For a thread w/o CLONE_SIGHAND we cannot handle a fault, so we want to
     * leave &uninit_tls (which was put in place in os_thread_exit()) as long as
     * possible.  For non-detach, that means until the exit.
     */
    return !INTERNAL_OPTION(safe_read_tls_init) || doing_detach;
#else
    return true;
#endif
}

/* TLS exit for the current thread who must own local_state. */
void
os_tls_thread_exit(local_state_t *local_state)
{
#ifdef HAVE_TLS
    /* We assume (assert below) that local_state_t's start == local_state_extended_t */
    os_local_state_t *os_tls =
        (os_local_state_t *)(((byte *)local_state) - offsetof(os_local_state_t, state));
    tls_type_t tls_type = os_tls->tls_type;
    int index = os_tls->ldt_index;
    ASSERT(offsetof(local_state_t, spill_space) ==
           offsetof(local_state_extended_t, spill_space));

    if (should_zero_tls_at_thread_exit()) {
        tls_thread_free(tls_type, index);

#    if defined(X86) && defined(X64) && !defined(MACOS)
        if (tls_type == TLS_TYPE_ARCH_PRCTL) {
            /* syscall re-sets gs register so re-clear it */
            if (read_thread_register(SEG_TLS) != 0) {
                static const ptr_uint_t zero = 0;
                WRITE_DR_SEG(zero); /* macro needs lvalue! */
            }
        }
#    endif
    }

    /* We already set TLS to &uninit_tls in os_thread_exit() */

    /* Do not set last_thread_tls_exited if a client_thread is exiting.
     * If set, get_thread_private_dcontext() returns NULL, which may cause
     * other thread fault on using dcontext.
     */
    if (dynamo_exited_all_other_threads && !last_thread_tls_exited) {
        last_thread_tls_exited = true;
        first_thread_tls_initialized = false; /* for possible re-attach */
    }
#endif
}

/* Frees local_state.  If the calling thread is exiting (i.e.,
 * !other_thread) then also frees kernel resources for the calling
 * thread; if other_thread then that may not be possible.
 */
void
os_tls_exit(local_state_t *local_state, bool other_thread)
{
#ifdef HAVE_TLS
#    if defined(X86) && !defined(MACOS64)
    static const ptr_uint_t zero = 0;
#    endif /* X86 */
    /* We can't read from fs: as we can be called from other threads */
#    if defined(X86) && !defined(MACOS64)
    /* If the MSR is in use, writing to the reg faults.  We rely on it being 0
     * to indicate that.
     */
    if (!other_thread && read_thread_register(SEG_TLS) != 0 &&
        should_zero_tls_at_thread_exit()) {
        WRITE_DR_SEG(zero); /* macro needs lvalue! */
    }
#    endif /* X86 */

    /* For another thread we can't really make these syscalls so we have to
     * leave it un-cleaned-up.  That's fine if the other thread is exiting:
     * but for detach (i#95) we get the other thread to run this code.
     */
    if (!other_thread)
        os_tls_thread_exit(local_state);

#    ifndef MACOS64
    /* We can't free prior to tls_thread_free() in case that routine refs os_tls */
    /* ASSUMPTION: local_state_t is laid out at same start as local_state_extended_t */
    os_local_state_t *os_tls =
        (os_local_state_t *)(((byte *)local_state) - offsetof(os_local_state_t, state));
    heap_munmap(os_tls->self, PAGE_SIZE, VMM_SPECIAL_MMAP | VMM_PER_THREAD);
#    endif
#else
    global_heap_free(tls_table, MAX_THREADS * sizeof(tls_slot_t) HEAPACCT(ACCT_OTHER));
    DELETE_LOCK(tls_lock);
#endif
}

static int
os_tls_get_gdt_index(dcontext_t *dcontext)
{
    os_local_state_t *os_tls = (os_local_state_t *)(((byte *)dcontext->local_state) -
                                                    offsetof(os_local_state_t, state));
    if (os_tls->tls_type == TLS_TYPE_GDT)
        return os_tls->ldt_index;
    else
        return -1;
}

void
os_tls_pre_init(int gdt_index)
{
#if defined(X86) && !defined(MACOS64)
    /* Only set to above 0 for tls_type == TLS_TYPE_GDT */
    if (gdt_index > 0) {
        /* PR 458917: clear gdt slot to avoid leak across exec */
        DEBUG_DECLARE(bool ok;)
        static const ptr_uint_t zero = 0;
        /* Be sure to clear the selector before anything that might
         * call get_thread_private_dcontext()
         */
        WRITE_DR_SEG(zero); /* macro needs lvalue! */
        DEBUG_DECLARE(ok =)
        tls_clear_descriptor(gdt_index);
        ASSERT(ok);
    }
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
}

/* Allocates num_slots tls slots aligned with alignment align */
bool
os_tls_calloc(OUT uint *offset, uint num_slots, uint alignment)
{
    bool res = false;
    uint i, count = 0;
    int start = -1;
    uint offs = offsetof(os_local_state_t, client_tls);
    if (num_slots == 0 || num_slots > MAX_NUM_CLIENT_TLS)
        return false;
    d_r_mutex_lock(&client_tls_lock);
    for (i = 0; i < MAX_NUM_CLIENT_TLS; i++) {
        if (!client_tls_allocated[i] &&
            /* ALIGNED doesn't work for 0 */
            (alignment == 0 || ALIGNED(offs + i * sizeof(void *), alignment))) {
            if (start == -1)
                start = i;
            count++;
            if (count >= num_slots)
                break;
        } else {
            start = -1;
            count = 0;
        }
    }
    if (count >= num_slots) {
        for (i = 0; i < num_slots; i++)
            client_tls_allocated[i + start] = true;
        *offset = offs + start * sizeof(void *);
        res = true;
    }
    d_r_mutex_unlock(&client_tls_lock);
    return res;
}

bool
os_tls_cfree(uint offset, uint num_slots)
{
    uint i;
    uint offs = (offset - offsetof(os_local_state_t, client_tls)) / sizeof(void *);
    bool ok = true;
    d_r_mutex_lock(&client_tls_lock);
    for (i = 0; i < num_slots; i++) {
        if (!client_tls_allocated[i + offs])
            ok = false;
        client_tls_allocated[i + offs] = false;
    }
    d_r_mutex_unlock(&client_tls_lock);
    return ok;
}

/* os_data is a clone_record_t for signal_thread_inherit */
void
os_thread_init(dcontext_t *dcontext, void *os_data)
{
    os_local_state_t *os_tls = get_os_tls();
    os_thread_data_t *ostd = (os_thread_data_t *)heap_alloc(
        dcontext, sizeof(os_thread_data_t) HEAPACCT(ACCT_OTHER));
    dcontext->os_field = (void *)ostd;
    /* make sure stack fields, etc. are 0 now so they can be initialized on demand
     * (don't have app esp register handy here to init now)
     */
    memset(ostd, 0, sizeof(*ostd));

    ksynch_init_var(&ostd->suspended);
    ksynch_init_var(&ostd->wakeup);
    ksynch_init_var(&ostd->resumed);
    ksynch_init_var(&ostd->terminated);
    ksynch_init_var(&ostd->detached);

#ifdef RETURN_AFTER_CALL
    /* We only need the stack bottom for the initial thread, and due to thread
     * init now preceding vm_areas_init(), we initialize in find_executable_vm_areas()
     */
    ostd->stack_bottom_pc = NULL;
#endif

    ASSIGN_INIT_LOCK_FREE(ostd->suspend_lock, suspend_lock);

    signal_thread_init(dcontext, os_data);

    /* i#107, initialize thread area information,
     * the value was first get in os_tls_init and stored in os_tls
     */
    ostd->priv_lib_tls_base = os_tls->os_seg_info.priv_lib_tls_base;
    ostd->priv_alt_tls_base = os_tls->os_seg_info.priv_alt_tls_base;
    ostd->dr_tls_base = os_tls->os_seg_info.dr_tls_base;

    LOG(THREAD, LOG_THREADS, 1, "TLS app lib base  =" PFX "\n", os_tls->app_lib_tls_base);
    LOG(THREAD, LOG_THREADS, 1, "TLS app alt base  =" PFX "\n", os_tls->app_alt_tls_base);
    LOG(THREAD, LOG_THREADS, 1, "TLS priv lib base =" PFX "\n", ostd->priv_lib_tls_base);
    LOG(THREAD, LOG_THREADS, 1, "TLS priv alt base =" PFX "\n", ostd->priv_alt_tls_base);
    LOG(THREAD, LOG_THREADS, 1, "TLS DynamoRIO base=" PFX "\n", ostd->dr_tls_base);

#ifdef X86
    if (INTERNAL_OPTION(mangle_app_seg)) {
        ostd->app_thread_areas = heap_alloc(
            dcontext, sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS HEAPACCT(ACCT_OTHER));
        memcpy(ostd->app_thread_areas, os_tls->os_seg_info.app_thread_areas,
               sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS);
    }
#endif

    LOG(THREAD, LOG_THREADS, 1, "post-TLS-setup, cur %s base is " PFX "\n", STR_SEG,
        get_segment_base(SEG_TLS));
    LOG(THREAD, LOG_THREADS, 1, "post-TLS-setup, cur %s base is " PFX "\n", STR_LIB_SEG,
        get_segment_base(LIB_SEG_TLS));

#ifdef MACOS
    /* XXX: do we need to free/close dcontext->thread_port?  I don't think so. */
    dcontext->thread_port = dynamorio_mach_syscall(MACH_thread_self_trap, 0);
    LOG(THREAD, LOG_ALL, 1, "Mach thread port: %d\n", dcontext->thread_port);
#endif
}

/* os_data is a clone_record_t for signal_thread_inherit */
void
os_thread_init_finalize(dcontext_t *dcontext, void *os_data)
{
    /* We do not want to record pending signals until at least synch_thread_init()
     * is finished so we delay until here: but we need this inside the
     * thread_initexit_lock (i#2779).
     */
    signal_thread_inherit(dcontext, os_data);
}

void
os_thread_exit(dcontext_t *dcontext, bool other_thread)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;

    /* i#237/PR 498284: if we had a vfork child call execve we need to clean up
     * the env vars.
     */
    if (dcontext->thread_record->execve)
        handle_execve_post(dcontext);

    DELETE_LOCK(ostd->suspend_lock);

    signal_thread_exit(dcontext, other_thread);

    ksynch_free_var(&ostd->suspended);
    ksynch_free_var(&ostd->wakeup);
    ksynch_free_var(&ostd->resumed);
    ksynch_free_var(&ostd->terminated);
    ksynch_free_var(&ostd->detached);

#ifdef X86
    if (ostd->clone_tls != NULL) {
        if (!other_thread) {
            /* Avoid faults in is_thread_tls_initialized() */
            /* FIXME i#2088: we need to restore the app's aux seg, if any, instead. */
            os_set_dr_tls_base(dcontext, NULL, (byte *)&uninit_tls);
        }
        /* We have to free in release build too b/c "local unprotected" is global. */
        HEAP_TYPE_FREE(dcontext, ostd->clone_tls, os_local_state_t, ACCT_THREAD_MGT,
                       UNPROTECTED);
    }
#endif

    if (INTERNAL_OPTION(private_loader))
        privload_tls_exit(IF_UNIT_TEST_ELSE(NULL, ostd->priv_lib_tls_base));
    /* for non-debug we do fast exit path and don't free local heap */
    DODEBUG({
        if (MACHINE_TLS_IS_DR_TLS) {
#ifdef X86
            heap_free(dcontext, ostd->app_thread_areas,
                      sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS HEAPACCT(ACCT_OTHER));
#endif
        }
        heap_free(dcontext, ostd, sizeof(os_thread_data_t) HEAPACCT(ACCT_OTHER));
    });
}

/* Happens in the parent prior to fork. */
static void
os_fork_pre(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;

    /* Otherwise a thread might wait for us. */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(ostd->fork_threads == NULL && ostd->fork_num_threads == 0);

    /* i#239: Synch with all other threads to ensure that they are holding no
     * locks across the fork.
     * FIXME i#26: Suspend signals received before initializing siginfo are
     * squelched, so we won't be able to suspend threads that are initializing.
     */
    LOG(GLOBAL, 2, LOG_SYSCALLS | LOG_THREADS,
        "fork: synching with other threads to prevent deadlock in child\n");
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER,
                                &ostd->fork_threads, &ostd->fork_num_threads,
                                THREAD_SYNCH_VALID_MCONTEXT,
                                /* If we fail to suspend a thread, there is a
                                 * risk of deadlock in the child, so it's worth
                                 * retrying on failure.
                                 */
                                THREAD_SYNCH_SUSPEND_FAILURE_RETRY)) {
        /* If we failed to synch with all threads, we live with the possiblity
         * of deadlock and continue as normal.
         */
        LOG(GLOBAL, 1, LOG_SYSCALLS | LOG_THREADS,
            "fork: synch failed, possible deadlock in child\n");
        ASSERT_CURIOSITY(false);
    }

    vmm_heap_fork_pre(dcontext);

    /* We go back to the code cache to execute the syscall, so we can't hold
     * locks.  If the synch succeeded, no one else is running, so it should be
     * safe to release these locks.  However, if there are any rogue threads,
     * then releasing these locks will allow them to synch and create threads.
     * Such threads could be running due to synch failure or presence of
     * non-suspendable client threads.  We keep our data in ostd to prevent some
     * conflicts, but there are some unhandled corner cases.
     */
    d_r_mutex_unlock(&thread_initexit_lock);
    d_r_mutex_unlock(&all_threads_synch_lock);
}

/* Happens after the fork in both the parent and child. */
static void
os_fork_post(dcontext_t *dcontext, bool parent)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    /* Re-acquire the locks we released before the fork. */
    d_r_mutex_lock(&all_threads_synch_lock);
    d_r_mutex_lock(&thread_initexit_lock);
    /* Resume the other threads that we suspended. */
    if (parent) {
        LOG(GLOBAL, 2, LOG_SYSCALLS | LOG_THREADS,
            "fork: resuming other threads after fork\n");
    }
    end_synch_with_all_threads(ostd->fork_threads, ostd->fork_num_threads,
                               parent /*resume in parent, not in child*/);
    ostd->fork_threads = NULL; /* Freed by end_synch_with_all_threads. */
    ostd->fork_num_threads = 0;
    vmm_heap_fork_post(dcontext, parent);
}

/* this one is called before child's new logfiles are set up */
void
os_fork_init(dcontext_t *dcontext)
{
    int iter;
    /* We use a larger data size than file_t to avoid clobbering our stack (i#991) */
    ptr_uint_t fd;
    ptr_uint_t flags;

    /* Static assert would save debug build overhead: could use array bound trick */
    ASSERT(sizeof(file_t) <= sizeof(ptr_uint_t));

    /* i#239: If there were unsuspended threads across the fork, we could have
     * forked while another thread held locks.  We reset the locks and try to
     * cope with any intermediate state left behind from the parent.  If we
     * encounter more deadlocks after fork, we can add more lock and data resets
     * on a case by case basis.
     */
    d_r_mutex_fork_reset(&all_threads_synch_lock);
    d_r_mutex_fork_reset(&thread_initexit_lock);

    os_fork_post(dcontext, false /*!parent*/);

    /* re-populate cached data that contains pid */
    pid_cached = get_process_id();
    get_application_pid_helper(true);
    get_application_name_helper(true, true /* not important */);

    /* close all copies of parent files */
    TABLE_RWLOCK(fd_table, write, lock);
    iter = 0;
    do {
        iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, fd_table, iter, &fd,
                                         (void **)&flags);
        if (iter < 0)
            break;
        if (TEST(OS_OPEN_CLOSE_ON_FORK, flags)) {
            close_syscall((file_t)fd);
            iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, fd_table, iter, fd);
        }
    } while (true);
    TABLE_RWLOCK(fd_table, write, unlock);
}

static void
os_swap_dr_tls(dcontext_t *dcontext, bool to_app)
{
#ifdef X86
    /* If the option is off, we really should swap it (xref i#107/i#2088 comments
     * in os_swap_context()) but there are few consequences of not doing it, and we
     * have no code set up separate from the i#2089 scheme here.
     */
    if (!INTERNAL_OPTION(safe_read_tls_init))
        return;
    if (to_app) {
        /* i#2089: we want the child to inherit a TLS with invalid .magic, but we
         * need our own syscall execution and post-syscall code to have valid scratch
         * and dcontext values.  We can't clear our own magic b/c we don't know when
         * the child will be scheduled, so we use a copy of our TLS.  We carefully
         * never have a valid magic there in case a prior child is still unscheduled.
         *
         * We assume the child will not modify this TLS copy in any way.
         * CLONE_SETTLS touc * hes the other segment (we'll have to watch for
         * addition of CLONE_SETTLS_AUX). The parent will use the scratch space
         * returning from the syscall to d_r_dispatch, but we restore via os_clone_post()
         * immediately before anybody calls get_thread_private_dcontext() or
         * anything.
         */
        /* FIXME i#2088: to preserve the app's aux seg, if any, we should pass it
         * and the seg reg value via the clone record (like we do for ARM today).
         */
        os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
        os_local_state_t *cur_tls = get_os_tls_from_dc(dcontext);
        if (ostd->clone_tls == NULL) {
            ostd->clone_tls = (os_local_state_t *)HEAP_TYPE_ALLOC(
                dcontext, os_local_state_t, ACCT_THREAD_MGT, UNPROTECTED);
            LOG(THREAD, LOG_THREADS, 2, "TLS copy is " PFX "\n", ostd->clone_tls);
        }
        /* Leave no window where a prior uninit child could read valid magic by
         * invalidating prior to copying.
         */
        cur_tls->magic = TLS_MAGIC_INVALID;
        memcpy(ostd->clone_tls, cur_tls, sizeof(*ostd->clone_tls));
        cur_tls->magic = TLS_MAGIC_VALID;
        ostd->clone_tls->self = ostd->clone_tls;
        os_set_dr_tls_base(dcontext, NULL, (byte *)ostd->clone_tls);
    } else {
        /* i#2089: restore the parent's DR TLS */
        os_local_state_t *real_tls = get_os_tls_from_dc(dcontext);
        /* For dr_app_start we can end up here with nothing to do, so we check. */
        if (get_segment_base(SEG_TLS) != (byte *)real_tls) {
            DEBUG_DECLARE(os_thread_data_t *ostd =
                              (os_thread_data_t *)dcontext->os_field);
            ASSERT(get_segment_base(SEG_TLS) == (byte *)ostd->clone_tls);
            /* We assume there's no need to copy the scratch slots back */
            os_set_dr_tls_base(dcontext, real_tls, (byte *)real_tls);
        }
    }
#elif defined(AARCHXX)
    /* For aarchxx we don't have a separate thread register for DR, and we
     * always leave the DR pointer in the slot inside the app's or privlib's TLS.
     * That means we have nothing to do here.
     * For SYS_clone, we are ok with the parent's TLS being inherited until
     * new_thread_setup() calls set_thread_register_from_clone_record().
     */
#endif
}

static void
os_new_thread_pre(void)
{
    /* We use a barrier on new threads to ensure we make progress when
     * attaching to an app that is continually making threads.
     * XXX i#1305: if we fully suspend all threads during attach we can
     * get rid of this barrier.
     */
    wait_for_event(dr_attach_finished, 0);
    ATOMIC_INC(int, uninit_thread_count);
}

/* This is called from pre_system_call() and before cloning a client thread in
 * dr_create_client_thread. Hence os_clone_pre is used for app threads as well
 * as client threads. Do not add anything that we do not want to happen while
 * in DR mode.
 */
static void
os_clone_pre(dcontext_t *dcontext)
{
    /* We switch the lib tls segment back to app's segment.
     * Please refer to comment on os_switch_lib_tls.
     */
    if (INTERNAL_OPTION(private_loader)) {
        os_switch_lib_tls(dcontext, true /*to app*/);
    }
    os_swap_dr_tls(dcontext, true /*to app*/);
}

/* This is called from d_r_dispatch prior to post_system_call() and after
 * cloning a client thread in dr_create_client_thread. Hence os_clone_post is
 * used for app threads as well as client threads. Do not add anything that
 * we do not want to happen while in DR mode.
 */
void
os_clone_post(dcontext_t *dcontext)
{
    os_swap_dr_tls(dcontext, false /*to DR*/);
}

byte *
os_get_dr_tls_base(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    return ostd->dr_tls_base;
}

/* We only bother swapping the library segment if we're using the private
 * loader.
 */
bool
os_should_swap_state(void)
{
#ifdef X86
    /* -private_loader currently implies -mangle_app_seg, but let's be safe. */
    return (INTERNAL_OPTION(mangle_app_seg) && INTERNAL_OPTION(private_loader));
#elif defined(AARCHXX) || defined(RISCV64)
    return INTERNAL_OPTION(private_loader);
#endif
}

bool
os_using_app_state(dcontext_t *dcontext)
{
#ifdef X86
    /* FIXME: This could be optimized to avoid the syscall by keeping state in
     * the dcontext.
     */
    if (INTERNAL_OPTION(mangle_app_seg)) {
        return (get_segment_base(TLS_REG_LIB) ==
                os_get_app_tls_base(dcontext, TLS_REG_LIB));
    }
#endif
    /* We're always in the app state if we're not mangling. */
    return true;
}

/* Similar to PEB swapping on Windows, this call will switch between DR's
 * private lib segment base and the app's segment base.
 * i#107/i#2088: If the app wants to use SEG_TLS, we should also switch that back at
 * this boundary, but there are many places where we simply assume it is always
 * installed.
 */
void
os_swap_context(dcontext_t *dcontext, bool to_app, dr_state_flags_t flags)
{
    if (os_should_swap_state())
        os_switch_seg_to_context(dcontext, LIB_SEG_TLS, to_app);
    if (TEST(DR_STATE_DR_TLS, flags))
        os_swap_dr_tls(dcontext, to_app);
}

void
os_thread_under_dynamo(dcontext_t *dcontext)
{
    os_swap_context(dcontext, false /*to dr*/, DR_STATE_GO_NATIVE);
    signal_swap_mask(dcontext, false /*to dr*/);
    start_itimer(dcontext);
}

void
os_thread_not_under_dynamo(dcontext_t *dcontext)
{
    stop_itimer(dcontext);
    signal_swap_mask(dcontext, true /*to app*/);
    os_swap_context(dcontext, true /*to app*/, DR_STATE_GO_NATIVE);
}

void
os_process_under_dynamorio_initiate(dcontext_t *dcontext)
{
    LOG(GLOBAL, LOG_THREADS, 1, "process now under DR\n");
    /* We only support regular process-wide signal handlers for delayed takeover. */
    /* i#2161: we ignore alarm signals during the attach process to avoid races. */
    signal_reinstate_handlers(dcontext, true /*ignore alarm*/);
    /* XXX: there's a tradeoff here: we have a race when we remove the hook
     * because dr_app_stop() has no barrier and a thread sent native might
     * resume from vsyscall after we remove the hook.  However, if we leave the
     * hook, then the next takeover signal might hit a native thread that's
     * inside DR just to go back native after having hit the hook.  For now we
     * remove the hook and rely on translate_from_synchall_to_dispatch() moving
     * threads from vsyscall to our gencode and not relying on the hook being
     * present to finish up their go-native code.
     */
    hook_vsyscall(dcontext, false);
}

void
os_process_under_dynamorio_complete(dcontext_t *dcontext)
{
    /* i#2161: only now do we un-ignore alarm signals. */
    signal_reinstate_alarm_handlers(dcontext);
    IF_NO_MEMQUERY({
        /* Update the memory cache (i#2037) now that we've taken over all the
         * threads, if there may have been a gap between setup and start.
         */
        if (dr_api_entry)
            memcache_update_all_from_os();
    });
}

void
os_process_not_under_dynamorio(dcontext_t *dcontext)
{
    /* We only support regular process-wide signal handlers for mixed-mode control. */
    signal_remove_handlers(dcontext);
    unhook_vsyscall();
    LOG(GLOBAL, LOG_THREADS, 1, "process no longer under DR\n");
}

bool
detach_do_not_translate(thread_record_t *tr)
{
    return false;
}

void
detach_finalize_translation(thread_record_t *tr, priv_mcontext_t *mc)
{
    /* Nothing to do. */
}

void
detach_finalize_cleanup(void)
{
    /* Nothing to do. */
}

static pid_t
get_process_group_id()
{
    return dynamorio_syscall(SYS_getpgid, 0);
}

process_id_t
get_parent_id(void)
{
    return dynamorio_syscall(SYS_getppid, 0);
}

thread_id_t
get_sys_thread_id(void)
{
#ifdef MACOS
    if (kernel_thread_groups)
        return dynamorio_syscall(SYS_thread_selfid, 0);
#else
    if (kernel_thread_groups)
        return dynamorio_syscall(SYS_gettid, 0);
#endif
    return dynamorio_syscall(SYS_getpid, 0);
}

thread_id_t
d_r_get_thread_id(void)
{
    /* i#228/PR 494330: making a syscall here is a perf bottleneck since we call
     * this routine in read and recursive locks so use the TLS value instead
     */
    thread_id_t id = get_tls_thread_id();
    if (id != INVALID_THREAD_ID)
        return id;
    else
        return get_sys_thread_id();
}

thread_id_t
get_tls_thread_id(void)
{
    ptr_int_t tid = 0; /* can't use thread_id_t since it's 32-bits */
    if (!is_thread_tls_initialized())
        return INVALID_THREAD_ID;
    READ_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, tid);
    /* it reads 8-bytes into the memory, which includes app_gs and app_fs.
     * 0x000000007127357b <get_tls_thread_id+37>:      mov    %gs:(%rax),%rax
     * 0x000000007127357f <get_tls_thread_id+41>:      mov    %rax,-0x8(%rbp)
     * so we remove the TRUNCATE check and trucate it on return.
     */
    return (thread_id_t)tid;
}

/* returns the thread-private dcontext pointer for the calling thread */
dcontext_t *
get_thread_private_dcontext(void)
{
#ifdef HAVE_TLS
    dcontext_t *dcontext = NULL;
    /* We have to check this b/c this is called from __errno_location prior
     * to os_tls_init, as well as after os_tls_exit, and early in a new
     * thread's initialization (see comments below on that).
     */
    if (!is_thread_tls_initialized())
        return standalone_library ? GLOBAL_DCONTEXT : NULL;
    /* We used to check tid and return NULL to distinguish parent from child, but
     * that was affecting performance (xref PR 207366: but I'm leaving the assert in
     * for now so debug build will still incur it).  So we fixed the cases that
     * needed that:
     *
     * - dynamo_thread_init() calling is_thread_initialized() for a new thread
     *   created via clone or the start/stop interface: so we have
     *   is_thread_initialized() pay the d_r_get_thread_id() cost.
     * - new_thread_setup()'s ENTER_DR_HOOK kstats, or a crash and the signal
     *   handler asking about dcontext: we have new_thread_dynamo_start()
     *   clear the segment register for us early on.
     * - child of fork (ASSERT_OWN_NO_LOCKS, etc. on re-entering DR):
     *   here we just suppress the assert: we'll use this same dcontext.
     *   xref PR 209518 where w/o this fix we used to need an extra KSTOP.
     *
     * An alternative would be to have the parent thread clear the segment
     * register, or even set up the child's TLS ahead of time ourselves
     * (and special-case so that we know if at clone syscall the app state is not
     * quite correct: but we're already stealing a register there: PR 286194).
     * We could also have the kernel set up TLS for us (PR 285898).
     *
     * For hotp_only or non-full-control (native_exec, e.g.) (PR 212012), this
     * routine is not the only issue: we have to catch all new threads since
     * hotp_only gateways assume tls is set up.
     * Xref PR 192231.
     */
    /* PR 307698: this assert causes large slowdowns (also xref PR 207366) */
    DOCHECK(CHKLVL_DEFAULT + 1, {
        ASSERT(get_tls_thread_id() == get_sys_thread_id() ||
               /* ok for fork as mentioned above */
               pid_cached != get_process_id());
    });
    READ_TLS_SLOT_IMM(TLS_DCONTEXT_OFFSET, dcontext);
    return dcontext;
#else
    /* Assumption: no lock needed on a read => no race conditions between
     * reading and writing same tid!  Since both get and set are only for
     * the current thread, they cannot both execute simultaneously for the
     * same tid, right?
     */
    thread_id_t tid = d_r_get_thread_id();
    int i;
    if (tls_table != NULL) {
        for (i = 0; i < MAX_THREADS; i++) {
            if (tls_table[i].tid == tid) {
                return tls_table[i].dcontext;
            }
        }
    }
    return NULL;
#endif
}

/* sets the thread-private dcontext pointer for the calling thread */
void
set_thread_private_dcontext(dcontext_t *dcontext)
{
#ifdef HAVE_TLS
    ASSERT(is_thread_tls_allocated());
    WRITE_TLS_SLOT_IMM(TLS_DCONTEXT_OFFSET, dcontext);
#else
    thread_id_t tid = d_r_get_thread_id();
    int i;
    bool found = false;
    ASSERT(tls_table != NULL);
    d_r_mutex_lock(&tls_lock);
    for (i = 0; i < MAX_THREADS; i++) {
        if (tls_table[i].tid == tid) {
            if (dcontext == NULL) {
                /* if setting to NULL, clear the entire slot for reuse */
                tls_table[i].tid = 0;
            }
            tls_table[i].dcontext = dcontext;
            found = true;
            break;
        }
    }
    if (!found) {
        if (dcontext == NULL) {
            /* don't do anything...but why would this happen? */
        } else {
            /* look for an empty slot */
            for (i = 0; i < MAX_THREADS; i++) {
                if (tls_table[i].tid == 0) {
                    tls_table[i].tid = tid;
                    tls_table[i].dcontext = dcontext;
                    found = true;
                    break;
                }
            }
        }
    }
    d_r_mutex_unlock(&tls_lock);
    ASSERT(found);
#endif
}

/* replaces old with new
 * use for forking: child should replace parent's id with its own
 */
static void
replace_thread_id(thread_id_t old, thread_id_t new)
{
#ifdef HAVE_TLS
    thread_id_t new_tid = new;
    ASSERT(is_thread_tls_initialized());
    DOCHECK(1, {
        thread_id_t old_tid = 0;
        IF_LINUX_ELSE(READ_TLS_INT_SLOT_IMM(TLS_THREAD_ID_OFFSET, old_tid),
                      READ_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, old_tid));
        ASSERT(old_tid == old);
    });
    IF_LINUX_ELSE(WRITE_TLS_INT_SLOT_IMM(TLS_THREAD_ID_OFFSET, new_tid),
                  WRITE_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, new_tid));
#else
    int i;
    d_r_mutex_lock(&tls_lock);
    for (i = 0; i < MAX_THREADS; i++) {
        if (tls_table[i].tid == old) {
            tls_table[i].tid = new;
            break;
        }
    }
    d_r_mutex_unlock(&tls_lock);
#endif
}

/* translate native flags to platform independent protection bits */
static inline uint
osprot_to_memprot(uint prot)
{
    uint mem_prot = 0;
    if (TEST(PROT_EXEC, prot))
        mem_prot |= MEMPROT_EXEC;
    if (TEST(PROT_READ, prot))
        mem_prot |= MEMPROT_READ;
    if (TEST(PROT_WRITE, prot))
        mem_prot |= MEMPROT_WRITE;
    return mem_prot;
}

/* returns osprot flags preserving all native protection flags except
 * for RWX, which are replaced according to memprot */
uint
osprot_replace_memprot(uint old_osprot, uint memprot)
{
    /* Note only protection flags PROT_ are relevant to mprotect()
     * and they are separate from any other MAP_ flags passed to mmap()
     */
    uint new_osprot = memprot_to_osprot(memprot);
    return new_osprot;
}

/* libc independence */
static inline long
mprotect_syscall(byte *p, size_t size, uint prot)
{
    return dynamorio_syscall(SYS_mprotect, 3, p, size, prot);
}

/* free memory allocated from os_raw_mem_alloc */
bool
os_raw_mem_free(void *p, size_t size, uint flags, heap_error_code_t *error_code)
{
    long rc;
    ASSERT(error_code != NULL);
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));

    rc = munmap_syscall(p, size);
    if (rc != 0) {
        *error_code = -rc;
    } else {
        *error_code = HEAP_ERROR_SUCCESS;
    }
    return (rc == 0);
}

/* try to alloc memory at preferred from os directly,
 * caller is required to handle thread synchronization and to update
 */
void *
os_raw_mem_alloc(void *preferred, size_t size, uint prot, uint flags,
                 heap_error_code_t *error_code)
{
    byte *p;
    uint os_prot = memprot_to_osprot(prot);
    uint os_flags =
        MAP_PRIVATE | MAP_ANONYMOUS | (TEST(RAW_ALLOC_32BIT, flags) ? MAP_32BIT : 0);
#if defined(MACOS) && defined(AARCH64)
    if (TEST(MEMPROT_EXEC, prot))
        os_flags |= MAP_JIT;
#endif

    ASSERT(error_code != NULL);
    /* should only be used on aligned pieces */
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));

    p = mmap_syscall(preferred, size, os_prot, os_flags, -1, 0);
    if (!mmap_syscall_succeeded(p)) {
        *error_code = -(heap_error_code_t)(ptr_int_t)p;
        LOG(GLOBAL, LOG_HEAP, 3, "os_raw_mem_alloc %d bytes failed" PFX "\n", size, p);
        return NULL;
    }
    if (preferred != NULL && p != preferred) {
        *error_code = HEAP_ERROR_NOT_AT_PREFERRED;
        os_raw_mem_free(p, size, flags, error_code);
        LOG(GLOBAL, LOG_HEAP, 3, "os_raw_mem_alloc %d bytes failed" PFX "\n", size, p);
        return NULL;
    }
    LOG(GLOBAL, LOG_HEAP, 2, "os_raw_mem_alloc: " SZFMT " bytes @ " PFX "\n", size, p);
    return p;
}

#ifdef LINUX
void
init_emulated_brk(app_pc exe_end)
{
    ASSERT(DYNAMO_OPTION(emulate_brk));
    if (app_brk_map != NULL) {
        return;
    }
    /* i#1004: emulate brk via a separate mmap.  The real brk starts out empty, but
     * we need at least a page to have an mmap placeholder.  We also want to reserve
     * enough memory to avoid a client lib or other mmap truncating the brk at a
     * too-small size, which can crash the app (i#3982).
     */
#    define BRK_INITIAL_SIZE 4 * 1024 * 1024
    app_brk_map = mmap_syscall(exe_end, BRK_INITIAL_SIZE, PROT_READ | PROT_WRITE,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    ASSERT(mmap_syscall_succeeded(app_brk_map));
    app_brk_cur = app_brk_map;
    app_brk_end = app_brk_map + BRK_INITIAL_SIZE;
    LOG(GLOBAL, LOG_HEAP, 1, "%s: initial brk is " PFX "-" PFX "\n", __FUNCTION__,
        app_brk_cur, app_brk_end);
}

static byte *
emulate_app_brk(dcontext_t *dcontext, byte *new_val)
{
    byte *old_brk = app_brk_cur;
    ASSERT(DYNAMO_OPTION(emulate_brk));
    LOG(THREAD, LOG_HEAP, 2, "%s: cur=" PFX ", requested=" PFX "\n", __FUNCTION__,
        app_brk_cur, new_val);
    new_val = (byte *)ALIGN_FORWARD(new_val, PAGE_SIZE);
    if (new_val == NULL || new_val == app_brk_cur ||
        /* Not allowed to shrink below original base */
        new_val < app_brk_map) {
        /* Just return cur val */
    } else if (new_val < app_brk_cur) {
        /* Shrink */
        if (munmap_syscall(new_val, app_brk_cur - new_val) == 0) {
            app_brk_cur = new_val;
            app_brk_end = new_val;
        }
    } else if (new_val < app_brk_end) {
        /* We've already allocated the space */
        app_brk_cur = new_val;
    } else {
        /* Expand */
        byte *remap = (byte *)dynamorio_syscall(SYS_mremap, 4, app_brk_map,
                                                app_brk_end - app_brk_map,
                                                new_val - app_brk_map, 0 /*do not move*/);
        if (mmap_syscall_succeeded(remap)) {
            ASSERT(remap == app_brk_map);
            app_brk_cur = new_val;
            app_brk_end = new_val;
        } else {
            LOG(THREAD, LOG_HEAP, 1, "%s: mremap to " PFX " failed\n", __FUNCTION__,
                new_val);
        }
    }
    if (app_brk_cur != old_brk)
        handle_app_brk(dcontext, app_brk_map, old_brk, app_brk_cur);
    return app_brk_cur;
}
#endif /* LINUX */

#ifdef LINUX
DR_API
/* XXX: could add dr_raw_mem_realloc() instead of dr_raw_mremap() -- though there
 * is no realloc for Windows: supposed to reserve yourself and then commit in
 * pieces.
 */
void *
dr_raw_mremap(void *old_address, size_t old_size, size_t new_size, int flags,
              void *new_address)
{
    byte *res;
    dr_mem_info_t info;
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* i#173: we need prot + type from prior to mremap */
    DEBUG_DECLARE(bool ok =)
    query_memory_ex(old_address, &info);
    /* XXX: this could be a large region w/ multiple protection regions
     * inside.  For now we assume our handling of it doesn't care.
     */
    ASSERT(ok);
    if (is_pretend_or_executable_writable(old_address))
        info.prot |= DR_MEMPROT_WRITE;
    /* we just unconditionally send the 5th param */
    res = (byte *)dynamorio_syscall(SYS_mremap, 5, old_address, old_size, new_size, flags,
                                    new_address);
    handle_app_mremap(dcontext, res, new_size, old_address, old_size, info.prot,
                      info.size);
    return res;
}

DR_API
void *
dr_raw_brk(void *new_address)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (DYNAMO_OPTION(emulate_brk)) {
        /* i#1004: emulate brk via a separate mmap */
        return (void *)emulate_app_brk(dcontext, (byte *)new_address);
    } else {
        /* We pay the cost of 2 syscalls.  This should be infrequent enough that
         * it doesn't mater.
         */
        if (new_address == NULL) {
            /* Just a query */
            return (void *)dynamorio_syscall(SYS_brk, 1, new_address);
        } else {
            byte *old_brk = (byte *)dynamorio_syscall(SYS_brk, 1, 0);
            byte *res = (byte *)dynamorio_syscall(SYS_brk, 1, new_address);
            handle_app_brk(dcontext, NULL, old_brk, res);
            return res;
        }
    }
}
#endif /* LINUX */

/* caller is required to handle thread synchronization and to update dynamo vm areas */
void
os_heap_free(void *p, size_t size, heap_error_code_t *error_code)
{
    long rc;
    ASSERT(error_code != NULL);
    if (!dynamo_exited)
        LOG(GLOBAL, LOG_HEAP, 4, "os_heap_free: %d bytes @ " PFX "\n", size, p);
    rc = munmap_syscall(p, size);
    if (rc != 0) {
        *error_code = -rc;
    } else {
        *error_code = HEAP_ERROR_SUCCESS;
    }
    ASSERT(rc == 0);
}

/* reserve virtual address space without committing swap space for it,
   and of course no physical pages since it will never be touched */
/* to be transparent, we do not use sbrk, and are
 * instead using mmap, and asserting that all os_heap requests are for
 * reasonably large pieces of memory */
void *
os_heap_reserve(void *preferred, size_t size, heap_error_code_t *error_code,
                bool executable)
{
    void *p;
    uint prot = PROT_NONE;
#ifdef VMX86_SERVER
    /* PR 365331: we need to be in the mmap_text region for code cache and
     * gencode (PROT_EXEC).
     */
    ASSERT(!os_in_vmkernel_userworld() || !executable || preferred == NULL ||
           ((byte *)preferred >= os_vmk_mmap_text_start() &&
            ((byte *)preferred) + size <= os_vmk_mmap_text_end()));
    /* Note that a preferred address overrides PROT_EXEC and a mmap_data
     * address will be honored, even though any execution there will fault.
     */
    /* FIXME: note that PROT_EXEC => read access, so our guard pages and other
     * non-committed memory, while not writable, is readable.
     * Plus, we can't later clear all prot bits for userworld mmap due to PR 107872
     * (PR 365748 covers fixing this for us).
     * But in most uses we should get our preferred vmheap and shouldn't run
     * out of vmheap, so this should be a corner-case issue.
     */
    if (executable)
        prot = PROT_EXEC;
#endif
    /* should only be used on aligned pieces */
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));
    ASSERT(error_code != NULL);
    uint os_flags = MAP_PRIVATE |
        MAP_ANONYMOUS IF_X64(| (DYNAMO_OPTION(heap_in_lower_4GB) ? MAP_32BIT : 0));
#if defined(MACOS) && defined(AARCH64)
    if (executable)
        os_flags |= MAP_JIT;
#endif

    /* FIXME: note that this memory is in fact still committed - see man mmap */
    /* FIXME: case 2347 on Linux or -vm_reserve should be set to false */
    /* FIXME: Need to actually get a mmap-ing with |MAP_NORESERVE */
    p = mmap_syscall(preferred, size, prot, os_flags, -1, 0);
    if (!mmap_syscall_succeeded(p)) {
        *error_code = -(heap_error_code_t)(ptr_int_t)p;
        LOG(GLOBAL, LOG_HEAP, 4, "os_heap_reserve %d bytes failed " PFX "\n", size, p);
        return NULL;
    } else if (preferred != NULL && p != preferred) {
        /* We didn't get the preferred address.  To harmonize with windows behavior and
         * give greater control we fail the reservation. */
        heap_error_code_t dummy;
        *error_code = HEAP_ERROR_NOT_AT_PREFERRED;
        os_heap_free(p, size, &dummy);
        ASSERT(dummy == HEAP_ERROR_SUCCESS);
        LOG(GLOBAL, LOG_HEAP, 4,
            "os_heap_reserve %d bytes at " PFX " not preferred " PFX "\n", size,
            preferred, p);
        return NULL;
    } else {
        *error_code = HEAP_ERROR_SUCCESS;
    }
    LOG(GLOBAL, LOG_HEAP, 2, "os_heap_reserve: %d bytes @ " PFX "\n", size, p);
#ifdef VMX86_SERVER
    /* PR 365331: ensure our memory is all in the mmap_text region */
    ASSERT(!os_in_vmkernel_userworld() || !executable ||
           ((byte *)p >= os_vmk_mmap_text_start() &&
            ((byte *)p) + size <= os_vmk_mmap_text_end()));
#endif
#if defined(ANDROID) && defined(DEBUG)
    /* We don't label in release to be more transparent */
    dynamorio_syscall(SYS_prctl, 5, PR_SET_VMA, PR_SET_VMA_ANON_NAME, p, size,
                      "DynamoRIO-internal");
#endif
    return p;
}

static bool
find_free_memory_in_region(byte *start, byte *end, size_t size, byte **found_start OUT,
                           byte **found_end OUT)
{
    memquery_iter_t iter;
    /* XXX: despite /proc/sys/vm/mmap_min_addr == PAGE_SIZE, mmap won't
     * give me that address if I use it as a hint.
     */
    app_pc last_end = (app_pc)(PAGE_SIZE * 16);
    bool found = false;
    memquery_iterator_start(&iter, NULL, false /*won't alloc*/);
    while (memquery_iterator_next(&iter)) {
        if (iter.vm_start >= start &&
            MIN(iter.vm_start, end) - MAX(last_end, start) >= size) {
            if (found_start != NULL)
                *found_start = MAX(last_end, start);
            if (found_end != NULL)
                *found_end = MIN(iter.vm_start, end);
            found = true;
            break;
        }
        if (iter.vm_end >= end)
            break;
        last_end = iter.vm_end;
    }
    memquery_iterator_stop(&iter);
    return found;
}

void *
os_heap_reserve_in_region(void *start, void *end, size_t size,
                          heap_error_code_t *error_code, bool executable)
{
    byte *p = NULL;
    void *find_start = start;
    byte *try_start = NULL, *try_end = NULL;
    uint iters = 0;

    ASSERT(ALIGNED(start, PAGE_SIZE) && ALIGNED(end, PAGE_SIZE));
    ASSERT(ALIGNED(size, PAGE_SIZE));

    LOG(GLOBAL, LOG_HEAP, 3,
        "os_heap_reserve_in_region: " SZFMT " bytes in " PFX "-" PFX "\n", size, start,
        end);

    /* if no restriction on location use regular os_heap_reserve() */
    if (start == (void *)PTR_UINT_0 && end == (void *)POINTER_MAX)
        return os_heap_reserve(NULL, size, error_code, executable);

        /* loop to handle races */
#define RESERVE_IN_REGION_MAX_ITERS 128
    while (find_free_memory_in_region(find_start, end, size, &try_start, &try_end)) {
        /* If there's space we'd prefer the end, to avoid the common case of
         * a large binary + heap at attach where we're likely to reserve
         * right at the start of the brk: we'd prefer to leave more brk space.
         */
        p = os_heap_reserve(try_end - size, size, error_code, executable);
        if (p != NULL) {
            ASSERT(*error_code == HEAP_ERROR_SUCCESS);
            ASSERT(p >= (byte *)start && p + size <= (byte *)end);
            break;
        }
        if (++iters > RESERVE_IN_REGION_MAX_ITERS) {
            ASSERT_NOT_REACHED();
            break;
        }
        find_start = try_end;
    }
    if (p == NULL)
        *error_code = HEAP_ERROR_CANT_RESERVE_IN_REGION;
    else
        *error_code = HEAP_ERROR_SUCCESS;

    LOG(GLOBAL, LOG_HEAP, 2,
        "os_heap_reserve_in_region: reserved " SZFMT " bytes @ " PFX " in " PFX "-" PFX
        "\n",
        size, p, start, end);
    return p;
}

/* commit previously reserved with os_heap_reserve pages */
/* returns false when out of memory */
/* A replacement of os_heap_alloc can be constructed by using os_heap_reserve
   and os_heap_commit on a subset of the reserved pages. */
/* caller is required to handle thread synchronization */
bool
os_heap_commit(void *p, size_t size, uint prot, heap_error_code_t *error_code)
{
    uint os_prot = memprot_to_osprot(prot);
    long res;
    /* should only be used on aligned pieces */
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));
    ASSERT(p);
    ASSERT(error_code != NULL);

    /* FIXME: note that the memory would not be not truly committed if we have */
    /* not actually marked a mmap-ing without MAP_NORESERVE */
    res = mprotect_syscall(p, size, os_prot);
    if (res != 0) {
        *error_code = -res;
        return false;
    } else {
        *error_code = HEAP_ERROR_SUCCESS;
    }

    LOG(GLOBAL, LOG_HEAP, 2, "os_heap_commit: %d bytes @ " PFX "\n", size, p);
    return true;
}

/* caller is required to handle thread synchronization and to update dynamo vm areas */
void
os_heap_decommit(void *p, size_t size, heap_error_code_t *error_code)
{
    ASSERT(error_code != NULL);

    if (!dynamo_exited)
        LOG(GLOBAL, LOG_HEAP, 4, "os_heap_decommit: %d bytes @ " PFX "\n", size, p);

    *error_code = HEAP_ERROR_SUCCESS;
    /* FIXME: for now do nothing since os_heap_reserve has in fact committed the memory */
    /* TODO:
           p = mmap_syscall(p, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
       we should either do a mremap()
       or we can do a munmap() followed 'quickly' by a mmap() -
       also see above the comment that os_heap_reserve() in fact is not so lightweight
    */
}

bool
os_heap_systemwide_overcommit(heap_error_code_t last_error_code)
{
    /* FIXME: conservative answer yes */
    return true;
}

bool
os_heap_get_commit_limit(size_t *commit_used, size_t *commit_limit)
{
    /* FIXME - NYI */
    return false;
}

/* yield the current thread */
void
os_thread_yield()
{
#ifdef MACOS
    /* XXX i#1291: use raw syscall instead */
    swtch_pri(0);
#else
    dynamorio_syscall(SYS_sched_yield, 0);
#endif
}

bool
thread_signal(process_id_t pid, thread_id_t tid, int signum)
{
#ifdef MACOS
    /* FIXME i#58: this takes in a thread port.  Need to map thread id to port.
     * Need to figure out whether we support raw Mach threads w/o pthread on top.
     */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#else
    /* Note that the pid is equivalent to the thread group id.
     * However, we can have threads sharing address space but not pid
     * (if created via CLONE_VM but not CLONE_THREAD), so make sure to
     * use the pid of the target thread, not our pid.
     */
    return (dynamorio_syscall(SYS_tgkill, 3, pid, tid, signum) == 0);
#endif
}

/* This is not available on all platforms/kernels and so may fail. */
bool
thread_signal_queue(process_id_t pid, thread_id_t tid, int signum, void *value)
{
#ifdef MACOS
    return false;
#else
    kernel_siginfo_t info;
    memset(&info, 0, sizeof(info));
    info.si_signo = signum;
    info.si_code = SI_QUEUE;
    info.si_value.sival_ptr = value;
    /* SYS_rt_sigqueueinfo is on 2.6.31+ so we expect failure on older kernels.
     * The caller can use is_sigqueue_supported() to check.
     */
    return dynamorio_syscall(SYS_rt_tgsigqueueinfo, 4, pid, tid, signum, &info) == 0;
#endif
}

static bool
known_thread_signal(thread_record_t *tr, int signum)
{
#ifdef MACOS
    ptr_int_t res;
    if (tr->dcontext == NULL)
        return FALSE;
    res = dynamorio_syscall(SYS___pthread_kill, 2, tr->dcontext->thread_port, signum);
    LOG(THREAD_GET, LOG_ALL, 3, "%s: signal %d to port %d => %ld\n", __FUNCTION__, signum,
        tr->dcontext->thread_port, res);
    return res == 0;
#else
    return thread_signal(tr->pid, tr->id, signum);
#endif
}

void
os_thread_sleep(uint64 milliseconds)
{
#ifdef MACOS
    semaphore_t sem = MACH_PORT_NULL;
    int res;
#else
    struct timespec remain;
    int count = 0;
#endif
    struct timespec req;
    req.tv_sec = (milliseconds / 1000);
    /* docs say can go up to 1000000000, but doesn't work on FC9 */
    req.tv_nsec = (milliseconds % 1000) * 1000000;
#ifdef MACOS
    if (sem == MACH_PORT_NULL) {
        DEBUG_DECLARE(kern_return_t res =)
        semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO, 0);
        ASSERT(res == KERN_SUCCESS);
    }
    res =
        dynamorio_syscall(SYSNUM_NO_CANCEL(SYS___semwait_signal), 6, sem, MACH_PORT_NULL,
                          1, 1, (int64_t)req.tv_sec, (int32_t)req.tv_nsec);
    if (res == -EINTR) {
        /* FIXME i#58: figure out how much time elapsed and re-wait */
    }
#else
    /* FIXME: if we need accurate sleeps in presence of itimers we should
     * be using SYS_clock_nanosleep w/ an absolute time instead of relative
     */
    while (dynamorio_syscall(SYS_nanosleep, 2, &req, &remain) == -EINTR) {
        /* interrupted by signal or something: finish the interval */
        ASSERT_CURIOSITY_ONCE(remain.tv_sec <= req.tv_sec &&
                              (remain.tv_sec < req.tv_sec ||
                               /* there seems to be some rounding, and sometimes
                                * remain nsec > req nsec (I've seen 40K diff)
                                */
                               req.tv_nsec - remain.tv_nsec < 100000 ||
                               req.tv_nsec - remain.tv_nsec > -100000));
        /* not unusual for client threads to use itimers and have their run
         * routine sleep forever
         */
        if (count++ > 3 && !IS_CLIENT_THREAD(get_thread_private_dcontext())) {
            ASSERT_NOT_REACHED();
            break; /* paranoid */
        }
        req = remain;
    }
#endif
}

/* For an unknown thread, pass tr==NULL.  Always pass pid and tid. */
static bool
send_suspend_signal(thread_record_t *tr, pid_t pid, thread_id_t tid)
{
#ifdef MACOS
    if (tr != NULL)
        return known_thread_signal(tr, SUSPEND_SIGNAL);
    else
        return thread_signal(pid, tid, SUSPEND_SIGNAL);
#else
    if (is_sigqueueinfo_enosys) {
        /* We'd prefer to use sigqueueinfo to better distinguish our signals from app
         * signals, and to support SUSPEND_SIGNAL == NUDGESIG_SIGNUM.  If sigqueueinfo
         * is not available and SUSPEND_SIGNAL == NUDGESIG_SIGNUM, we won't be able to
         * distinguish a suspend from a nudge, but we don't support nudges on old Linux
         * kernels anyway (<2.6.31).
         */
        if (tr != NULL)
            return known_thread_signal(tr, SUSPEND_SIGNAL);
        else
            return thread_signal(pid, tid, SUSPEND_SIGNAL);
    }
    kernel_siginfo_t info;
    if (!create_nudge_signal_payload(&info, 0, NUDGE_IS_SUSPEND, 0, 0))
        return false;
    ptr_int_t res =
        dynamorio_syscall(SYS_rt_tgsigqueueinfo, 4, pid, tid, SUSPEND_SIGNAL, &info);
    return (res >= 0);
#endif
}

bool
os_thread_suspend(thread_record_t *tr)
{
    os_thread_data_t *ostd = (os_thread_data_t *)tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    /* See synch comments in os_thread_resume: the mutex held there
     * prevents prematurely sending a re-suspend signal.
     */
    d_r_mutex_lock(&ostd->suspend_lock);
    ostd->suspend_count++;
    ASSERT(ostd->suspend_count > 0);
    /* If already suspended, do not send another signal.  However, we do
     * need to ensure the target is suspended in case of a race, so we can't
     * just return.
     */
    if (ostd->suspend_count == 1) {
        /* PR 212090: we use a custom signal handler to suspend.  We wait
         * here until the target reaches the suspend point, and leave it
         * up to the caller to check whether it is a safe suspend point,
         * to match Windows behavior.
         */
        ASSERT(ksynch_get_value(&ostd->suspended) == 0);
        if (!send_suspend_signal(tr, tr->pid, tr->id)) {
            ostd->suspend_count--;
            d_r_mutex_unlock(&ostd->suspend_lock);
            return false;
        }
    }
    /* we can unlock before the wait loop b/c we're using a separate "resumed"
     * int and os_thread_resume holds the lock across its wait.  this way a resume
     * can proceed as soon as the suspended thread is suspended, before the
     * suspending thread gets scheduled again.
     */
    d_r_mutex_unlock(&ostd->suspend_lock);
    while (ksynch_get_value(&ostd->suspended) == 0) {
        /* For Linux, waits only if the suspended flag is not set as 1. Return value
         * doesn't matter because the flag will be re-checked.
         */
        /* We time out and assert in debug build to provide better diagnostics than a
         * silent hang.  We can't safely return false b/c the synch model here
         * assumes there will not be a retry until the target reaches the suspend
         * point.  Xref i#2779.
         */
#define SUSPEND_DEBUG_TIMEOUT_MS 5000
        if (ksynch_wait(&ostd->suspended, 0, SUSPEND_DEBUG_TIMEOUT_MS) == -ETIMEDOUT) {
            ASSERT_CURIOSITY(false && "failed to suspend thread in 5s");
        }
        if (ksynch_get_value(&ostd->suspended) == 0) {
            /* If it still has to wait, give up the cpu. */
            os_thread_yield();
        }
    }
    return true;
}

bool
os_thread_resume(thread_record_t *tr)
{
    os_thread_data_t *ostd = (os_thread_data_t *)tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    /* This mutex prevents sending a re-suspend signal before the target
     * reaches a safe post-resume point from a first suspend signal.
     * Given that race, we can't just use atomic_add_exchange_int +
     * atomic_dec_becomes_zero on suspend_count.
     */
    d_r_mutex_lock(&ostd->suspend_lock);
    ASSERT(ostd->suspend_count > 0);
    /* PR 479750: if do get here and target is not suspended then abort
     * to avoid possible deadlocks
     */
    if (ostd->suspend_count == 0) {
        d_r_mutex_unlock(&ostd->suspend_lock);
        return true; /* the thread is "resumed", so success status */
    }
    ostd->suspend_count--;
    if (ostd->suspend_count > 0) {
        d_r_mutex_unlock(&ostd->suspend_lock);
        return true; /* still suspended */
    }
    ksynch_set_value(&ostd->wakeup, 1);
    ksynch_wake(&ostd->wakeup);
    while (ksynch_get_value(&ostd->resumed) == 0) {
        /* For Linux, waits only if the resumed flag is not set as 1.  Return value
         * doesn't matter because the flag will be re-checked.
         */
        ksynch_wait(&ostd->resumed, 0, 0);
        if (ksynch_get_value(&ostd->resumed) == 0) {
            /* If it still has to wait, give up the cpu. */
            os_thread_yield();
        }
    }
    ksynch_set_value(&ostd->wakeup, 0);
    ksynch_set_value(&ostd->resumed, 0);
    d_r_mutex_unlock(&ostd->suspend_lock);
    return true;
}

bool
os_thread_terminate(thread_record_t *tr)
{
    /* PR 297902: for NPTL sending SIGKILL will take down the whole group:
     * so instead we send SUSPEND_SIGNAL and have a flag set telling
     * target thread to execute SYS_exit
     */
    os_thread_data_t *ostd = (os_thread_data_t *)tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ostd->terminate = true;
    /* Even if the thread is currently suspended, it's simpler to send it
     * another signal than to resume it.
     */
    return send_suspend_signal(tr, tr->pid, tr->id);
}

bool
is_thread_terminated(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(ostd != NULL);
    return (ksynch_get_value(&ostd->terminated) == 1);
}

static void
os_wait_thread_futex(KSYNCH_TYPE *var)
{
    while (ksynch_get_value(var) == 0) {
        /* On Linux, waits only if var is not set as 1. Return value
         * doesn't matter because var will be re-checked.
         */
        ksynch_wait(var, 0, 0);
        if (ksynch_get_value(var) == 0) {
            /* If it still has to wait, give up the cpu. */
            os_thread_yield();
        }
    }
}

void
os_wait_thread_terminated(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(ostd != NULL);
    os_wait_thread_futex(&ostd->terminated);
}

void
os_wait_thread_detached(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(ostd != NULL);
    os_wait_thread_futex(&ostd->detached);
}

void
os_signal_thread_detach(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(ostd != NULL);
    ostd->do_detach = true;
}

bool
thread_get_mcontext(thread_record_t *tr, priv_mcontext_t *mc)
{
    /* PR 212090: only works when target is suspended by us, and
     * we then take the signal context
     */
    os_thread_data_t *ostd = (os_thread_data_t *)tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ASSERT(ostd->suspend_count > 0);
    if (ostd->suspend_count == 0)
        return false;
    ASSERT(ostd->suspended_sigcxt != NULL);
    sigcontext_to_mcontext(mc, ostd->suspended_sigcxt, DR_MC_ALL);
    IF_ARM(dr_set_isa_mode(tr->dcontext, get_sigcontext_isa_mode(ostd->suspended_sigcxt),
                           NULL));
    return true;
}

bool
thread_set_mcontext(thread_record_t *tr, priv_mcontext_t *mc)
{
    /* PR 212090: only works when target is suspended by us, and
     * we then replace the signal context
     */
    os_thread_data_t *ostd = (os_thread_data_t *)tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ASSERT(ostd->suspend_count > 0);
    if (ostd->suspend_count == 0)
        return false;
    ASSERT(ostd->suspended_sigcxt != NULL);
    mcontext_to_sigcontext(ostd->suspended_sigcxt, mc, DR_MC_ALL);
    IF_ARM(
        set_sigcontext_isa_mode(ostd->suspended_sigcxt, dr_get_isa_mode(tr->dcontext)));
    return true;
}

/* Only one of mc and dmc can be non-NULL. */
bool
os_context_to_mcontext(dr_mcontext_t *dmc, priv_mcontext_t *mc, os_cxt_ptr_t osc)
{
    if (dmc != NULL)
        sigcontext_to_mcontext(dr_mcontext_as_priv_mcontext(dmc), &osc, dmc->flags);
    else if (mc != NULL)
        sigcontext_to_mcontext(mc, &osc, DR_MC_ALL);
    else
        return false;
    return true;
}

/* Only one of mc and dmc can be non-NULL. */
bool
mcontext_to_os_context(os_cxt_ptr_t osc, dr_mcontext_t *dmc, priv_mcontext_t *mc)
{
    if (dmc != NULL)
        mcontext_to_sigcontext(&osc, dr_mcontext_as_priv_mcontext(dmc), dmc->flags);
    else if (mc != NULL)
        mcontext_to_sigcontext(&osc, mc, DR_MC_ALL);
    else
        return false;
    return true;
}

bool
is_thread_currently_native(thread_record_t *tr)
{
    return (!tr->under_dynamo_control ||
            /* start/stop doesn't change under_dynamo_control and has its own field */
            (tr->dcontext != NULL && tr->dcontext->currently_stopped));
}

#ifdef LINUX /* XXX i#58: just until we have Mac support */
static void
client_thread_run(void)
{
    void (*func)(void *param);
    dcontext_t *dcontext;
    byte *xsp;
    GET_STACK_PTR(xsp);
#    ifdef AARCH64
    /* AArch64's Scalable Vector Extension (SVE) requires more space on the
     * stack. Align to page boundary, similar to that in get_clone_record().
     */
    xsp = (app_pc)ALIGN_BACKWARD(xsp, PAGE_SIZE);
#    endif
    void *crec = get_clone_record((reg_t)xsp);
    /* i#2335: we support setup separate from start, and we want to allow a client
     * to create a client thread during init, but we do not support that thread
     * executing until the app has started (b/c we have no signal handlers in place).
     */
    /* i#3973: in addition to _executing_ a client thread before the
     * app has started, if we even create the thread before
     * dynamo_initialized is set, we will not copy tls blocks.  By
     * waiting for the app to be started before dynamo_thread_init is
     * called, we ensure this race condition can never happen, since
     * dynamo_initialized will always be set before the app is started.
     */
    wait_for_event(dr_app_started, 0);
    IF_DEBUG(int rc =)
    dynamo_thread_init(get_clone_record_dstack(crec), NULL, crec, true);
    ASSERT(rc != -1); /* this better be a new thread */
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    LOG(THREAD, LOG_ALL, 1, "\n***** CLIENT THREAD %d *****\n\n", d_r_get_thread_id());
    /* We stored the func and args in particular clone record fields */
    func = (void (*)(void *param))dcontext->next_tag;
    /* Reset any inherited mask (i#2337). */
    signal_swap_mask(dcontext, false /*to DR*/);

    void *arg = (void *)get_clone_record_app_xsp(crec);
    LOG(THREAD, LOG_ALL, 1, "func=" PFX ", arg=" PFX "\n", func, arg);

    (*func)(arg);

    LOG(THREAD, LOG_ALL, 1, "\n***** CLIENT THREAD %d EXITING *****\n\n",
        d_r_get_thread_id());
    block_cleanup_and_terminate(dcontext, SYS_exit, 0, 0, false /*just thread*/,
                                IF_MACOS_ELSE(dcontext->thread_port, 0), 0);
}
#endif

/* i#41/PR 222812: client threads
 * * thread must have dcontext since many API routines require one and we
 *   don't expose GLOBAL_DCONTEXT (xref PR 243008, PR 216936, PR 536058)
 * * reversed the old design of not using dstack (partly b/c want dcontext)
 *   and I'm using the same parent-creates-dstack and clone_record_t design
 *   to create linux threads: dstack should be big enough for client threads
 *   (xref PR 202669)
 * * reversed the old design of explicit dr_terminate_client_thread(): now
 *   the thread is auto-terminated and stack cleaned up on return from run
 *   function
 */
DR_API bool
dr_create_client_thread(void (*func)(void *param), void *arg)
{
#ifdef LINUX
    dcontext_t *dcontext = get_thread_private_dcontext();
    byte *xsp;
    /* We do not pass SIGCHLD since don't want signal to parent and don't support
     * waiting on child.
     * We do not pass CLONE_THREAD so that the new thread is in its own thread
     * group, allowing it to have private itimers and not receive any signals
     * sent to the app's thread groups.  It also makes the thread not show up in
     * the thread list for the app, making it more invisible.
     */
    uint flags = CLONE_VM | CLONE_FS | CLONE_FILES |
        CLONE_SIGHAND
            /* CLONE_THREAD required.  Signals and itimers are private anyway. */
            IF_VMX86(| (os_in_vmkernel_userworld() ? CLONE_THREAD : 0));
    pre_second_thread();
    /* need to share signal handler table, prior to creating clone record */
    handle_clone(dcontext, flags);
    ATOMIC_INC(int, uninit_thread_count);
    void *crec = create_clone_record(dcontext, (reg_t *)&xsp, NULL, NULL);
    /* make sure client_thread_run can get the func and arg, and that
     * signal_thread_inherit gets the right syscall info
     */
    set_clone_record_fields(crec, (reg_t)arg, (app_pc)func, SYS_clone, flags);
    LOG(THREAD, LOG_ALL, 1, "dr_create_client_thread xsp=" PFX " dstack=" PFX "\n", xsp,
        get_clone_record_dstack(crec));
    /* i#501 switch to app's tls before creating client thread.
     * i#3526 switch DR's tls to an invalid one before cloning, and switch lib_tls
     * to the app's.
     */
    os_clone_pre(dcontext);
#    ifdef AARCHXX
    /* We need to invalidate DR's TLS to avoid get_thread_private_dcontext() finding one
     * and hitting asserts in dynamo_thread_init lock calls -- yet we don't want to for
     * app threads, so we're doing this here and not in os_clone_pre().
     * XXX: Find a way to put this in os_clone_* to simplify the code?
     */
    void *tls = (void *)read_thread_register(LIB_SEG_TLS);
    write_thread_register(NULL);
#    endif
    thread_id_t newpid = dynamorio_clone(flags, xsp, NULL, NULL, NULL, client_thread_run);
    /* i#3526 switch DR's tls back to the original one before cloning. */
    os_clone_post(dcontext);
#    ifdef AARCHXX
    write_thread_register(tls);
#    endif
    /* i#501 the app's tls was switched in os_clone_pre. */
    if (INTERNAL_OPTION(private_loader))
        os_switch_lib_tls(dcontext, false /*to dr*/);
    if (newpid < 0) {
        LOG(THREAD, LOG_ALL, 1, "client thread creation failed: %d\n", newpid);
        return false;
    } else if (newpid == 0) {
        /* dynamorio_clone() should have called client_thread_run directly */
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
#else
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME i#58: implement on Mac */
    return false;
#endif
}

int
get_num_processors(void)
{
    static uint num_cpu = 0; /* cached value */
    if (!num_cpu) {
#ifdef MACOS
        DEBUG_DECLARE(bool ok =)
        sysctl_query(CTL_HW, HW_NCPU, &num_cpu, sizeof(num_cpu));
        ASSERT(ok);
#else
        /* We used to use get_nprocs_conf, but that's in libc, so now we just
         * look at the /sys filesystem ourselves, which is what glibc does.
         */
        uint local_num_cpus = 0;
        file_t cpu_dir = os_open_directory("/sys/devices/system/cpu", OS_OPEN_READ);
        dir_iterator_t iter;
        ASSERT(cpu_dir != INVALID_FILE &&
               "/sys must be mounted: mount -t sysfs sysfs /sys");
        os_dir_iterator_start(&iter, cpu_dir);
        while (os_dir_iterator_next(&iter)) {
            int dummy_num;
            if (sscanf(iter.name, "cpu%d", &dummy_num) == 1)
                local_num_cpus++;
        }
        os_close(cpu_dir);
        num_cpu = local_num_cpus;
#endif
        ASSERT(num_cpu);
    }
    return num_cpu;
}

/* i#46: To support -no_private_loader, we have to call the dlfcn family of
 * routines in libdl.so.  When we do early injection, there is no loader to
 * resolve these imports, so they will crash.  Early injection is incompatible
 * with -no_private_loader, so this should never happen.
 */

shlib_handle_t
load_shared_library(const char *name, bool reachable)
{
#ifdef STATIC_LIBRARY
    if (os_files_same(name, get_application_name())) {
        /* The private loader falls back to dlsym() and friends for modules it
         * doesn't recognize, so this works without disabling the private loader.
         */
        return dlopen(NULL, RTLD_LAZY); /* Gets a handle to the exe. */
    }
#endif
    /* We call locate_and_load_private_library() to support searching for
     * a pathless name.
     */
    if (INTERNAL_OPTION(private_loader))
        return (shlib_handle_t)locate_and_load_private_library(name, reachable);
#if defined(STATIC_LIBRARY) || defined(MACOS)
    ASSERT(!DYNAMO_OPTION(early_inject));
    return dlopen(name, RTLD_LAZY);
#else
    /* -no_private_loader is no longer supported in our default builds.
     * If we want it for hybrid mode we should add a new build param and include
     * the libdl calls here under that param.
     */
    ASSERT_NOT_REACHED();
    return NULL;
#endif
}

shlib_routine_ptr_t
lookup_library_routine(shlib_handle_t lib, const char *name)
{
    if (INTERNAL_OPTION(private_loader)) {
        return (shlib_routine_ptr_t)get_private_library_address((app_pc)lib, name);
    }
#if defined(STATIC_LIBRARY) || defined(MACOS)
    ASSERT(!DYNAMO_OPTION(early_inject));
    return dlsym(lib, name);
#else
    ASSERT_NOT_REACHED(); /* -no_private_loader is no longer supported: see above */
    return NULL;
#endif
}

void
unload_shared_library(shlib_handle_t lib)
{
    if (INTERNAL_OPTION(private_loader)) {
        unload_private_library(lib);
    } else {
#if defined(STATIC_LIBRARY) || defined(MACOS)
        ASSERT(!DYNAMO_OPTION(early_inject));
        if (!DYNAMO_OPTION(avoid_dlclose)) {
            dlclose(lib);
        }
#else
        ASSERT_NOT_REACHED(); /* -no_private_loader is no longer supported: see above  */
#endif
    }
}

void
shared_library_error(char *buf, int maxlen)
{
    const char *err;
    if (INTERNAL_OPTION(private_loader)) {
        err = "error in private loader";
    } else {
#if defined(STATIC_LIBRARY) || defined(MACOS)
        ASSERT(!DYNAMO_OPTION(early_inject));
        err = dlerror();
        if (err == NULL) {
            err = "dlerror returned NULL";
        }
#else
        ASSERT_NOT_REACHED(); /* -no_private_loader is no longer supported */
        err = "unknown error";
#endif
    }
    strncpy(buf, err, maxlen - 1);
    buf[maxlen - 1] = '\0'; /* strncpy won't put on trailing null if maxes out */
}

/* addr is any pointer known to lie within the library.
 * for linux, one of addr or name is needed; for windows, neither is needed.
 */
bool
shared_library_bounds(IN shlib_handle_t lib, IN byte *addr, IN const char *name,
                      OUT byte **start, OUT byte **end)
{
    ASSERT(start != NULL && end != NULL);
    /* PR 366195: dlopen() handle truly is opaque, so we have to use either
     * addr or name
     */
    ASSERT(addr != NULL || name != NULL);
    *start = addr;
    if (INTERNAL_OPTION(private_loader)) {
        privmod_t *mod;
        /* look for private library first */
        acquire_recursive_lock(&privload_lock);
        mod = privload_lookup_by_base((app_pc)lib);
        if (name != NULL && mod == NULL)
            mod = privload_lookup(name);
        if (mod != NULL && !mod->externally_loaded) {
            *start = mod->base;
            if (end != NULL)
                *end = mod->base + mod->size;
            release_recursive_lock(&privload_lock);
            return true;
        }
        release_recursive_lock(&privload_lock);
    }
    return (memquery_library_bounds(name, start, end, NULL, 0, NULL, 0) > 0);
}

static int
fcntl_syscall(int fd, int cmd, long arg)
{
    return dynamorio_syscall(SYSNUM_NO_CANCEL(SYS_fcntl), 3, fd, cmd, arg);
}

/* dups curfd to a private fd.
 * returns -1 if unsuccessful.
 */
file_t
fd_priv_dup(file_t curfd)
{
    file_t newfd = -1;
    if (DYNAMO_OPTION(steal_fds) > 0) {
        /* RLIMIT_NOFILES is 1 greater than max and F_DUPFD starts at given value */
        /* XXX: if > linux 2.6.24, can use F_DUPFD_CLOEXEC to avoid later call:
         * so how do we tell if the flag is supported?  try calling once at init?
         */
        newfd = fcntl_syscall(curfd, F_DUPFD, min_dr_fd);
        if (newfd < 0) {
            /* We probably ran out of fds, esp if debug build and there are
             * lots of threads.  Should we track how many we've given out to
             * avoid a failed syscall every time after?
             */
            SYSLOG_INTERNAL_WARNING_ONCE("ran out of stolen fd space");
            /* Try again but this time in the app space, somewhere high up
             * to avoid issues like tcsh assuming it can own fds 3-5 for
             * piping std{in,out,err} (xref the old -open_tcsh_fds option).
             */
            newfd = fcntl_syscall(curfd, F_DUPFD, min_dr_fd / 2);
        }
    }
    return newfd;
}

bool
fd_mark_close_on_exec(file_t fd)
{
    /* we assume FD_CLOEXEC is the only flag and don't bother w/ F_GETFD */
    if (fcntl_syscall(fd, F_SETFD, FD_CLOEXEC) != 0) {
        SYSLOG_INTERNAL_WARNING("unable to mark file %d as close-on-exec", fd);
        return false;
    }
    return true;
}

void
fd_table_add(file_t fd, uint flags)
{
    if (fd_table != NULL) {
        TABLE_RWLOCK(fd_table, write, lock);
        DODEBUG({
            /* i#1010: If the fd is already in the table, chances are it's a
             * stale logfile fd left behind by a vforked or cloned child that
             * called execve.  Avoid an assert if that happens.
             */
            bool present = generic_hash_remove(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd);
            ASSERT_CURIOSITY_ONCE(!present && "stale fd not cleaned up");
        });
        generic_hash_add(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd,
                         /* store the flags, w/ a set bit to ensure not 0 */
                         (void *)(ptr_uint_t)(flags | OS_OPEN_RESERVED));
        TABLE_RWLOCK(fd_table, write, unlock);
    } else {
        /* We come here only for the main_logfile and the dual_map_file, which
         * we add to the fd_table in d_r_os_init().
         */
        ASSERT(num_fd_add_pre_heap < MAX_FD_ADD_PRE_HEAP &&
               num_fd_add_pre_heap < (DYNAMO_OPTION(satisfy_w_xor_x) ? 2 : 1) &&
               "only main_logfile and dual_map_file should come here");
        if (num_fd_add_pre_heap < MAX_FD_ADD_PRE_HEAP) {
            fd_add_pre_heap[num_fd_add_pre_heap] = fd;
            fd_add_pre_heap_flags[num_fd_add_pre_heap] = flags;
            num_fd_add_pre_heap++;
        }
    }
}

void
fd_table_remove(file_t fd)
{
    if (fd_table != NULL) {
        TABLE_RWLOCK(fd_table, write, lock);
        generic_hash_remove(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd);
        TABLE_RWLOCK(fd_table, write, unlock);
    } else {
        ASSERT(dynamo_exited || standalone_library);
    }
}

static bool
fd_is_dr_owned(file_t fd)
{
    ptr_uint_t flags;
    ASSERT(fd_table != NULL);
    TABLE_RWLOCK(fd_table, read, lock);
    flags = (ptr_uint_t)generic_hash_lookup(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd);
    TABLE_RWLOCK(fd_table, read, unlock);
    return (flags != 0);
}

static bool
fd_is_in_private_range(file_t fd)
{
    return (DYNAMO_OPTION(steal_fds) > 0 && min_dr_fd > 0 && fd >= min_dr_fd);
}

file_t
os_open_protected(const char *fname, int os_open_flags)
{
    file_t dup;
    file_t res = os_open(fname, os_open_flags);
    if (res < 0)
        return res;
    /* we could have os_open() always switch to a private fd but it's probably
     * not worth the extra syscall for temporary open/close sequences so we
     * only use it for persistent files
     */
    dup = fd_priv_dup(res);
    if (dup >= 0) {
        close_syscall(res);
        res = dup;
        fd_mark_close_on_exec(res);
    } /* else just keep original */

    /* ditto here, plus for things like config.c opening files we can't handle
     * grabbing locks and often don't have heap available so no fd_table
     */
    fd_table_add(res, os_open_flags);

    return res;
}

void
os_close_protected(file_t f)
{
    fd_table_remove(f);
    os_close(f);
}

bool
os_get_current_dir(char *buf, size_t bufsz)
{
#ifdef MACOS
    static char noheap_buf[MAXPATHLEN];
    bool res = false;
    file_t fd = os_open(".", OS_OPEN_READ);
    int len;
    /* F_GETPATH assumes a buffer of size MAXPATHLEN */
    char *fcntl_buf;
    if (dynamo_heap_initialized)
        fcntl_buf = global_heap_alloc(MAXPATHLEN HEAPACCT(ACCT_OTHER));
    else
        fcntl_buf = noheap_buf;
    if (fd == INVALID_FILE)
        goto cwd_error;
    if (fcntl_syscall(fd, F_GETPATH, (long)fcntl_buf) != 0)
        goto cwd_error;
    len = snprintf(buf, bufsz, "%s", fcntl_buf);
    buf[bufsz - 1] = '\0';
    return (len > 0 && len < bufsz);
cwd_error:
    if (dynamo_heap_initialized)
        global_heap_free(fcntl_buf, MAXPATHLEN HEAPACCT(ACCT_OTHER));
    os_close(fd);
    return res;
#else
    return (dynamorio_syscall(SYS_getcwd, 2, buf, bufsz) > 0);
#endif
}

ssize_t
os_write(file_t f, const void *buf, size_t count)
{
    return write_syscall(f, buf, count);
}

/* There are enough differences vs the shared drlibc_os.c version that we override
 * it here.  We use a loop to ensure reachability for the core.
 */
byte *
os_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
            map_flags_t map_flags)
{
    int flags;
    byte *map = NULL;
#if defined(X64)
    bool loop = false;
    uint iters = 0;
#    define MAX_MMAP_LOOP_ITERS 100
    byte *region_start = NULL, *region_end = NULL;
#else
    uint pg_offs;
    ASSERT_TRUNCATE(pg_offs, uint, offs / PAGE_SIZE);
    pg_offs = (uint)(offs / PAGE_SIZE);
#endif
#ifdef VMX86_SERVER
    flags = MAP_PRIVATE; /* MAP_SHARED not supported yet */
#else
    flags = TEST(MAP_FILE_COPY_ON_WRITE, map_flags) ? MAP_PRIVATE : MAP_SHARED;
#endif
#if defined(X64)
    /* Allocate memory from reachable range for image: or anything (pcache
     * in particular): for low 4GB, easiest to just pass MAP_32BIT (which is
     * low 2GB, but good enough).
     */
    if (DYNAMO_OPTION(heap_in_lower_4GB) &&
        !TESTANY(MAP_FILE_FIXED | MAP_FILE_APP, map_flags))
        flags |= MAP_32BIT;
#endif
    /* Allows memory request instead of mapping a file,
     * so we can request memory from a particular address with fixed argument */
    if (f == -1)
        flags |= MAP_ANONYMOUS;
    if (TEST(MAP_FILE_FIXED, map_flags))
        flags |= MAP_FIXED;
#if defined(X64)
    if (!TEST(MAP_32BIT, flags) && TEST(MAP_FILE_REACHABLE, map_flags)) {
        vmcode_get_reachable_region(&region_start, &region_end);
        /* addr need not be NULL: we'll use it if it's in the region */
        ASSERT(!TEST(MAP_FILE_FIXED, map_flags));
        /* Loop to handle races */
        loop = true;
    }
    if ((!TEST(MAP_32BIT, flags) && TEST(MAP_FILE_REACHABLE, map_flags) &&
         (is_vmm_reserved_address(addr, *size, NULL, NULL) ||
          /* Try to honor a library's preferred address.  This does open up a race
           * window during attach where another thread could take this spot,
           * and with this current code we'll never go back and try to get VMM
           * memory.  We live with that as being rare rather than complicate the code.
           */
          !rel32_reachable_from_current_vmcode(addr))) ||
        (TEST(MAP_FILE_FIXED, map_flags) && !TEST(MAP_FILE_VMM_COMMIT, map_flags) &&
         is_vmm_reserved_address(addr, *size, NULL, NULL))) {
        if (DYNAMO_OPTION(vm_reserve)) {
            /* Try to get space inside the vmcode reservation. */
            map = heap_reserve_for_external_mapping(addr, *size,
                                                    VMM_SPECIAL_MMAP | VMM_REACHABLE);
            if (map != NULL) {
                addr = map;
                flags |= MAP_FIXED;
            }
        }
    }
    while (!loop ||
           (addr != NULL && addr >= region_start && addr + *size <= region_end) ||
           find_free_memory_in_region(region_start, region_end, *size, &addr, NULL)) {
#endif
        map = mmap_syscall(addr, *size, memprot_to_osprot(prot), flags, f,
                           /* x86 Linux mmap uses offset in pages */
                           IF_LINUX_ELSE(IF_X64_ELSE(offs, pg_offs), offs));
        if (!mmap_syscall_succeeded(map)) {
            LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, map);
            map = NULL;
        }
#if defined(X64)
        else if (loop && (map < region_start || map + *size > region_end)) {
            /* Try again: probably a race.  Hopefully our notion of "there's a free
             * region big enough" matches the kernel's, else we'll loop forever
             * (which we try to catch w/ a max iters count).
             */
            munmap_syscall(map, *size);
            map = NULL;
        } else
            break;
        if (!loop)
            break;
        if (++iters > MAX_MMAP_LOOP_ITERS) {
            ASSERT_NOT_REACHED();
            map = NULL;
            break;
        }
        addr = NULL; /* pick a new one */
    }
#endif
    return map;
}

bool
os_unmap_file(byte *map, size_t size)
{
    if (DYNAMO_OPTION(vm_reserve) && is_vmm_reserved_address(map, size, NULL, NULL)) {
        /* XXX i#3570: We'd prefer to have the VMM perform this to ensure it matches
         * how it originally reserved the memory.  To do that would we expose a way
         * to ask for MAP_FIXED in os_heap_reserve*()?
         */
        byte *addr = mmap_syscall(map, size, PROT_NONE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (!mmap_syscall_succeeded(addr))
            return false;
        return heap_unreserve_for_external_mapping(map, size,
                                                   VMM_SPECIAL_MMAP | VMM_REACHABLE);
    }
    long res = munmap_syscall(map, size);
    return (res == 0);
}

#ifdef LINUX
static void
os_get_memory_file_shm_path(const char *name, OUT char *buf, size_t bufsz)
{
    snprintf(buf, bufsz, "/dev/shm/%s.%d", name, get_process_id());
    buf[bufsz - 1] = '\0';
}
#endif

file_t
os_create_memory_file(const char *name, size_t size)
{
#ifdef LINUX
    char path[MAXIMUM_PATH];
    file_t fd;
    /* We need an in-memory file. We prefer the new memfd_create over /dev/shm (it
     * has no name conflict issues, stale files left around on a crash, or
     * reliance on tmpfs).
     */
#    ifdef SYS_memfd_create
    snprintf(path, BUFFER_SIZE_ELEMENTS(path), "/%s.%d", name, get_process_id());
    NULL_TERMINATE_BUFFER(path);
    fd = dynamorio_syscall(SYS_memfd_create, 2, path, 0);
#    else
    fd = -ENOSYS;
#    endif
    if (fd == -ENOSYS) {
        /* Fall back on /dev/shm. */
        os_get_memory_file_shm_path(name, path, BUFFER_SIZE_ELEMENTS(path));
        NULL_TERMINATE_BUFFER(path);
        fd = open_syscall(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -EEXIST) {
            /* We assume a stale file from some prior crash. */
            SYSLOG_INTERNAL_WARNING("Removing presumed-stale %s", path);
            os_delete_file(path);
            fd = open_syscall(path, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        }
    }
    if (fd < 0)
        return INVALID_FILE;

    /* Work around an IMA (kernel optional feature "Integrity Measurement
     * Architecture") slowdown where the first executable mmap causes a hash
     * to be computed of the entire file size, which can take 5 or 10
     * *seconds* for gigabyte files.  This is only done once, so if we
     * trigger it while the file is tiny, we can avoid the delay later.
     */
    byte *temp_map = mmap_syscall(0, PAGE_SIZE, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    if (mmap_syscall_succeeded(temp_map))
        munmap_syscall(temp_map, PAGE_SIZE);
    /* Else, not fatal: this may not be destined for a later executable mapping anyway. */

    if (dynamorio_syscall(SYS_ftruncate, 2, fd, size) < 0) {
        close_syscall(fd);
        return INVALID_FILE;
    }
    file_t priv_fd = fd_priv_dup(fd);
    close_syscall(fd); /* Close the old descriptor on success *and* error. */
    if (priv_fd < 0) {
        return INVALID_FILE;
    }
    fd = priv_fd;
    fd_mark_close_on_exec(fd); /* We could use MFD_CLOEXEC for memfd_create. */
    fd_table_add(fd, 0 /*keep across fork*/);
    return fd;
#else
    ASSERT_NOT_IMPLEMENTED(false && "i#3556 NYI for Mac");
    return INVALID_FILE;
#endif
}

void
os_delete_memory_file(const char *name, file_t fd)
{
#ifdef LINUX
    /* There is no need to delete a memfd_create path, but if we used shm we need
     * to clean it up.  We blindly do this rather than trying to record whether
     * we created this file.
     */
    char path[MAXIMUM_PATH];
    os_get_memory_file_shm_path(name, path, BUFFER_SIZE_ELEMENTS(path));
    NULL_TERMINATE_BUFFER(path);
    os_delete_file(path);
    fd_table_remove(fd);
    close_syscall(fd);
#else
    ASSERT_NOT_IMPLEMENTED(false && "i#3556 NYI for Mac");
#endif
}

bool
os_get_disk_free_space(/*IN*/ file_t file_handle,
                       /*OUT*/ uint64 *AvailableQuotaBytes /*OPTIONAL*/,
                       /*OUT*/ uint64 *TotalQuotaBytes /*OPTIONAL*/,
                       /*OUT*/ uint64 *TotalVolumeBytes /*OPTIONAL*/)
{
    /* libc struct seems to match kernel's */
    struct statfs stat;
    ptr_int_t res = dynamorio_syscall(SYS_fstatfs, 2, file_handle, &stat);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: " PIFX "\n", __func__, res);
        return false;
    }
    LOG(GLOBAL, LOG_STATS, 3,
        "os_get_disk_free_space: avail=" SZFMT ", free=" SZFMT ", bsize=" SZFMT "\n",
        stat.f_bavail, stat.f_bfree, stat.f_bsize);
    if (AvailableQuotaBytes != NULL)
        *AvailableQuotaBytes = ((uint64)stat.f_bavail * stat.f_bsize);
    /* no support for quotas */
    if (TotalQuotaBytes != NULL)
        *TotalQuotaBytes = ((uint64)stat.f_bavail * stat.f_bsize);
    if (TotalVolumeBytes != NULL) /* despite name this is how much is free */
        *TotalVolumeBytes = ((uint64)stat.f_bfree * stat.f_bsize);
    return true;
}

#ifdef LINUX
static bool
symlink_is_self_exe(const char *path)
{
    /* Look for "/proc/%d/exe" where %d exists in /proc/self/task/%d,
     * or "/proc/self/exe".  Rule out the exe link for another process
     * (though it could also be under DR we have no simple way to obtain
     * its actual app path).
     */
#    define SELF_LEN_LEADER 6  /* "/proc/" */
#    define SELF_LEN_TRAILER 4 /* "/exe" */
#    define SELF_LEN_MAX 18
    size_t len = strlen(path);
    if (strcmp(path, "/proc/self/exe") == 0)
        return true;
    if (len < SELF_LEN_MAX && /* /proc/nnnnnn/exe */
        strncmp(path, "/proc/", SELF_LEN_LEADER) == 0 &&
        strncmp(path + len - SELF_LEN_TRAILER, "/exe", SELF_LEN_TRAILER) == 0) {
        int pid;
        if (sscanf(path + SELF_LEN_LEADER, "%d", &pid) == 1) {
            char task[32];
            snprintf(task, BUFFER_SIZE_ELEMENTS(task), "/proc/self/task/%d", pid);
            NULL_TERMINATE_BUFFER(task);
            return os_file_exists(task, true /*dir*/);
        }
    }
    return false;
}
#endif

void
exit_process_syscall(long status)
{
    /* We now assume SYS_exit_group is defined: not building on old machines,
     * but will execute there.  We try exit_group and if it fails we use exit.
     *
     * FIXME: if no exit_group, kill all other threads (==processes in same addr
     * space) manually?  Presumably we got here b/c at an unsafe point to do
     * full exit?  Or is that not true: what about dr_abort()?
     */
    dynamorio_syscall(SYSNUM_EXIT_PROCESS, 1, status);
    /* would assert that result is -ENOSYS but assert likely calls us => infinite loop */
    exit_thread_syscall(status);
    ASSERT_NOT_REACHED();
}

void
exit_thread_syscall(long status)
{
#ifdef MACOS
    mach_port_t thread_port = dynamorio_mach_syscall(MACH_thread_self_trap, 0);
    /* FIXME i#1403: on MacOS we fail to free the app's stack: we need to pass it to
     * bsdthread_terminate.
     */
    dynamorio_syscall(SYSNUM_EXIT_THREAD, 4, 0, 0, thread_port, 0);
#else
    dynamorio_syscall(SYSNUM_EXIT_THREAD, 1, status);
#endif
}

/* FIXME: this one will not be easily internationalizable
   yet it is easier to have a syslog based Unix implementation with real strings.
 */

void
os_syslog(syslog_event_type_t priority, uint message_id, uint substitutions_num,
          va_list args)
{
    int native_priority UNUSED;
    switch (priority) {
    case SYSLOG_INFORMATION: native_priority = LOG_INFO; break;
    case SYSLOG_WARNING: native_priority = LOG_WARNING; break;
    case SYSLOG_CRITICAL: native_priority = LOG_CRIT; break;
    case SYSLOG_ERROR: native_priority = LOG_ERR; break;
    default: ASSERT_NOT_REACHED();
    }
    /* can amount to passing a format string (careful here) to vsyslog */

    /* Never let user controlled data in the format string! */
    ASSERT_NOT_IMPLEMENTED(false);
}

/* This is subject to races, but should only happen at init/attach when
 * there should only be one live thread.
 */
static bool
safe_read_via_query(const void *base, size_t size, void *out_buf, size_t *bytes_read)
{
    bool res = false;
    size_t num_read = 0;
    ASSERT(!fault_handling_initialized);
    /* XXX: in today's init ordering, allmem will never be initialized when we come
     * here, but we check it nevertheless to be general in case this routine is
     * ever called at some later time
     */
    if (IF_MEMQUERY_ELSE(false, memcache_initialized()))
        res = is_readable_without_exception_internal(base, size, false /*use allmem*/);
    else
        res = is_readable_without_exception_query_os((void *)base, size);
    if (res) {
        memcpy(out_buf, base, size);
        num_read = size;
    }
    if (bytes_read != NULL)
        *bytes_read = num_read;
    return res;
}

bool
safe_read_ex(const void *base, size_t size, void *out_buf, size_t *bytes_read)
{
    STATS_INC(num_safe_reads);
    /* XXX i#350: we'd like to always use safe_read_fast() and remove this extra
     * call layer, but safe_read_fast() requires fault handling to be set up.
     * We do set up an early signal handler in d_r_os_init(),
     * but there is still be a window prior to that with no handler.
     */
    if (!fault_handling_initialized) {
        return safe_read_via_query(base, size, out_buf, bytes_read);
    } else {
        return safe_read_fast(base, size, out_buf, bytes_read);
    }
}

bool
safe_read_if_fast(const void *base, size_t size, void *out_buf)
{
    if (!fault_handling_initialized) {
        memcpy(out_buf, base, size);
        return true;
    } else {
        return safe_read_ex(base, size, out_buf, NULL);
    }
}

/* FIXME - fold this together with safe_read_ex() (is a lot of places to update) */
bool
d_r_safe_read(const void *base, size_t size, void *out_buf)
{
    return safe_read_ex(base, size, out_buf, NULL);
}

bool
safe_write_ex(void *base, size_t size, const void *in_buf, size_t *bytes_written)
{
    return safe_write_try_except(base, size, in_buf, bytes_written);
}

/* is_readable_without_exception checks to see that all bytes with addresses
 * from pc to pc+size-1 are readable and that reading from there won't
 * generate an exception. if 'from_os' is true, check what the os thinks
 * the prot bits are instead of using the all memory list.
 */
static bool
is_readable_without_exception_internal(const byte *pc, size_t size, bool query_os)
{
    uint prot = MEMPROT_NONE;
    byte *check_pc = (byte *)ALIGN_BACKWARD(pc, PAGE_SIZE);
    if (size > ((byte *)POINTER_MAX - pc))
        size = (byte *)POINTER_MAX - pc;
    do {
        bool rc = query_os ? get_memory_info_from_os(check_pc, NULL, NULL, &prot)
                           : get_memory_info(check_pc, NULL, NULL, &prot);
        if (!rc || !TESTANY(MEMPROT_READ | MEMPROT_EXEC, prot))
            return false;
        if (POINTER_OVERFLOW_ON_ADD(check_pc, PAGE_SIZE))
            break;
        check_pc += PAGE_SIZE;
    } while (check_pc < pc + size);
    return true;
}

bool
is_readable_without_exception(const byte *pc, size_t size)
{
    /* case 9745 / i#853: We've had problems with all_memory_areas not being
     * accurate in the past.  Parsing proc maps is too slow for some apps, so we
     * use a runtime option.
     */
    bool query_os = IF_MEMQUERY_ELSE(true, !DYNAMO_OPTION(use_all_memory_areas));
    return is_readable_without_exception_internal(pc, size, query_os);
}

/* Identical to is_readable_without_exception except that the os is queried
 * for info on the indicated region */
bool
is_readable_without_exception_query_os(byte *pc, size_t size)
{
    return is_readable_without_exception_internal(pc, size, true);
}

bool
is_readable_without_exception_query_os_noblock(byte *pc, size_t size)
{
    if (memquery_from_os_will_block())
        return false;
    return is_readable_without_exception_internal(pc, size, true);
}

bool
is_user_address(byte *pc)
{
    /* FIXME: NYI */
    /* note returning true will always skip the case 9022 logic on Linux */
    return true;
}

/* change protections on memory region starting at pc of length length
 * this does not update the all memory area info
 */
bool
os_set_protection(byte *pc, size_t length, uint prot /*MEMPROT_*/)
{
    app_pc start_page = (app_pc)PAGE_START(pc);
    uint num_bytes = ALIGN_FORWARD(length + (pc - start_page), PAGE_SIZE);
    long res = 0;
    uint flags = memprot_to_osprot(prot);
    DOSTATS({
        /* once on each side of prot, to get on right side of writability */
        if (!TEST(PROT_WRITE, flags)) {
            STATS_INC(protection_change_calls);
            STATS_ADD(protection_change_pages, num_bytes / PAGE_SIZE);
        }
    });
    res = mprotect_syscall((void *)start_page, num_bytes, flags);
    if (res != 0)
        return false;
    LOG(THREAD_GET, LOG_VMAREAS, 3,
        "change_prot(" PFX ", " PIFX ", %s) => "
        "mprotect(" PFX ", " PIFX ", %d)==%d pages\n",
        pc, length, memprot_string(prot), start_page, num_bytes, flags,
        num_bytes / PAGE_SIZE);
    DOSTATS({
        /* once on each side of prot, to get on right side of writability */
        if (TEST(PROT_WRITE, flags)) {
            STATS_INC(protection_change_calls);
            STATS_ADD(protection_change_pages, num_bytes / PAGE_SIZE);
        }
    });
    return true;
}

/* change protections on memory region starting at pc of length length */
bool
set_protection(byte *pc, size_t length, uint prot /*MEMPROT_*/)
{
    if (os_set_protection(pc, length, prot) == false)
        return false;
#ifndef HAVE_MEMINFO_QUERY
    else {
        app_pc start_page = (app_pc)PAGE_START(pc);
        uint num_bytes = ALIGN_FORWARD(length + (pc - start_page), PAGE_SIZE);
        memcache_update_locked(start_page, start_page + num_bytes, prot,
                               -1 /*type unchanged*/, true /*exists*/);
    }
#endif
    return true;
}

/* change protections on memory region starting at pc of length length */
bool
change_protection(byte *pc, size_t length, bool writable)
{
    if (writable)
        return make_writable(pc, length);
    else
        make_unwritable(pc, length);
    return true;
}

/* make pc's page writable */
bool
make_writable(byte *pc, size_t size)
{
    long res;
    app_pc start_page = (app_pc)PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC | PROT_READ | PROT_WRITE;
    /* if can get current protection then keep old read/exec flags.
     * this is crucial on modern linux kernels which refuse to mark stack +x.
     */
    if (!is_in_dynamo_dll(pc) /*avoid allmem assert*/ &&
#ifdef STATIC_LIBRARY
        /* FIXME i#975: is_in_dynamo_dll() is always false for STATIC_LIBRARY,
         * but we can't call get_memory_info() until allmem is initialized.  Our
         * uses before then are for patching x86.asm, which is OK.
         */
        IF_NO_MEMQUERY(memcache_initialized() &&)
#endif
            get_memory_info(pc, NULL, NULL, &prot))
        prot |= PROT_WRITE;

    ASSERT(start_page == pc && ALIGN_FORWARD(size, PAGE_SIZE) == size);
    res = mprotect_syscall((void *)start_page, prot_size, prot);
    LOG(THREAD_GET, LOG_VMAREAS, 3, "make_writable: pc " PFX " -> " PFX "-" PFX " %d\n",
        pc, start_page, start_page + prot_size, res);
    ASSERT(res == 0);
    if (res != 0)
        return false;
    STATS_INC(protection_change_calls);
    STATS_ADD(protection_change_pages, size / PAGE_SIZE);

#ifndef HAVE_MEMINFO_QUERY
    /* update all_memory_areas list with the protection change */
    if (memcache_initialized()) {
        memcache_update_locked(start_page, start_page + prot_size,
                               osprot_to_memprot(prot), -1 /*type unchanged*/,
                               true /*exists*/);
    }
#endif
    return true;
}

/* like make_writable but adds COW */
bool
make_copy_on_writable(byte *pc, size_t size)
{
    /* FIXME: for current usage this should be fine */
    return make_writable(pc, size);
}

/* make pc's page unwritable */
void
make_unwritable(byte *pc, size_t size)
{
    app_pc start_page = (app_pc)PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC | PROT_READ;
    /* if can get current protection then keep old read/exec flags.
     * this is crucial on modern linux kernels which refuse to mark stack +x.
     */
    if (!is_in_dynamo_dll(pc) /*avoid allmem assert*/ &&
#ifdef STATIC_LIBRARY
        /* FIXME i#975: is_in_dynamo_dll() is always false for STATIC_LIBRARY,
         * but we can't call get_memory_info() until allmem is initialized.  Our
         * uses before then are for patching x86.asm, which is OK.
         */
        IF_NO_MEMQUERY(memcache_initialized() &&)
#endif
            get_memory_info(pc, NULL, NULL, &prot))
        prot &= ~PROT_WRITE;

    ASSERT(start_page == pc && ALIGN_FORWARD(size, PAGE_SIZE) == size);
    /* inc stats before making unwritable, in case messing w/ data segment */
    STATS_INC(protection_change_calls);
    STATS_ADD(protection_change_pages, size / PAGE_SIZE);
    DEBUG_DECLARE(long res =) mprotect_syscall((void *)start_page, prot_size, prot);
    LOG(THREAD_GET, LOG_VMAREAS, 3, "make_unwritable: pc " PFX " -> " PFX "-" PFX "\n",
        pc, start_page, start_page + prot_size);
    ASSERT(res == 0);

#ifndef HAVE_MEMINFO_QUERY
    /* update all_memory_areas list with the protection change */
    if (memcache_initialized()) {
        memcache_update_locked(start_page, start_page + prot_size,
                               osprot_to_memprot(prot), -1 /*type unchanged*/,
                               false /*!exists*/);
    }
#endif
}

/****************************************************************************/
/* SYSTEM CALLS */

/* SYS_ defines are in /usr/include/bits/syscall.h
 * numbers used by libc are in /usr/include/asm/unistd.h
 * kernel defines are in /usr/src/linux-2.4/include/asm-i386/unistd.h
 * kernel function names are in /usr/src/linux/arch/i386/kernel/entry.S
 *
 * For now, we've copied the SYS/NR defines from syscall.h and unistd.h
 * and put them in our own local syscall.h.
 */

/* num_raw should be the xax register value.
 * For a live system call, dcontext_live should be passed (for examining
 * the dcontext->last_exit and exit_reason flags); otherwise, gateway should
 * be passed.
 */
int
os_normalized_sysnum(int num_raw, instr_t *gateway, dcontext_t *dcontext)
{
#ifdef MACOS
    /* The x64 encoding indicates the syscall type in the top 8 bits.
     * We drop the 0x2000000 for BSD so we can use the SYS_ enum constants.
     * That leaves 0x1000000 for Mach and 0x3000000 for Machdep.
     * On 32-bit, a different encoding is used: we transform that
     * to the x64 encoding minus BSD.
     */
    int interrupt = 0;
    int num = 0;
    if (gateway != NULL) {
        if (instr_is_interrupt(gateway))
            interrupt = instr_get_interrupt_number(gateway);
    } else {
        ASSERT(dcontext != NULL);
        if (TEST(LINK_SPECIAL_EXIT, dcontext->last_exit->flags)) {
            if (dcontext->upcontext.upcontext.exit_reason ==
                EXIT_REASON_NI_SYSCALL_INT_0x81)
                interrupt = 0x81;
            else {
                ASSERT(dcontext->upcontext.upcontext.exit_reason ==
                       EXIT_REASON_NI_SYSCALL_INT_0x82);
                interrupt = 0x82;
            }
        }
    }
#    ifdef X64
    if (TEST(SYSCALL_NUM_MARKER_BSD, num_raw))
        return (int)(num_raw & 0xffffff); /* Drop BSD bit */
    else
        num = (int)num_raw; /* Keep Mach and Machdep bits */
#    else
    if ((ptr_int_t)num_raw < 0) /* Mach syscall */
        return (SYSCALL_NUM_MARKER_MACH | -(int)num_raw);
    else {
        /* Bottom 16 bits are the number, top are arg size. */
        num = (int)(num_raw & 0xffff);
    }
#    endif
    if (interrupt == 0x81)
        num |= SYSCALL_NUM_MARKER_MACH;
    else if (interrupt == 0x82)
        num |= SYSCALL_NUM_MARKER_MACHDEP;
    return num;
#else
    return num_raw;
#endif
}

static bool
ignorable_system_call_normalized(int num)
{
    switch (num) {
#if defined(SYS_exit_group)
    case SYS_exit_group:
#endif
    case SYS_exit:
#ifdef MACOS
    case SYS_bsdthread_terminate:
#endif
#ifdef LINUX
    case SYS_brk:
#    ifdef SYS_uselib
    case SYS_uselib:
#    endif
#endif
#if defined(X64) || !defined(ARM)
    case SYS_mmap:
#endif
#if !defined(X64) && !defined(MACOS)
    case SYS_mmap2:
#endif
    case SYS_munmap:
#ifdef LINUX
    case SYS_mremap:
#endif
    case SYS_mprotect:
#ifdef ANDROID
    case SYS_prctl:
#endif
    case SYS_execve:
#ifdef LINUX
    case SYS_clone3:
    case SYS_clone:
#elif defined(MACOS)
    case SYS_bsdthread_create:
    case SYS_posix_spawn:
#endif
#ifdef SYS_fork
    case SYS_fork:
#endif
#ifdef SYS_vfork
    case SYS_vfork:
#endif
    case SYS_kill:
#if defined(SYS_tkill)
    case SYS_tkill:
#endif
#if defined(SYS_tgkill)
    case SYS_tgkill:
#endif
#if defined(LINUX) && !defined(X64) && !defined(ARM)
    case SYS_signal:
#endif
#ifdef MACOS
    case SYS_sigsuspend_nocancel:
#endif
#if !defined(X64) || defined(MACOS)
    case SYS_sigaction:
    case SYS_sigsuspend:
    case SYS_sigpending:
    case SYS_sigreturn:
    case SYS_sigprocmask:
#endif
#ifdef LINUX
    case SYS_rt_sigreturn:
    case SYS_rt_sigaction:
    case SYS_rt_sigprocmask:
    case SYS_rt_sigpending:
#    ifdef SYS_rt_sigtimedwait_time64
    case SYS_rt_sigtimedwait_time64:
#    endif
    case SYS_rt_sigtimedwait:
    case SYS_rt_sigqueueinfo:
    case SYS_rt_tgsigqueueinfo:
    case SYS_rt_sigsuspend:
#    ifdef SYS_signalfd
    case SYS_signalfd:
#    endif
    case SYS_signalfd4:
#endif
    case SYS_sigaltstack:
#if defined(LINUX) && !defined(X64) && !defined(ARM)
    case SYS_sgetmask:
    case SYS_ssetmask:
#endif
    case SYS_setitimer:
    case SYS_getitimer:
#ifdef MACOS
    case SYS_close_nocancel:
#endif
#ifdef SYS_close_range
    case SYS_close_range:
#endif
    case SYS_close:
#ifdef SYS_dup2
    case SYS_dup2:
#endif
#ifdef LINUX
    case SYS_dup3:
#endif
#ifdef MACOS
    case SYS_fcntl_nocancel:
#endif
    case SYS_fcntl:
#if defined(X64) || !defined(ARM)
    case SYS_getrlimit:
#endif
#if defined(LINUX) && !defined(X64)
    case SYS_ugetrlimit:
#endif
    case SYS_setrlimit:
#ifdef LINUX
    case SYS_prlimit64:
#endif
#if defined(LINUX) && defined(X86)
    /* i#784: app may have behavior relying on SIGALRM */
    case SYS_alarm:
#endif
        /* i#107: syscall might change/query app's seg memory
         * need stop app from clobbering our GDT slot.
         */
#if defined(LINUX) && defined(X86) && defined(X64)
    case SYS_arch_prctl:
#endif
#if defined(LINUX) && defined(X86)
    case SYS_set_thread_area:
    case SYS_get_thread_area:
        /* FIXME: we might add SYS_modify_ldt later. */
#endif
#if defined(LINUX) && defined(ARM)
    /* syscall changes app's thread register */
    case SYS_set_tls:
    case SYS_cacheflush:
#endif
#if defined(LINUX)
/* Syscalls change procsigmask */
#    ifdef SYS_pselect6_time64
    case SYS_pselect6_time64:
#    endif
    case SYS_pselect6:
#    ifdef SYS_ppoll_time64
    case SYS_ppoll_time64:
#    endif
    case SYS_ppoll:
    case SYS_epoll_pwait:
    /* Used as a lazy trigger. */
    case SYS_rseq:
#endif
#ifdef DEBUG
#    ifdef MACOS
    case SYS_open_nocancel:
#    endif
#    ifdef SYS_open
    case SYS_open:
#    endif
#endif
        return false;
#ifdef LINUX
#    ifdef SYS_readlink
    case SYS_readlink:
#    endif
    case SYS_readlinkat: return !DYNAMO_OPTION(early_inject);
#endif
#ifdef SYS_openat2
    case SYS_openat2:
#endif
    case SYS_openat: return IS_STRING_OPTION_EMPTY(xarch_root);
    default:
#ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(num))
            return vmkuw_ignorable_system_call(num);
#endif
        return true;
    }
}

bool
ignorable_system_call(int num_raw, instr_t *gateway, dcontext_t *dcontext_live)
{
    return ignorable_system_call_normalized(
        os_normalized_sysnum(num_raw, gateway, dcontext_live));
}

typedef struct {
    unsigned long addr;
    unsigned long len;
    unsigned long prot;
    unsigned long flags;
    unsigned long fd;
    unsigned long offset;
} mmap_arg_struct_t;

static inline reg_t *
sys_param_addr(dcontext_t *dcontext, int num)
{
    /* we force-inline get_mcontext() and so don't take it as a param */
    priv_mcontext_t *mc = get_mcontext(dcontext);
#if defined(X86) && defined(X64)
    switch (num) {
    case 0: return &mc->xdi;
    case 1: return &mc->xsi;
    case 2: return &mc->xdx;
    case 3: return &mc->r10; /* since rcx holds retaddr for syscall instr */
    case 4: return &mc->r8;
    case 5: return &mc->r9;
    default: CLIENT_ASSERT(false, "invalid system call parameter number");
    }
#else
#    if defined(MACOS) && defined(X86)
    /* XXX: if we don't end up using dcontext->sys_was_int here, we could
     * make that field Linux-only.
     */
    /* For 32-bit, the args are passed on the stack, above a retaddr slot
     * (regardless of whether using a sysenter or int gateway).
     */
    return ((reg_t *)mc->esp) + 1 /*retaddr*/ + num;
#    endif
    /* even for vsyscall where ecx (syscall) or esp (sysenter) are saved into
     * ebp, the original parameter registers are not yet changed pre-syscall,
     * except for ebp, which is pushed on the stack:
     *     0xffffe400  55                   push   %ebp %esp -> %esp (%esp)
     *     0xffffe401  89 cd                mov    %ecx -> %ebp
     *     0xffffe403  0f 05                syscall -> %ecx
     *
     *     0xffffe400  51                   push   %ecx %esp -> %esp (%esp)
     *     0xffffe401  52                   push   %edx %esp -> %esp (%esp)
     *     0xffffe402  55                   push   %ebp %esp -> %esp (%esp)
     *     0xffffe403  89 e5                mov    %esp -> %ebp
     *     0xffffe405  0f 34                sysenter -> %esp
     */
    switch (num) {
    case 0: return &mc->IF_X86_ELSE(xbx, IF_RISCV64_ELSE(a0, r0));
    case 1: return &mc->IF_X86_ELSE(xcx, IF_RISCV64_ELSE(a1, r1));
    case 2: return &mc->IF_X86_ELSE(xdx, IF_RISCV64_ELSE(a2, r2));
    case 3: return &mc->IF_X86_ELSE(xsi, IF_RISCV64_ELSE(a3, r3));
    case 4: return &mc->IF_X86_ELSE(xdi, IF_RISCV64_ELSE(a4, r4));
    /* FIXME: do a safe_read: but what about performance?
     * See the #if 0 below, as well. */
    case 5:
        return IF_X86_ELSE((dcontext->sys_was_int ? &mc->xbp : ((reg_t *)mc->xsp)),
                           &mc->IF_RISCV64_ELSE(a5, r5));
#    ifdef ARM
    /* AArch32 supposedly has 7 args in some cases. */
    case 6: return &mc->r6;
#    endif
    default: CLIENT_ASSERT(false, "invalid system call parameter number");
    }
#endif
    return 0;
}

static inline reg_t
sys_param(dcontext_t *dcontext, int num)
{
    return *sys_param_addr(dcontext, num);
}

void
set_syscall_param(dcontext_t *dcontext, int param_num, reg_t new_value)
{
    *sys_param_addr(dcontext, param_num) = new_value;
}

static inline bool
syscall_successful(priv_mcontext_t *mc, int normalized_sysnum)
{
#ifdef MACOS
    if (TEST(SYSCALL_NUM_MARKER_MACH, normalized_sysnum)) {
        /* XXX: Mach syscalls vary (for some KERN_SUCCESS=0 is success,
         * for others that return mach_port_t 0 is failure (I think?).
         * We defer to drsyscall.
         */
        return ((ptr_int_t)MCXT_SYSCALL_RES(mc) >= 0);
    } else {
#    ifdef X86
        return !TEST(EFLAGS_CF, mc->xflags);
#    elif defined(AARCH64)
        return !TEST(EFLAGS_C, mc->xflags);
#    else
#        error NYI
#    endif
    }
#else
    if (normalized_sysnum == IF_X64_ELSE(SYS_mmap, SYS_mmap2) ||
#    if !defined(ARM) && !defined(X64)
        normalized_sysnum == SYS_mmap ||
#    endif
        normalized_sysnum == SYS_mremap)
        return mmap_syscall_succeeded((byte *)MCXT_SYSCALL_RES(mc));
    return ((ptr_int_t)MCXT_SYSCALL_RES(mc) >= 0);
#endif
}

/* For non-Mac, this does nothing to indicate "success": you can pass -errno.
 * For Mac, this clears CF and just sets xax.  To return a 64-bit value in
 * 32-bit mode, the caller must explicitly set xdx as well (we don't always
 * do so b/c syscalls that just return 32-bit values do not touch xdx).
 */
static inline void
set_success_return_val(dcontext_t *dcontext, reg_t val)
{
    /* since always coming from d_r_dispatch now, only need to set mcontext */
    priv_mcontext_t *mc = get_mcontext(dcontext);
#ifdef MACOS
    /* On MacOS, success is determined by CF, except for Mach syscalls, but
     * there it doesn't hurt to set CF.
     */
#    ifdef X86
    mc->xflags &= ~(EFLAGS_CF);
#    elif defined(AARCH64)
    mc->xflags &= ~(EFLAGS_C);
#    else
#        error NYI
#    endif
#endif
    MCXT_SYSCALL_RES(mc) = val;
}

/* Always pass a positive value for errno */
static inline void
set_failure_return_val(dcontext_t *dcontext, uint errno_val)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
#ifdef MACOS
    /* On MacOS, success is determined by CF, and errno is positive */
#    ifdef X86
    mc->xflags |= EFLAGS_CF;
#    elif defined(AARCH64)
    mc->xflags |= EFLAGS_C;
#    else
#        error NYI
#    endif
    MCXT_SYSCALL_RES(mc) = errno_val;
#else
    MCXT_SYSCALL_RES(mc) = -(int)errno_val;
#endif
}

DR_API
reg_t
dr_syscall_get_param(void *drcontext, int param_num)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall,
                  "dr_syscall_get_param() can only be called from pre-syscall event");
    return sys_param(dcontext, param_num);
}

DR_API
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_param() can only be called from a syscall event");
    *sys_param_addr(dcontext, param_num) = new_value;
}

DR_API
reg_t
dr_syscall_get_result(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_get_param() can only be called from post-syscall event");
    return MCXT_SYSCALL_RES(get_mcontext(dcontext));
}

DR_API
bool
dr_syscall_get_result_ex(void *drcontext, dr_syscall_result_info_t *info INOUT)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "only call dr_syscall_get_param_ex() from post-syscall event");
    CLIENT_ASSERT(info != NULL, "invalid parameter");
    CLIENT_ASSERT(info->size == sizeof(*info), "invalid dr_syscall_result_info_t size");
    if (info->size != sizeof(*info))
        return false;
    info->value = MCXT_SYSCALL_RES(mc);
    info->succeeded = syscall_successful(mc, dcontext->sys_num);
    if (info->use_high) {
        /* MacOS has some 32-bit syscalls that return 64-bit values in
         * xdx:xax, but the other syscalls don't clear xdx, so we can't easily
         * return a 64-bit value all the time.
         */
        IF_X86_ELSE({ info->high = mc->xdx; }, { ASSERT_NOT_REACHED(); });
    }
    if (info->use_errno) {
        if (info->succeeded)
            info->errno_value = 0;
        else {
            info->errno_value = (uint)IF_LINUX(-(int)) MCXT_SYSCALL_RES(mc);
        }
    }
    return true;
}

DR_API
void
dr_syscall_set_result(void *drcontext, reg_t value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_result() can only be called from a syscall event");
    /* For non-Mac, the caller can still pass -errno and this will work */
    set_success_return_val(dcontext, value);
}

DR_API
bool
dr_syscall_set_result_ex(void *drcontext, dr_syscall_result_info_t *info)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_result() can only be called from a syscall event");
    CLIENT_ASSERT(info->size == sizeof(*info), "invalid dr_syscall_result_info_t size");
    if (info->size != sizeof(*info))
        return false;
    if (info->use_errno) {
        if (info->succeeded) {
            /* a weird case but we let the user combine these */
            set_success_return_val(dcontext, info->errno_value);
        } else
            set_failure_return_val(dcontext, info->errno_value);
    } else {
        if (info->succeeded)
            set_success_return_val(dcontext, info->value);
        else {
            /* use this to set CF, even though it might negate the value */
            set_failure_return_val(dcontext, (uint)info->value);
            /* now set the value, overriding set_failure_return_val() */
            MCXT_SYSCALL_RES(mc) = info->value;
        }
        if (info->use_high) {
            /* MacOS has some 32-bit syscalls that return 64-bit values in
             * xdx:xax.
             */
            IF_X86_ELSE({ mc->xdx = info->high; }, { ASSERT_NOT_REACHED(); });
        }
    }
    return true;
}

DR_API
void
dr_syscall_set_sysnum(void *drcontext, int new_num)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                      dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_sysnum() can only be called from a syscall event");
    MCXT_SYSNUM_REG(mc) = new_num;
}

DR_API
void
dr_syscall_invoke_another(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_invoke_another() can only be called from post-syscall "
                  "event");
    LOG(THREAD, LOG_SYSCALLS, 2, "invoking additional syscall on client request\n");
    dcontext->client_data->invoke_another_syscall = true;
#ifdef X86
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        priv_mcontext_t *mc = get_mcontext(dcontext);
        /* restore xbp to xsp */
        mc->xbp = mc->xsp;
    }
#endif /* X86 */
    /* for x64 we don't need to copy xcx into r10 b/c we use r10 as our param */
}

static inline bool
is_thread_create_syscall_helper(ptr_uint_t sysnum, uint64 flags)
{
#ifdef MACOS
    /* XXX i#1403: we need earlier injection to intercept
     * bsdthread_register in order to capture workqueue threads.
     */
    return (sysnum == SYS_bsdthread_create || sysnum == SYS_vfork);
#else
#    ifdef SYS_vfork
    if (sysnum == SYS_vfork)
        return true;
#    endif
#    ifdef LINUX
    if ((sysnum == SYS_clone || sysnum == SYS_clone3) && TEST(CLONE_VM, flags))
        return true;
#    endif
    return false;
#endif
}

bool
is_thread_create_syscall(dcontext_t *dcontext _IF_LINUX(void *maybe_clone_args))
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    uint64 flags = sys_param(dcontext, 0);
    ptr_uint_t sysnum = MCXT_SYSNUM_REG(mc);
#ifdef LINUX
    /* For clone3, we use flags from the clone_args that was obtained using a
     * a safe read from the user-provided syscall args.
     */
    if (sysnum == SYS_clone3) {
        ASSERT(maybe_clone_args != NULL);
        flags = ((clone3_syscall_args_t *)maybe_clone_args)->flags;
    }
#endif
    return is_thread_create_syscall_helper(sysnum, flags);
}

#ifdef LINUX
static ptr_uint_t
get_stored_clone3_flags(dcontext_t *dcontext)
{
    return ((uint64)dcontext->sys_param4 << 32) | dcontext->sys_param3;
}
#endif

bool
was_thread_create_syscall(dcontext_t *dcontext)
{
    uint64 flags = dcontext->sys_param0;
#ifdef LINUX
    if (dcontext->sys_num == SYS_clone3)
        flags = get_stored_clone3_flags(dcontext);
#endif
    return is_thread_create_syscall_helper(dcontext->sys_num, flags);
}

bool
is_sigreturn_syscall_number(int sysnum)
{
#ifdef MACOS
    return sysnum == SYS_sigreturn;
#else
    return (IF_NOT_X64(sysnum == SYS_sigreturn ||) sysnum == SYS_rt_sigreturn);
#endif
}

bool
is_sigreturn_syscall(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    return is_sigreturn_syscall_number(MCXT_SYSNUM_REG(mc));
}

bool
was_sigreturn_syscall(dcontext_t *dcontext)
{
    return is_sigreturn_syscall_number(dcontext->sys_num);
}

/* process a signal this process/thread is sending to itself */
static void
handle_self_signal(dcontext_t *dcontext, uint sig)
{
    /* FIXME PR 297903: watch for all DEFAULT_TERMINATE signals,
     * and for any thread in the group, not just self.
     *
     * FIXME PR 297033: watch for SIGSTOP and SIGCONT.
     *
     * With -intercept_all_signals, we only need to watch for SIGKILL
     * and SIGSTOP here, and we avoid the FIXMEs below.  If it's fine
     * for DR not to clean up on a SIGKILL, then SIGSTOP is all that's
     * left (at least once we have PR 297033 and are intercepting the
     * various STOP variations and CONT).
     */
    if (sig == SIGABRT && !DYNAMO_OPTION(intercept_all_signals)) {
        LOG(GLOBAL, LOG_TOP | LOG_SYSCALLS, 1,
            "thread " TIDFMT " sending itself a SIGABRT\n", d_r_get_thread_id());
        KSTOP(num_exits_dir_syscall);
        /* FIXME: need to check whether app has a handler for SIGABRT! */
        /* FIXME PR 211180/6723: this will do SYS_exit rather than the SIGABRT.
         * Should do set_default_signal_action(SIGABRT) (and set a flag so
         * no races w/ another thread re-installing?) and then SYS_kill.
         */
        block_cleanup_and_terminate(dcontext, SYSNUM_EXIT_THREAD, -1, 0,
                                    (is_last_app_thread() && !dynamo_exited),
                                    IF_MACOS_ELSE(dcontext->thread_port, 0), 0);
        ASSERT_NOT_REACHED();
    }
}

/***************************************************************************
 * EXECVE
 */

/* when adding here, also add to the switch in handle_execve if necessary */
enum {
    ENV_PROP_RUNUNDER,
    ENV_PROP_OPTIONS,
    ENV_PROP_EXECVE_LOGDIR,
    ENV_PROP_EXE_PATH,
    ENV_PROP_CONFIGDIR,
};

static const char *const env_to_propagate[] = {
    /* these must line up with the enum */
    DYNAMORIO_VAR_RUNUNDER,
    DYNAMORIO_VAR_OPTIONS,
    /* DYNAMORIO_VAR_EXECVE_LOGDIR is different from DYNAMORIO_VAR_LOGDIR:
     * - DYNAMORIO_VAR_LOGDIR: a parent dir inside which a new dir will be created;
     * - DYNAMORIO_VAR_EXECVE_LOGDIR: the same subdir with the pre-execve process.
     * Xref comment in create_log_dir about their precedence.
     */
    DYNAMORIO_VAR_EXECVE_LOGDIR,
    /* i#909: needed for early injection */
    DYNAMORIO_VAR_EXE_PATH,
    /* These will only be propagated if they exist: */
    DYNAMORIO_VAR_CONFIGDIR,
    DYNAMORIO_VAR_AUTOINJECT,
    DYNAMORIO_VAR_ALTINJECT,
};
#define NUM_ENV_TO_PROPAGATE (sizeof(env_to_propagate) / sizeof(env_to_propagate[0]))

/* Called at pre-SYS_execve to append DR vars in the target process env vars list.
 * For late injection via libdrpreload, we call this for *all children, because
 * even if -no_follow_children is specified, a allowlist will still ask for takeover
 * and it's libdrpreload who checks the allowlist.
 * For -early, however, we check the config ahead of time and only call this routine
 * if we in fact want to inject.
 * XXX i#1679: these parent vs child differences bring up corner cases of which
 * config dir takes precedence (if the child clears the HOME env var, e.g.).
 */
static void
add_dr_env_vars(dcontext_t *dcontext, char *inject_library_path, const char *app_path)
{
    char **envp = (char **)sys_param(dcontext, 2);
    int idx, j, preload = -1, ldpath = -1;
    int num_old, num_new, sz;
    bool need_var[NUM_ENV_TO_PROPAGATE];
    int prop_idx[NUM_ENV_TO_PROPAGATE];
    bool ldpath_us = false, preload_us = false;
    char **new_envp, *var, *old;

    /* check if any var needs to be propagated */
    for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
        prop_idx[j] = -1;
        if (get_config_val(env_to_propagate[j]) == NULL)
            need_var[j] = false;
        else
            need_var[j] = true;
    }
    /* Special handling for DYNAMORIO_VAR_EXECVE_LOGDIR:
     * we only need it if follow_children is true and PROCESS_DIR exists.
     */
    if (DYNAMO_OPTION(follow_children) && get_log_dir(PROCESS_DIR, NULL, NULL))
        need_var[ENV_PROP_EXECVE_LOGDIR] = true;
    else
        need_var[ENV_PROP_EXECVE_LOGDIR] = false;

    if (DYNAMO_OPTION(early_inject))
        need_var[ENV_PROP_EXE_PATH] = true;

    /* iterate the env in target process  */
    if (envp == NULL) {
        LOG(THREAD, LOG_SYSCALLS, 3, "\tenv is NULL\n");
        idx = 0;
    } else {
        for (idx = 0; envp[idx] != NULL; idx++) {
            /* execve env vars should never be set here */
            ASSERT(strstr(envp[idx], DYNAMORIO_VAR_EXECVE) != envp[idx]);
            for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
                if (strstr(envp[idx], env_to_propagate[j]) == envp[idx]) {
                    /* If conflict between env and cfg, we assume those env vars
                     * are for DR usage only, and replace them with cfg value.
                     */
                    prop_idx[j] = idx; /* remember the index for replacing later */
                    break;
                }
            }
            if (!DYNAMO_OPTION(early_inject) &&
                strstr(envp[idx], "LD_LIBRARY_PATH=") == envp[idx]) {
                ldpath = idx;
                if (strstr(envp[idx], inject_library_path) != NULL)
                    ldpath_us = true;
            }
            if (!DYNAMO_OPTION(early_inject) &&
                strstr(envp[idx], "LD_PRELOAD=") == envp[idx]) {
                preload = idx;
                if (strstr(envp[idx], DYNAMORIO_PRELOAD_NAME) != NULL &&
                    strstr(envp[idx], get_dynamorio_library_path()) != NULL) {
                    preload_us = true;
                }
            }
            LOG(THREAD, LOG_SYSCALLS, 3, "\tenv %d: %s\n", idx, envp[idx]);
        }
    }

    /* We want to add new env vars, so we create a new envp
     * array.  We have to deallocate them and restore the old
     * envp if execve fails; if execve succeeds, the address
     * space is reset so we don't need to do anything.
     */
    num_old = idx;
    /* how many new env vars we need add */
    num_new = 2 + /* execve indicator var plus final NULL */
        (DYNAMO_OPTION(early_inject)
             ? 0
             : (((preload < 0) ? 1 : 0) + ((ldpath < 0) ? 1 : 0)));

    for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
        if ((DYNAMO_OPTION(follow_children) || j == ENV_PROP_EXE_PATH) && need_var[j] &&
            prop_idx[j] < 0)
            num_new++;
    }
    /* setup new envp */
    new_envp =
        heap_alloc(dcontext, sizeof(char *) * (num_old + num_new) HEAPACCT(ACCT_OTHER));
    /* copy old envp */
    memcpy(new_envp, envp, sizeof(char *) * num_old);
    /* change/add preload and ldpath if necessary */
    if (!DYNAMO_OPTION(early_inject) && !preload_us) {
        int idx_preload;
        LOG(THREAD, LOG_SYSCALLS, 1,
            "WARNING: execve env does NOT preload DynamoRIO, forcing it!\n");
        if (preload >= 0) {
            /* replace the existing preload */
            const char *dr_lib_path = get_dynamorio_library_path();
            sz = strlen(envp[preload]) + strlen(DYNAMORIO_PRELOAD_NAME) +
                strlen(dr_lib_path) + 3;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            old = envp[preload] + strlen("LD_PRELOAD=");
            snprintf(var, sz, "LD_PRELOAD=%s %s %s", DYNAMORIO_PRELOAD_NAME, dr_lib_path,
                     old);
            idx_preload = preload;
        } else {
            /* add new preload */
            const char *dr_lib_path = get_dynamorio_library_path();
            sz = strlen("LD_PRELOAD=") + strlen(DYNAMORIO_PRELOAD_NAME) +
                strlen(dr_lib_path) + 2;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "LD_PRELOAD=%s %s", DYNAMORIO_PRELOAD_NAME, dr_lib_path);
            idx_preload = idx++;
        }
        *(var + sz - 1) = '\0'; /* null terminate */
        new_envp[idx_preload] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", idx_preload,
            new_envp[idx_preload]);
    }
    if (!DYNAMO_OPTION(early_inject) && !ldpath_us) {
        int idx_ldpath;
        if (ldpath >= 0) {
            sz = strlen(envp[ldpath]) + strlen(inject_library_path) + 2;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            old = envp[ldpath] + strlen("LD_LIBRARY_PATH=");
            snprintf(var, sz, "LD_LIBRARY_PATH=%s:%s", inject_library_path, old);
            idx_ldpath = ldpath;
        } else {
            sz = strlen("LD_LIBRARY_PATH=") + strlen(inject_library_path) + 1;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "LD_LIBRARY_PATH=%s", inject_library_path);
            idx_ldpath = idx++;
        }
        *(var + sz - 1) = '\0'; /* null terminate */
        new_envp[idx_ldpath] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", idx_ldpath,
            new_envp[idx_ldpath]);
    }
    /* propagating DR env vars */
    for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
        const char *val = "";
        if (!need_var[j])
            continue;
        if (!DYNAMO_OPTION(follow_children) && j != ENV_PROP_EXE_PATH)
            continue;
        switch (j) {
        case ENV_PROP_RUNUNDER:
            ASSERT(strcmp(env_to_propagate[j], DYNAMORIO_VAR_RUNUNDER) == 0);
            /* Must pass RUNUNDER_ALL to get child injected if has no app config.
             * If rununder var is already set we assume it's set to 1.
             */
            ASSERT((RUNUNDER_ON | RUNUNDER_ALL) == 0x3); /* else, update "3" */
            val = "3";
            break;
        case ENV_PROP_OPTIONS:
            ASSERT(strcmp(env_to_propagate[j], DYNAMORIO_VAR_OPTIONS) == 0);
            val = d_r_option_string;
            break;
        case ENV_PROP_EXECVE_LOGDIR:
            /* we use PROCESS_DIR for DYNAMORIO_VAR_EXECVE_LOGDIR */
            ASSERT(strcmp(env_to_propagate[j], DYNAMORIO_VAR_EXECVE_LOGDIR) == 0);
            ASSERT(get_log_dir(PROCESS_DIR, NULL, NULL));
            break;
        case ENV_PROP_EXE_PATH:
            ASSERT(strcmp(env_to_propagate[j], DYNAMORIO_VAR_EXE_PATH) == 0);
            val = app_path;
            break;
        default:
            val = getenv(env_to_propagate[j]);
            if (val == NULL)
                val = "";
            break;
        }
        if (j == ENV_PROP_EXECVE_LOGDIR) {
            uint logdir_length;
            get_log_dir(PROCESS_DIR, NULL, &logdir_length);
            /* logdir_length includes the terminating NULL */
            sz = strlen(DYNAMORIO_VAR_EXECVE_LOGDIR) + logdir_length + 1 /* '=' */;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "%s=", DYNAMORIO_VAR_EXECVE_LOGDIR);
            get_log_dir(PROCESS_DIR, var + strlen(var), &logdir_length);
        } else {
            sz = strlen(env_to_propagate[j]) + strlen(val) + 2 /* '=' + null */;
            var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "%s=%s", env_to_propagate[j], val);
        }
        *(var + sz - 1) = '\0'; /* null terminate */
        prop_idx[j] = (prop_idx[j] >= 0) ? prop_idx[j] : idx++;
        new_envp[prop_idx[j]] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", prop_idx[j],
            new_envp[prop_idx[j]]);
    }
    if (!DYNAMO_OPTION(follow_children) && !DYNAMO_OPTION(early_inject)) {
        if (prop_idx[ENV_PROP_RUNUNDER] >= 0) {
            /* disable auto-following of this execve, yet still allow preload
             * on other side to inject if config file exists.
             * kind of hacky mangle here:
             */
            ASSERT(!need_var[ENV_PROP_RUNUNDER]);
            ASSERT(new_envp[prop_idx[ENV_PROP_RUNUNDER]][0] == 'D');
            new_envp[prop_idx[ENV_PROP_RUNUNDER]][0] = 'X';
        }
    }

    sz = strlen(DYNAMORIO_VAR_EXECVE) + 4;
    /* we always pass this var to indicate "post-execve" */
    var = heap_alloc(dcontext, sizeof(char) * sz HEAPACCT(ACCT_OTHER));
    /* PR 458917: we overload this to also pass our gdt index */
    ASSERT(os_tls_get_gdt_index(dcontext) < 100 &&
           os_tls_get_gdt_index(dcontext) >= -1); /* only 2 chars allocated */
    snprintf(var, sz, "%s=%02d", DYNAMORIO_VAR_EXECVE, os_tls_get_gdt_index(dcontext));
    *(var + sz - 1) = '\0'; /* null terminate */
    new_envp[idx++] = var;
    LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", idx - 1, new_envp[idx - 1]);
    /* must end with NULL */
    new_envp[idx++] = NULL;
    ASSERT((num_new + num_old) == idx);

    /* update syscall param */
    *sys_param_addr(dcontext, 2) = (reg_t)new_envp; /* OUT */
    /* store for reset in case execve fails, and for cleanup if
     * this is a vfork thread
     */
    dcontext->sys_param0 = (reg_t)envp;
    dcontext->sys_param1 = (reg_t)new_envp;
}

static ssize_t
script_file_reader(const char *pathname, void *buf, size_t count)
{
    /* FIXME i#2090: Check file is executable. */
    file_t file = os_open(pathname, OS_OPEN_READ);
    size_t len;

    if (file == INVALID_FILE)
        return -1;
    len = os_read(file, buf, count);
    os_close(file);
    return len;
}

/* For early injection, recognise when the executable is a script ("#!") and
 * modify the syscall parameters to invoke a script interpreter instead. In
 * this case we will have allocated memory here but we expect the caller to
 * do a non-failing execve of libdynamorio.so and therefore not to have to
 * free the memory. That is one reason for checking that the (final) script
 * interpreter really is an executable binary.
 * We recognise one error case here and return the non-zero error code (ELOOP)
 * but in other cases we leave it up to the caller to detect the error, which
 * it may do by attempting to exec the path natively, expecting this to fail,
 * though there is the obvious danger that the file might have been modified
 * just before the exec.
 * We do not, and cannot easily, handle a file that is executable but not
 * readable. Currently such files will be executed without DynamoRIO though
 * in some situations it would be more helpful to stop with an error.
 *
 * XXX: There is a minor transparency bug with misformed binaries. For example,
 * execve can return EINVAL if the ELF executable has more than one PT_INTERP
 * segment but we do not check this and so under DynamoRIO the error would be
 * detected only after the exec, if we are following the child.
 *
 * FIXME i#2091: There is a memory leak if a script is recognised, and it is
 * later decided not to inject (see where should_inject is set), and the exec
 * fails, because in this case there is no mechanism for freeing the memory
 * allocated in this function. This function should return sufficient information
 * for the caller to free the memory, which it can do so before the exec if it
 * reverts to the original syscall arguments and execs the script.
 */
static int
handle_execve_script(dcontext_t *dcontext)
{
    char *fname = (char *)sys_param(dcontext, 0);
    char **orig_argv = (char **)sys_param(dcontext, 1);
    script_interpreter_t *script;
    int ret = 0;

    script = global_heap_alloc(sizeof(*script) HEAPACCT(ACCT_OTHER));
    if (!find_script_interpreter(script, fname, script_file_reader))
        goto free_and_return;

    if (script->argc == 0) {
        ret = ELOOP;
        goto free_and_return;
    }

    /* Check that the final interpreter is an executable binary. */
    {
        file_t file = os_open(script->argv[0], OS_OPEN_READ);
        bool is64;
        if (file == INVALID_FILE)
            goto free_and_return;
        if (!module_file_is_module64(file, &is64, NULL)) {
            os_close(file);
            goto free_and_return;
        }
    }

    {
        size_t i, orig_argc = 0;
        char **new_argv;

        /* Concatenate new arguments and original arguments. */
        while (orig_argv[orig_argc] != NULL)
            ++orig_argc;
        if (orig_argc == 0)
            orig_argc = 1;
        new_argv = global_heap_alloc((script->argc + orig_argc + 1) *
                                     sizeof(char *) HEAPACCT(ACCT_OTHER));
        for (i = 0; i < script->argc; i++)
            new_argv[i] = script->argv[i];
        new_argv[script->argc] = fname; /* replaces orig_argv[0] */
        for (i = 1; i < orig_argc; i++)
            new_argv[script->argc + i] = orig_argv[i];
        new_argv[script->argc + orig_argc] = NULL;

        /* Modify syscall parameters. */
        *sys_param_addr(dcontext, 0) = (reg_t)new_argv[0];
        *sys_param_addr(dcontext, 1) = (reg_t)new_argv;
    }
    return 0;

free_and_return:
    global_heap_free(script, sizeof(*script) HEAPACCT(ACCT_OTHER));
    return ret;
}

static int
handle_execve(dcontext_t *dcontext)
{
    /* in /usr/src/linux/arch/i386/kernel/process.c:
     *  asmlinkage int sys_execve(struct pt_regs regs) { ...
     *  error = do_execve(filename, (char **) regs.xcx, (char **) regs.xdx, &regs);
     * in fs/exec.c:
     *  int do_execve(char * filename, char ** argv, char ** envp, struct pt_regs * regs)
     */
    /* We need to make sure we get injected into the new image.
     *
     * For legacy late injection:
     * we simply make sure LD_PRELOAD contains us, and that our directory
     * is on LD_LIBRARY_PATH (seems not to work to put absolute paths in
     * LD_PRELOAD).
     * (This doesn't work for setuid programs though.)
     *
     * For -follow_children we also pass the current DYNAMORIO_RUNUNDER and
     * DYNAMORIO_OPTIONS and logdir to the new image to support a simple
     * run-all-children model without bothering w/ setting up config files for
     * children, and to support injecting across execve that does not
     * preserve $HOME.
     * FIXME i#287/PR 546544: we'll need to propagate DYNAMORIO_AUTOINJECT too
     * once we use it in preload
     */
    /* FIXME i#191: supposed to preserve things like pending signal
     * set across execve: going to ignore for now
     */
    char *fname;
    bool x64 = IF_X64_ELSE(true, false);
    bool expect_to_fail UNUSED = false;
    bool should_inject;
    file_t file;
    char *inject_library_path;
    char rununder_buf[16]; /* just an integer printed in ascii */
    bool app_specific, from_env, rununder_on;
#if defined(LINUX) || defined(DEBUG)
    const char **argv;
#endif

    if (DYNAMO_OPTION(follow_children) && DYNAMO_OPTION(early_inject)) {
        int ret = handle_execve_script(dcontext);
        if (ret != 0)
            return ret;
    }

    fname = (char *)sys_param(dcontext, 0);
#if defined(LINUX) || defined(DEBUG)
    argv = (const char **)sys_param(dcontext, 1);
#endif

#ifdef LINUX
    if (DYNAMO_OPTION(early_inject) && symlink_is_self_exe(fname)) {
        /* i#907: /proc/self/exe points at libdynamorio.so.  Make sure we run
         * the right thing here.
         */
        fname = get_application_name();
    }
#endif

    LOG(GLOBAL, LOG_ALL, 1,
        "\n---------------------------------------------------------------------------"
        "\n");
    LOG(THREAD, LOG_ALL, 1,
        "\n---------------------------------------------------------------------------"
        "\n");
    DODEBUG({
        int i;
        SYSLOG_INTERNAL_INFO("-- execve %s --", fname);
        LOG(THREAD, LOG_SYSCALLS, 1, "syscall: execve %s\n", fname);
        LOG(GLOBAL, LOG_TOP | LOG_SYSCALLS, 1, "execve %s\n", fname);
        if (d_r_stats->loglevel >= 3) {
            if (argv == NULL) {
                LOG(THREAD, LOG_SYSCALLS, 3, "\targs are NULL\n");
            } else {
                for (i = 0; argv[i] != NULL; i++) {
                    LOG(THREAD, LOG_SYSCALLS, 2, "\targ %d: len=%d\n", i,
                        strlen(argv[i]));
                    LOG(THREAD, LOG_SYSCALLS, 3, "\targ %d: %s\n", i, argv[i]);
                }
            }
        }
    });

    /* i#237/PR 498284: if we're a vfork "thread" we're really in a different
     * process and if we exec then the parent process will still be alive.  We
     * can't easily clean our own state (dcontext, dstack, etc.) up in our
     * parent process: we need it to invoke the syscall and the syscall might
     * fail.  We could expand cleanup_and_terminate to also be able to invoke
     * SYS_execve: but execve seems more likely to fail than termination
     * syscalls.  Our solution is to mark this thread as "execve" and hide it
     * from regular thread queries; we clean it up in the process-exiting
     * synch_with_thread(), or if the same parent thread performs another vfork
     * (to prevent heap accumulation from repeated vfork+execve).  Since vfork
     * on linux suspends the parent, there cannot be any races with the execve
     * syscall completing: there can't even be peer vfork threads, so we could
     * set a flag and clean up in d_r_dispatch, but that seems overkill.  (If vfork
     * didn't suspend the parent we'd need to touch a marker file or something
     * to know the execve was finished.)
     */
    mark_thread_execve(dcontext->thread_record, true);

#ifdef STATIC_LIBRARY
    /* no way we can inject, we just lose control */
    SYSLOG_INTERNAL_WARNING("WARNING: static DynamoRIO library, losing control on "
                            "execve");
    return 0;
#endif

    /* Issue 20: handle cross-architecture execve */
    file = os_open(fname, OS_OPEN_READ);
    if (file != INVALID_FILE) {
        if (!module_file_is_module64(file, &x64,
                                     NULL /*only care about primary==execve*/))
            expect_to_fail = true;
        os_close(file);
    } else
        expect_to_fail = true;
    inject_library_path =
        IF_X64_ELSE(x64, !x64) ? dynamorio_library_path : dynamorio_alt_arch_path;

    should_inject = DYNAMO_OPTION(follow_children);
    if (get_config_val_other_app(get_short_name(fname), get_process_id(),
                                 x64 ? DR_PLATFORM_64BIT : DR_PLATFORM_32BIT,
                                 DYNAMORIO_VAR_RUNUNDER, rununder_buf,
                                 BUFFER_SIZE_ELEMENTS(rununder_buf), &app_specific,
                                 &from_env, NULL /* 1config is ok */)) {
        if (should_inject_from_rununder(rununder_buf, app_specific, from_env,
                                        &rununder_on))
            should_inject = rununder_on;
    }

    if (should_inject)
        add_dr_env_vars(dcontext, inject_library_path, fname);
    else {
        dcontext->sys_param0 = 0;
        dcontext->sys_param1 = 0;
    }

#ifdef LINUX
    /* We have to be accurate with expect_to_fail as we cannot come back
     * and fail the syscall once the kernel execs DR!
     */
    if (should_inject && DYNAMO_OPTION(early_inject) && !expect_to_fail) {
        /* i#909: change the target image to libdynamorio.so */
        const char *drpath = IF_X64_ELSE(x64, !x64) ? dynamorio_library_filepath
                                                    : dynamorio_alt_arch_filepath;
        TRY_EXCEPT(
            dcontext, /* try */
            {
                if (symlink_is_self_exe(argv[0])) {
                    /* we're out of sys_param entries so we assume argv[0] == fname
                     */
                    dcontext->sys_param3 = (reg_t)argv;
                    argv[0] = fname; /* XXX: handle readable but not writable! */
                } else
                    dcontext->sys_param3 = 0;        /* no restore in post */
                dcontext->sys_param4 = (reg_t)fname; /* store for restore in post */
                *sys_param_addr(dcontext, 0) = (reg_t)drpath;
                LOG(THREAD, LOG_SYSCALLS, 2, "actual execve on: %s\n",
                    (char *)sys_param(dcontext, 0));
            },
            /* except */
            {
                dcontext->sys_param3 = 0; /* no restore in post */
                dcontext->sys_param4 = 0; /* no restore in post */
                LOG(THREAD, LOG_SYSCALLS, 2,
                    "argv is unreadable, expect execve to fail\n");
            });
    } else {
        dcontext->sys_param3 = 0; /* no restore in post */
        dcontext->sys_param4 = 0; /* no restore in post */
    }
#endif

    /* we need to clean up the .1config file here.  if the execve fails,
     * we'll just live w/o dynamic option re-read.
     */
    d_r_config_exit();
    return 0;
}

static void
handle_execve_post(dcontext_t *dcontext)
{
    /* if we get here it means execve failed (doesn't return on success),
     * or we did an execve from a vfork and its memory changes are visible
     * in the parent process.
     * we have to restore env to how it was and free the allocated heap.
     */
    char **old_envp = (char **)dcontext->sys_param0;
    char **new_envp = (char **)dcontext->sys_param1;
#ifdef STATIC_LIBRARY
    /* nothing to clean up */
    return;
#endif
#ifdef LINUX
    if (dcontext->sys_param4 != 0) {
        /* restore original /proc/.../exe */
        *sys_param_addr(dcontext, 0) = dcontext->sys_param4;
        if (dcontext->sys_param3 != 0) {
            /* restore original argv[0] */
            const char **argv = (const char **)dcontext->sys_param3;
            argv[0] = (const char *)dcontext->sys_param4;
        }
    }
#endif
    if (new_envp != NULL) {
        int i;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tcleaning up our env vars\n");
        /* we replaced existing ones and/or added new ones.
         * we can't compare to old_envp b/c it may have changed by now.
         */
        for (i = 0; new_envp[i] != NULL; i++) {
            if (is_dynamo_address((byte *)new_envp[i])) {
                heap_free(dcontext, new_envp[i],
                          sizeof(char) * (strlen(new_envp[i]) + 1) HEAPACCT(ACCT_OTHER));
            }
        }
        i++; /* need to de-allocate final null slot too */
        heap_free(dcontext, new_envp, sizeof(char *) * i HEAPACCT(ACCT_OTHER));
        /* restore prev envp if we're post-syscall */
        if (!dcontext->thread_record->execve)
            *sys_param_addr(dcontext, 2) = (reg_t)old_envp;
    }
}

/* i#237/PR 498284: to avoid accumulation of thread state we clean up a vfork
 * child who invoked execve here so we have at most one outstanding thread.  we
 * also clean up at process exit and before thread creation.  we could do this
 * in d_r_dispatch but too rare to be worth a flag check there.
 */
static void
cleanup_after_vfork_execve(dcontext_t *dcontext)
{
    thread_record_t **threads;
    int num_threads, i;
    if (num_execve_threads == 0)
        return;

    d_r_mutex_lock(&thread_initexit_lock);
    get_list_of_threads_ex(&threads, &num_threads, true /*include execve*/);
    for (i = 0; i < num_threads; i++) {
        if (threads[i]->execve) {
            LOG(THREAD, LOG_SYSCALLS, 2, "cleaning up earlier vfork thread " TIDFMT "\n",
                threads[i]->id);
            dynamo_other_thread_exit(threads[i]);
        }
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    global_heap_free(threads,
                     num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
}

static void
set_stdfile_fileno(stdfile_t **stdfile, file_t file_no)
{
#ifdef STDFILE_FILENO
    (*stdfile)->STDFILE_FILENO = file_no;
#else
#    warning stdfile_t is opaque; DynamoRIO will not set fds of libc FILEs.
    /* i#1973: musl libc support (and potentially other non-glibcs) */
    /* only called by handle_close_pre(), so warning is specific to that. */
    SYSLOG_INTERNAL_WARNING_ONCE(
        "DynamoRIO cannot set the file descriptors of private libc FILEs on "
        "this platform. Client usage of stdio.h stdin, stdout, or stderr may "
        "no longer work as expected, because the app is closing the UNIX fds "
        "backing these.");
#endif
}

/* returns whether to execute syscall */
static bool
handle_close_generic_pre(dcontext_t *dcontext, file_t fd, bool set_return_val)
{
    LOG(THREAD, LOG_SYSCALLS, 3, "syscall: close fd %d\n", fd);

    /* prevent app from closing our files */
    if (fd_is_dr_owned(fd)) {
        SYSLOG_INTERNAL_WARNING_ONCE("app trying to close DR file(s)");
        LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
            "WARNING: app trying to close DR file %d!  Not allowing it.\n", fd);
        if (set_return_val) {
            if (DYNAMO_OPTION(fail_on_stolen_fds)) {
                set_failure_return_val(dcontext, EBADF);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            } else
                set_success_return_val(dcontext, 0);
        }
        return false; /* do not execute syscall */
    }

    /* Xref PR 258731 - duplicate STDOUT/STDERR when app closes them so we (or
     * a client) can continue to use them for logging. */
    if (DYNAMO_OPTION(dup_stdout_on_close) && fd == STDOUT) {
        our_stdout = fd_priv_dup(fd);
        if (our_stdout < 0) /* no private fd available */
            our_stdout = dup_syscall(fd);
        if (our_stdout >= 0)
            fd_mark_close_on_exec(our_stdout);
        fd_table_add(our_stdout, 0);
        LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
            "WARNING: app is closing stdout=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n",
            fd, our_stdout);
        if (privmod_stdout != NULL && INTERNAL_OPTION(private_loader)) {
            /* update the privately loaded libc's stdout _fileno. */
            set_stdfile_fileno(privmod_stdout, our_stdout);
        }
    }
    if (DYNAMO_OPTION(dup_stderr_on_close) && fd == STDERR) {
        our_stderr = fd_priv_dup(fd);
        if (our_stderr < 0) /* no private fd available */
            our_stderr = dup_syscall(fd);
        if (our_stderr >= 0)
            fd_mark_close_on_exec(our_stderr);
        fd_table_add(our_stderr, 0);
        LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
            "WARNING: app is closing stderr=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n",
            fd, our_stderr);
        if (privmod_stderr != NULL && INTERNAL_OPTION(private_loader)) {
            /* update the privately loaded libc's stderr _fileno. */
            set_stdfile_fileno(privmod_stderr, our_stderr);
        }
    }
    if (DYNAMO_OPTION(dup_stdin_on_close) && fd == STDIN) {
        our_stdin = fd_priv_dup(fd);
        if (our_stdin < 0) /* no private fd available */
            our_stdin = dup_syscall(fd);
        if (our_stdin >= 0)
            fd_mark_close_on_exec(our_stdin);
        fd_table_add(our_stdin, 0);
        LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
            "WARNING: app is closing stdin=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n",
            fd, our_stdin);
        if (privmod_stdin != NULL && INTERNAL_OPTION(private_loader)) {
            /* update the privately loaded libc's stdout _fileno. */
            set_stdfile_fileno(privmod_stdin, our_stdin);
        }
    }
    return true;
}

static bool
handle_close_pre(dcontext_t *dcontext)
{
    return handle_close_generic_pre(dcontext, (uint)sys_param(dcontext, 0),
                                    true /*set_return_val*/);
}

#ifdef SYS_close_range
static bool
handle_close_range_pre(dcontext_t *dcontext, file_t fd)
{
    return handle_close_generic_pre(dcontext, fd, false /*set_return_val*/);
}
#endif

/***************************************************************************/

/* Used to obtain the pc of the syscall instr itself when the dcontext dc
 * is currently in a syscall handler.
 * Alternatively for sysenter we could set app_sysenter_instr_addr for Linux.
 */
#define SYSCALL_PC(dc)                                                              \
    ((get_syscall_method() == SYSCALL_METHOD_INT ||                                 \
      get_syscall_method() == SYSCALL_METHOD_SYSCALL)                               \
         ? (ASSERT(SYSCALL_LENGTH == INT_LENGTH), POST_SYSCALL_PC(dc) - INT_LENGTH) \
         : (vsyscall_syscall_end_pc - SYSENTER_LENGTH))

static void
handle_exit(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool exit_process = false;

    if (dcontext->sys_num == SYSNUM_EXIT_PROCESS) {
        /* We can have multiple thread groups within the same address space.
         * We need to know whether this is the only group left.
         * FIXME: we can have races where new threads are created after our
         * check: we'll live with that for now, but the right approach is to
         * suspend all threads via synch_with_all_threads(), do the check,
         * and if exit_process then exit w/o resuming: though have to
         * coordinate lock access w/ cleanup_and_terminate.
         * Xref i#94.  Xref PR 541760.
         */
        process_id_t mypid = get_process_id();
        thread_record_t **threads;
        int num_threads, i;
        exit_process = true;
        d_r_mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num_threads);
        for (i = 0; i < num_threads; i++) {
            if (threads[i]->pid != mypid && !IS_CLIENT_THREAD(threads[i]->dcontext)) {
                exit_process = false;
                break;
            }
        }
        if (!exit_process) {
            /* We need to clean up the other threads in our group here. */
            thread_id_t myid = d_r_get_thread_id();
            priv_mcontext_t mcontext;
            DEBUG_DECLARE(thread_synch_result_t synch_res;)
            LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
                "SYS_exit_group %d not final group: %d cleaning up just "
                "threads in group\n",
                get_process_id(), d_r_get_thread_id());
            /* Set where we are to handle reciprocal syncs */
            copy_mcontext(mc, &mcontext);
            mc->pc = SYSCALL_PC(dcontext);
            for (i = 0; i < num_threads; i++) {
                if (threads[i]->id != myid && threads[i]->pid == mypid) {
                    /* See comments in dynamo_process_exit_cleanup(): we terminate
                     * to make cleanup easier, but may want to switch to shifting
                     * the target thread to a stack-free loop.
                     */
                    DEBUG_DECLARE(synch_res =)
                    synch_with_thread(
                        threads[i]->id, true /*block*/, true /*have initexit lock*/,
                        THREAD_SYNCH_VALID_MCONTEXT, THREAD_SYNCH_TERMINATED_AND_CLEANED,
                        THREAD_SYNCH_SUSPEND_FAILURE_IGNORE);
                    /* initexit lock may be released and re-acquired in course of
                     * doing the synch so we may have races where the thread
                     * exits on its own (or new threads appear): we'll live
                     * with those for now.
                     */
                    ASSERT(synch_res == THREAD_SYNCH_RESULT_SUCCESS);
                }
            }
            copy_mcontext(&mcontext, mc);
        }
        d_r_mutex_unlock(&thread_initexit_lock);
        global_heap_free(
            threads, num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    }

    if (is_last_app_thread() && !dynamo_exited) {
        LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
            "SYS_exit%s(%d) in final thread " TIDFMT " of " PIDFMT
            " => exiting DynamoRIO\n",
            (dcontext->sys_num == SYSNUM_EXIT_PROCESS) ? "_group" : "",
            MCXT_SYSNUM_REG(mc), d_r_get_thread_id(), get_process_id());
        /* we want to clean up even if not automatic startup! */
        automatic_startup = true;
        exit_process = true;
    } else {
        LOG(THREAD, LOG_TOP | LOG_THREADS | LOG_SYSCALLS, 1,
            "SYS_exit%s(%d) in thread " TIDFMT " of " PIDFMT " => cleaning up %s\n",
            (dcontext->sys_num == SYSNUM_EXIT_PROCESS) ? "_group" : "",
            MCXT_SYSNUM_REG(mc), d_r_get_thread_id(), get_process_id(),
            exit_process ? "process" : "thread");
    }
    KSTOP(num_exits_dir_syscall);

    block_cleanup_and_terminate(dcontext, MCXT_SYSNUM_REG(mc), sys_param(dcontext, 0),
                                sys_param(dcontext, 1), exit_process,
                                /* SYS_bsdthread_terminate has 2 more args */
                                sys_param(dcontext, 2), sys_param(dcontext, 3));
}

#if defined(LINUX) && defined(X86) /* XXX i#58: until we have Mac support */
static bool
os_set_app_thread_area(dcontext_t *dcontext, our_modify_ldt_t *user_desc)
{
#    ifdef X86
    int i;
    os_thread_data_t *ostd = dcontext->os_field;
    our_modify_ldt_t *desc = (our_modify_ldt_t *)ostd->app_thread_areas;

    if (user_desc->seg_not_present == 1) {
        /* find an empty one to update */
        for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
            if (desc[i].seg_not_present == 1)
                break;
        }
        if (i < GDT_NUM_TLS_SLOTS) {
            user_desc->entry_number = GDT_SELECTOR(i + tls_min_index());
            memcpy(&desc[i], user_desc, sizeof(*user_desc));
        } else
            return false;
    } else {
        /* If we used early injection, this might be ld.so trying to set up TLS.  We
         * direct the app to use the GDT entry we already set up for our private
         * libraries, but only the first time it requests TLS.
         */
        if (user_desc->entry_number == -1 && return_stolen_lib_tls_gdt) {
            d_r_mutex_lock(&set_thread_area_lock);
            if (return_stolen_lib_tls_gdt) {
                uint selector = read_thread_register(LIB_SEG_TLS);
                uint index = SELECTOR_INDEX(selector);
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                return_stolen_lib_tls_gdt = false;
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                user_desc->entry_number = index;
                LOG(GLOBAL, LOG_THREADS, 2,
                    "%s: directing app to use "
                    "selector 0x%x for first call to set_thread_area\n",
                    __FUNCTION__, selector);
            }
            d_r_mutex_unlock(&set_thread_area_lock);
        }

        /* update the specific one */
        i = user_desc->entry_number - tls_min_index();
        if (i < 0 || i >= GDT_NUM_TLS_SLOTS)
            return false;
        LOG(GLOBAL, LOG_THREADS, 2,
            "%s: change selector 0x%x base from " PFX " to " PFX "\n", __FUNCTION__,
            GDT_SELECTOR(user_desc->entry_number), desc[i].base_addr,
            user_desc->base_addr);
        memcpy(&desc[i], user_desc, sizeof(*user_desc));
    }
    /* if not conflict with dr's tls, perform the syscall */
    if (!INTERNAL_OPTION(private_loader) &&
        GDT_SELECTOR(user_desc->entry_number) != read_thread_register(SEG_TLS) &&
        GDT_SELECTOR(user_desc->entry_number) != read_thread_register(LIB_SEG_TLS))
        return false;
#    elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#    endif /* X86/ARM */
    return true;
}

static bool
os_get_app_thread_area(dcontext_t *dcontext, our_modify_ldt_t *user_desc)
{
#    ifdef X86
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    our_modify_ldt_t *desc = (our_modify_ldt_t *)ostd->app_thread_areas;
    int i = user_desc->entry_number - tls_min_index();
    if (i < 0 || i >= GDT_NUM_TLS_SLOTS)
        return false;
    if (desc[i].seg_not_present == 1)
        return false;
#    elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#    endif /* X86/ARM */
    return true;
}
#endif

/* This function is used for switch lib tls segment on creating thread.
 * We switch to app's lib tls seg before thread creation system call, i.e.
 * clone and vfork, and switch back to dr's lib tls seg after the system call.
 * They are only called on parent thread, not the child thread.
 * The child thread's tls is setup in os_tls_app_seg_init.
 */
/* XXX: It looks like the Linux kernel has some dependency on the segment
 * descriptor. If using dr's segment descriptor, the created thread will have
 * access violation for tls not being setup. However, it works fine if we switch
 * the descriptor to app's segment descriptor before creating the thread.
 * We should be able to remove this function later if we find the problem.
 */
static bool
os_switch_lib_tls(dcontext_t *dcontext, bool to_app)
{
    return os_switch_seg_to_context(dcontext, LIB_SEG_TLS, to_app);
}

#ifdef X86
/* dcontext can be NULL if !to_app */
static bool
os_switch_seg_to_base(dcontext_t *dcontext, os_local_state_t *os_tls, reg_id_t seg,
                      bool to_app, app_pc base)
{
    bool res = false;
    ASSERT(dcontext != NULL);
    ASSERT(IF_X86_ELSE((seg == SEG_FS || seg == SEG_GS),
                       (seg == DR_REG_TPIDRURW || DR_REG_TPIDRURO)));
    switch (os_tls->tls_type) {
#    if defined(X64) && !defined(MACOS)
    case TLS_TYPE_ARCH_PRCTL: {
        res = tls_set_fs_gs_segment_base(os_tls->tls_type, seg, base, NULL);
        ASSERT(res);
        LOG(GLOBAL, LOG_THREADS, 2,
            "%s %s: arch_prctl successful for thread " TIDFMT " base " PFX "\n",
            __FUNCTION__, to_app ? "to app" : "to DR", d_r_get_thread_id(), base);
        if (seg == SEG_TLS && base == NULL) {
            /* Set the selector to 0 so we don't think TLS is available. */
            /* FIXME i#107: Still assumes app isn't using SEG_TLS. */
            reg_t zero = 0;
            WRITE_DR_SEG(zero);
        }
        break;
    }
#    endif
    case TLS_TYPE_GDT: {
        our_modify_ldt_t desc;
        uint index;
        uint selector;
        if (to_app) {
            selector = os_tls->app_lib_tls_reg;
            index = SELECTOR_INDEX(selector);
        } else {
            index = (seg == LIB_SEG_TLS ? tls_priv_lib_index() : tls_dr_index());
            ASSERT(index != -1 && "TLS indices not initialized");
            selector = GDT_SELECTOR(index);
        }
        if (selector != 0) {
            if (to_app) {
                our_modify_ldt_t *areas =
                    ((os_thread_data_t *)dcontext->os_field)->app_thread_areas;
                ASSERT((index >= tls_min_index()) &&
                       ((index - tls_min_index()) <= GDT_NUM_TLS_SLOTS));
                desc = areas[index - tls_min_index()];
            } else {
                tls_init_descriptor(&desc, base, GDT_NO_SIZE_LIMIT, index);
            }
            res = tls_set_fs_gs_segment_base(os_tls->tls_type, seg, NULL, &desc);
            ASSERT(res);
        } else {
            /* For a selector of zero, we just reset the segment to zero.  We
             * don't need to call set_thread_area.
             */
            res = true; /* Indicate success. */
        }
        /* XXX i#2098: it's unsafe to call LOG here in between GDT and register changes */
        /* i558 update lib seg reg to enforce the segment changes */
        if (seg == SEG_TLS)
            WRITE_DR_SEG(selector);
        else
            WRITE_LIB_SEG(selector);
        LOG(THREAD, LOG_LOADER, 2, "%s: switching to %s, setting %s to 0x%x\n",
            __FUNCTION__, (to_app ? "app" : "dr"), reg_names[seg], selector);
        LOG(THREAD, LOG_LOADER, 2,
            "%s %s: set_thread_area successful for thread " TIDFMT " base " PFX "\n",
            __FUNCTION__, to_app ? "to app" : "to DR", d_r_get_thread_id(), base);
        break;
    }
    case TLS_TYPE_LDT: {
        uint index;
        uint selector;
        if (to_app) {
            selector = os_tls->app_lib_tls_reg;
            index = SELECTOR_INDEX(selector);
        } else {
            index = (seg == LIB_SEG_TLS ? tls_priv_lib_index() : tls_dr_index());
            ASSERT(index != -1 && "TLS indices not initialized");
            selector = LDT_SELECTOR(index);
        }
        LOG(THREAD, LOG_LOADER, 2, "%s: switching to %s, setting %s to 0x%x\n",
            __FUNCTION__, (to_app ? "app" : "dr"), reg_names[seg], selector);
        if (seg == SEG_TLS)
            WRITE_DR_SEG(selector);
        else
            WRITE_LIB_SEG(selector);
        LOG(THREAD, LOG_LOADER, 2,
            "%s %s: ldt selector swap successful for thread " TIDFMT "\n", __FUNCTION__,
            to_app ? "to app" : "to DR", d_r_get_thread_id());
        break;
    }
    default: ASSERT_NOT_REACHED(); return false;
    }
    ASSERT((!to_app && seg == SEG_TLS) ||
           BOOLS_MATCH(to_app, os_using_app_state(dcontext)));
    return res;
}

static bool
os_set_dr_tls_base(dcontext_t *dcontext, os_local_state_t *tls, byte *base)
{
    if (tls == NULL) {
        ASSERT(dcontext != NULL);
        tls = get_os_tls_from_dc(dcontext);
    }
    return os_switch_seg_to_base(dcontext, tls, SEG_TLS, false, base);
}
#endif /* X86 */

static bool
os_switch_seg_to_context(dcontext_t *dcontext, reg_id_t seg, bool to_app)
{
    os_local_state_t *os_tls = get_os_tls_from_dc(dcontext);
#ifdef X86
    app_pc base;
    /* we can only update the executing thread's segment (i#920) */
    ASSERT_MESSAGE(CHKLVL_ASSERTS + 1 /*expensive*/, "can only act on executing thread",
                   /* i#2089: a clone syscall, or when native, temporarily puts in
                    * invalid TLS, so we don't check get_thread_private_dcontext().
                    */
                   is_thread_tls_allocated() &&
                       dcontext->owning_thread == get_sys_thread_id());
    if (to_app) {
        base = os_get_app_tls_base(dcontext, seg);
    } else {
        base = os_get_priv_tls_base(dcontext, seg);
    }
    return os_switch_seg_to_base(dcontext, os_tls, seg, to_app, base);
#elif defined(AARCHXX)
    bool res = false;
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    ASSERT(INTERNAL_OPTION(private_loader));
    if (to_app) {
        /* We need to handle being called when we're already in the requested state. */
        ptr_uint_t cur_seg = read_thread_register(LIB_SEG_TLS);
        if ((void *)cur_seg == os_tls->app_lib_tls_base)
            return true;
        bool app_mem_valid = true;
        if (os_tls->app_lib_tls_base == NULL)
            app_mem_valid = false;
        else {
            uint prot;
            bool rc = get_memory_info(os_tls->app_lib_tls_base, NULL, NULL, &prot);
            /* Rule out a garbage value, which happens in our own test
             * common.allasm_aarch_isa.
             * Also rule out an unwritable region, which seems to happen on arm
             * where at process init the thread reg points at rodata in libc
             * until properly set to a writable mmap later.
             */
            if (!rc || !TESTALL(MEMPROT_READ | MEMPROT_WRITE, prot))
                app_mem_valid = false;
        }
        if (!app_mem_valid) {
            /* XXX i#1578: For pure-asm apps that do not use libc, the app may have no
             * thread register value.  For detach we would like to write a 0 back into
             * the thread register, but it complicates our exit code, which wants access
             * to DR's TLS between dynamo_thread_exit_common()'s call to
             * dynamo_thread_not_under_dynamo() and its call to
             * set_thread_private_dcontext(NULL).  For now we just leave our privlib
             * segment in there.  It seems rather unlikely to cause a problem: app code
             * is unlikely to read the thread register; it's going to assume it owns it
             * and will just blindly write to it.
             */
            return true;
        }
        /* On switching to app's TLS, we need put DR's TLS base into app's TLS
         * at the same offset so it can be loaded on entering code cache.
         * Otherwise, the context switch code on entering fcache will fault on
         * accessing DR's TLS.
         * The app's TLS slot value is stored into privlib's TLS slot for
         * later restore on switching back to privlib's TLS.
         */
        byte **priv_lib_tls_swap_slot =
            (byte **)(ostd->priv_lib_tls_base + DR_TLS_BASE_OFFSET);
        byte **app_lib_tls_swap_slot =
            (byte **)(os_tls->app_lib_tls_base + DR_TLS_BASE_OFFSET);
        LOG(THREAD, LOG_LOADER, 3,
            "%s: switching to app: app slot=&" PFX " *" PFX ", priv slot=&" PFX " *" PFX
            "\n",
            __FUNCTION__, app_lib_tls_swap_slot, *app_lib_tls_swap_slot,
            priv_lib_tls_swap_slot, *priv_lib_tls_swap_slot);
        byte *dr_tls_base = *priv_lib_tls_swap_slot;
        *priv_lib_tls_swap_slot = *app_lib_tls_swap_slot;
        *app_lib_tls_swap_slot = dr_tls_base;
        LOG(THREAD, LOG_LOADER, 2, "%s: switching to %s, setting coproc reg to 0x%x\n",
            __FUNCTION__, (to_app ? "app" : "dr"), os_tls->app_lib_tls_base);
        res = write_thread_register(os_tls->app_lib_tls_base);
    } else {
        /* We need to handle being called when we're already in the requested state. */
        ptr_uint_t cur_seg = read_thread_register(LIB_SEG_TLS);
        if ((void *)cur_seg == ostd->priv_lib_tls_base)
            return true;
        /* Restore the app's TLS slot that we used for storing DR's TLS base,
         * and put DR's TLS base back to privlib's TLS slot.
         */
        byte **priv_lib_tls_swap_slot =
            (byte **)(ostd->priv_lib_tls_base + DR_TLS_BASE_OFFSET);
        byte **app_lib_tls_swap_slot =
            (byte **)(os_tls->app_lib_tls_base + DR_TLS_BASE_OFFSET);
        byte *dr_tls_base = *app_lib_tls_swap_slot;
        LOG(THREAD, LOG_LOADER, 3,
            "%s: switching to DR: app slot=&" PFX " *" PFX ", priv slot=&" PFX " *" PFX
            "\n",
            __FUNCTION__, app_lib_tls_swap_slot, *app_lib_tls_swap_slot,
            priv_lib_tls_swap_slot, *priv_lib_tls_swap_slot);
        *app_lib_tls_swap_slot = *priv_lib_tls_swap_slot;
        *priv_lib_tls_swap_slot = dr_tls_base;
        LOG(THREAD, LOG_LOADER, 2, "%s: switching to %s, setting coproc reg to 0x%x\n",
            __FUNCTION__, (to_app ? "app" : "dr"), ostd->priv_lib_tls_base);
        res = write_thread_register(ostd->priv_lib_tls_base);
    }
    LOG(THREAD, LOG_LOADER, 2, "%s %s: set_tls swap success=%d for thread " TIDFMT "\n",
        __FUNCTION__, to_app ? "to app" : "to DR", res, d_r_get_thread_id());
    return res;
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
    /* Marking as unused to silence -Wunused-variable. */
    (void)os_tls;
    return false;
#endif /* X86/AARCHXX/RISCV64 */
}

#ifdef LINUX
static bool
handle_clone_pre(dcontext_t *dcontext)
{
    /* For the clone syscall, in /usr/src/linux/arch/i386/kernel/process.c
     * 32-bit params: flags, newsp, ptid, tls, ctid
     * 64-bit params: should be the same yet tls (for ARCH_SET_FS) is in r8?!?
     *   I don't see how sys_clone gets its special args: shouldn't it
     *   just get pt_regs as a "special system call"?
     *   sys_clone(unsigned long clone_flags, unsigned long newsp,
     *     void __user *parent_tid, void __user *child_tid, struct pt_regs *regs)
     */
    uint64_t flags;
    /* For the clone3 syscall, DR creates its own copy of clone_args for two
     * reasons: to ensure that the app-provided clone_args is readable
     * without any fault, and to avoid modifying the app's clone_args in the
     * is_thread_create_syscall case (see below).
     */
    clone3_syscall_args_t *dr_clone_args = NULL, *app_clone_args = NULL;
    uint app_clone_args_size = 0;
    if (dcontext->sys_num == SYS_clone3) {
        if (is_clone3_enosys) {
            /* We know that clone3 will return ENOSYS, so we skip the pre-syscall
             * handling and fail early.
             */
            LOG(THREAD, LOG_SYSCALLS, 2, "\treturning ENOSYS to app for clone3\n");
            set_failure_return_val(dcontext, ENOSYS);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            return false;
        }
        app_clone_args_size =
            (uint)sys_param(dcontext, SYSCALL_PARAM_CLONE3_CLONE_ARGS_SIZE);
        if (app_clone_args_size < CLONE_ARGS_SIZE_VER0) {
            LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EINVAL to app for clone3\n");
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            return false;
        }
        app_clone_args =
            (clone3_syscall_args_t *)sys_param(dcontext, SYSCALL_PARAM_CLONE3_CLONE_ARGS);
        /* Note that the struct clone_args being used by the app may have
         * less/more fields than DR's internal struct (clone3_syscall_args_t).
         * For creating DR's copy of the app's clone_args object, we need to
         * allocate as much space as specified by the app in the clone3
         * syscall's args.
         */
        dr_clone_args = (clone3_syscall_args_t *)heap_alloc(
            dcontext, app_clone_args_size HEAPACCT(ACCT_OTHER));
        if (!d_r_safe_read(app_clone_args, app_clone_args_size, dr_clone_args)) {
            LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EFAULT to app for clone3\n");
            set_failure_return_val(dcontext, EFAULT);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            heap_free(dcontext, dr_clone_args, app_clone_args_size HEAPACCT(ACCT_OTHER));
            return false;
        }
        flags = dr_clone_args->flags;

        /* Save for post_system_call */
        /* We need to save the pointer to the app's clone_args so that we can restore it
         * post-syscall.
         */
        dcontext->sys_param0 = (reg_t)app_clone_args;
        /* For freeing the allocated memory. */
        dcontext->sys_param1 = (reg_t)dr_clone_args;
        dcontext->sys_param2 = (reg_t)app_clone_args_size;
        /* clone3 flags are 64-bit even on 32-bit systems. So we need to split them across
         * two reg_t vars on 32-bit. We do it on 64-bit systems as well for simpler code.
         */
        dcontext->sys_param3 = (reg_t)(flags & CLONE3_FLAGS_4_BYTE_MASK);
        ASSERT((flags >> 32 & ~CLONE3_FLAGS_4_BYTE_MASK) == 0);
        dcontext->sys_param4 = (reg_t)((flags >> 32));
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: clone3 with args: flags = 0x" HEX64_FORMAT_STRING
            ", exit_signal = 0x" HEX64_FORMAT_STRING ", stack = 0x" HEX64_FORMAT_STRING
            ", stack_size = 0x" HEX64_FORMAT_STRING "\n",
            dr_clone_args->flags, dr_clone_args->exit_signal, dr_clone_args->stack,
            dr_clone_args->stack_size);
    } else {
        flags = (uint)sys_param(dcontext, 0);
        /* Save for post_system_call.
         * Unlike clone3, here the flags are 32-bit, so truncation is okay.
         */
        dcontext->sys_param0 = (reg_t)flags;
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: clone with args: flags = " PFX ", stack = " PFX
            ", tid_field_parent = " PFX ", tid_field_child = " PFX ", thread_ptr = " PFX
            "\n",
            sys_param(dcontext, 0), sys_param(dcontext, 1), sys_param(dcontext, 2),
            sys_param(dcontext, 3), sys_param(dcontext, 4));
    }
    handle_clone(dcontext, flags);
    if ((flags & CLONE_VM) == 0) {
        LOG(THREAD, LOG_SYSCALLS, 1, "\tWARNING: CLONE_VM not set!\n");
    }

    /* i#1010: If we have private fds open (usually logfiles), we should
     * clean those up before they get reused by a new thread.
     * XXX: Ideally we'd do this in fd_table_add(), but we can't acquire
     * thread_initexit_lock there.
     */
    cleanup_after_vfork_execve(dcontext);

    /* For thread creation clone syscalls a clone_record_t structure
     * containing the pc after the app's syscall instr and other data
     * (see i#27) is placed at the bottom of the dstack (which is allocated
     * by create_clone_record() - it also saves app stack and switches
     * to dstack).  xref i#149/PR 403015.
     * Note: This must be done after sys_param0 is set.
     */
    if (is_thread_create_syscall(dcontext, dr_clone_args)) {
        if (dcontext->sys_num == SYS_clone3) {
            /* create_clone_record modifies some fields in clone_args for the
             * clone3 syscall. Instead of reusing the app's copy of
             * clone_args and modifying it, we choose to use our own copy.
             * Under CLONE_VM, the parent and child threads have a pointer to
             * the same app clone_args. By using our own copy of clone_args
             * for the syscall, we obviate the need to restore the modified
             * fields in the app's copy after the syscall in either the parent
             * or the child thread, which can be racy under CLONE_VM as the
             * parent and/or child threads may need to access/modify it. By
             * using a copy instead, both parent and child threads only
             * need to restore their own SYSCALL_PARAM_CLONE3_CLONE_ARGS reg
             * to the pointer to the app's clone_args. It is saved in the
             * clone record for the child thread, and in sys_param0 for the
             * parent thread. The DR copy of clone_args is freed by the parent
             * thread in the post-syscall handling of clone3; as it is used
             * only by the parent thread, there is no use-after-free danger here.
             */
            ASSERT(app_clone_args != NULL && dr_clone_args != NULL);
            *sys_param_addr(dcontext, SYSCALL_PARAM_CLONE3_CLONE_ARGS) =
                (reg_t)dr_clone_args;
            /* The pointer to the app's clone_args was saved in sys_param0 above. */
            create_clone_record(dcontext, NULL, dr_clone_args, app_clone_args);
        } else {
            /* We replace the app-provided stack pointer with our own stack
             * pointer in create_clone_record. Save the original pointer so
             * that we can restore it post-syscall in the parent. The same is
             * restored in the child in restore_clone_param_from_clone_record.
             */
            dcontext->sys_param1 = sys_param(dcontext, SYSCALL_PARAM_CLONE_STACK);
            create_clone_record(dcontext,
                                sys_param_addr(dcontext, SYSCALL_PARAM_CLONE_STACK), NULL,
                                NULL);
        }
        os_clone_pre(dcontext);
        os_new_thread_pre();
    } else {
        /* This is really a fork. */
        if (dcontext->sys_num == SYS_clone3) {
            /* We free this memory before the actual fork, to avoid having to free
             * it in the parent *and* the child later.
             */
            ASSERT(app_clone_args_size == (uint)dcontext->sys_param2);
            ASSERT(dr_clone_args == (clone3_syscall_args_t *)dcontext->sys_param1);
            heap_free(dcontext, dr_clone_args, app_clone_args_size HEAPACCT(ACCT_OTHER));
            /* We do not need these anymore for the fork case. */
            dcontext->sys_param1 = 0;
            dcontext->sys_param2 = 0;
        }
        os_fork_pre(dcontext);
    }
    return true;
}
#endif

/* System call interception: put any special handling here
 * Arguments come from the pusha right before the call
 */

/* WARNING: flush_fragments_and_remove_region assumes that pre and post system
 * call handlers do not examine or modify fcache or its fragments in any
 * way except for calling flush_fragments_and_remove_region!
 */

/* WARNING: All registers are IN values, but NOT OUT values --
 * must set mcontext's register for that.
 */

/* Returns false if system call should NOT be executed (in which case,
 * post_system_call() will *not* be called!).
 * Returns true if system call should go ahead
 */
/* XXX: split out specific handlers into separate routines
 */
bool
pre_system_call(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool execute_syscall = true;
    dr_where_am_i_t old_whereami = dcontext->whereami;
    dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;
    /* FIXME We haven't yet done the work to detect which syscalls we
     * can determine a priori will fail. Once we do, we will set the
     * expect_last_syscall_to_fail to true for those case, and can
     * confirm in post_system_call() that the syscall failed as
     * expected.
     */
    DODEBUG(dcontext->expect_last_syscall_to_fail = false;);

    /* save key register values for post_system_call (they get clobbered
     * in syscall itself)
     */
    dcontext->sys_num = os_normalized_sysnum((int)MCXT_SYSNUM_REG(mc), NULL, dcontext);

    RSTATS_INC(pre_syscall);
    DOSTATS({
        if (ignorable_system_call_normalized(dcontext->sys_num))
            STATS_INC(pre_syscall_ignorable);
    });
    LOG(THREAD, LOG_SYSCALLS, 2, "system call %d\n", dcontext->sys_num);

#if defined(LINUX) && defined(X86)
    /* PR 313715: If we fail to hook the vsyscall page (xref PR 212570, PR 288330)
     * we fall back on int, but we have to tweak syscall param #5 (ebp)
     * Once we have PR 288330 we can remove this.
     */
    if (should_syscall_method_be_sysenter() && !dcontext->sys_was_int) {
        dcontext->sys_xbp = mc->xbp;
        /* not using SAFE_READ due to performance concerns (we do this for
         * every single system call on systems where we can't hook vsyscall!)
         */
        TRY_EXCEPT(
            dcontext, /* try */ { mc->xbp = *(reg_t *)mc->xsp; }, /* except */
            {
                ASSERT_NOT_REACHED();
                mc->xbp = 0;
            });
    }
#endif

    switch (dcontext->sys_num) {

    case SYSNUM_EXIT_PROCESS:
#if defined(LINUX) && VMX86_SERVER
        if (os_in_vmkernel_32bit()) {
            /* on esx 3.5 => ENOSYS, so wait for SYS_exit */
            LOG(THREAD, LOG_SYSCALLS, 2, "on esx35 => ignoring exitgroup\n");
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            break;
        }
#endif
        /* fall-through */
    case SYSNUM_EXIT_THREAD: {
        handle_exit(dcontext);
        break;
    }

    /****************************************************************************/
    /* MEMORY REGIONS */

#if defined(LINUX) && !defined(X64) && !defined(ARM)
    case SYS_mmap: {
        /* in /usr/src/linux/arch/i386/kernel/sys_i386.c:
           asmlinkage int old_mmap(struct mmap_arg_struct_t *arg)
         */
        mmap_arg_struct_t *arg = (mmap_arg_struct_t *)sys_param(dcontext, 0);
        mmap_arg_struct_t arg_buf;
        if (d_r_safe_read(arg, sizeof(mmap_arg_struct_t), &arg_buf)) {
            void *addr = (void *)arg->addr;
            size_t len = (size_t)arg->len;
            uint prot = (uint)arg->prot;
            LOG(THREAD, LOG_SYSCALLS, 2,
                "syscall: mmap addr=" PFX " size=" PIFX " prot=0x%x"
                " flags=" PIFX " offset=" PIFX " fd=%d\n",
                addr, len, prot, arg->flags, arg->offset, arg->fd);
            /* Check for overlap with existing code or patch-proof regions */
            if (addr != NULL &&
                !app_memory_pre_alloc(dcontext, addr, len, osprot_to_memprot(prot),
                                      !TEST(MAP_FIXED, arg->flags),
                                      false /*we'll update in post*/,
                                      false /*unknown*/)) {
                /* Rather than failing or skipping the syscall we'd like to just
                 * remove the hint -- but we don't want to write to app memory, so
                 * we do fail.  We could set up our own mmap_arg_struct_t but
                 * we'd need dedicate per-thread storage, and SYS_mmap is obsolete.
                 */
                execute_syscall = false;
                set_failure_return_val(dcontext, ENOMEM);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                break;
            }
        }
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t)arg;
        break;
    }
#endif
    case IF_MACOS_ELSE(SYS_mmap, IF_X64_ELSE(SYS_mmap, SYS_mmap2)): {
        /* in /usr/src/linux/arch/i386/kernel/sys_i386.c:
           asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
             unsigned long prot, unsigned long flags,
             unsigned long fd, unsigned long pgoff)
         */
        void *addr = (void *)sys_param(dcontext, 0);
        size_t len = (size_t)sys_param(dcontext, 1);
        uint prot = (uint)sys_param(dcontext, 2);
        uint flags = (uint)sys_param(dcontext, 3);
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: mmap2 addr=" PFX " size=" PIFX " prot=0x%x"
            " flags=" PIFX " offset=" PIFX " fd=%d\n",
            addr, len, prot, flags, sys_param(dcontext, 5), sys_param(dcontext, 4));
        /* Check for overlap with existing code or patch-proof regions */
        /* Try to see whether it's an image, though we can't tell for addr==NULL
         * (typical for 1st mmap).
         */
        bool image = addr != NULL && !TEST(MAP_ANONYMOUS, flags) &&
            mmap_check_for_module_overlap(addr, len, TEST(PROT_READ, prot), 0, true);
        if (addr != NULL &&
            !app_memory_pre_alloc(dcontext, addr, len, osprot_to_memprot(prot),
                                  !TEST(MAP_FIXED, flags), false /*we'll update in post*/,
                                  image /*best estimate*/)) {
            if (!TEST(MAP_FIXED, flags)) {
                /* Rather than failing or skipping the syscall we just remove
                 * the hint which should eliminate any overlap.
                 */
                *sys_param_addr(dcontext, 0) = 0;
            } else {
                execute_syscall = false;
                set_failure_return_val(dcontext, ENOMEM);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                break;
            }
        }
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t)addr;
        dcontext->sys_param1 = len;
        dcontext->sys_param2 = prot;
        dcontext->sys_param3 = flags;
        break;
    }
    /* must flush stale fragments when we see munmap/mremap */
    case SYS_munmap: {
        /* in /usr/src/linux/mm/mmap.c:
           asmlinkage long sys_munmap(unsigned long addr, uint len)
         */
        app_pc addr = (void *)sys_param(dcontext, 0);
        size_t len = (size_t)sys_param(dcontext, 1);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: munmap addr=" PFX " size=" PFX "\n", addr,
            len);
        RSTATS_INC(num_app_munmaps);
        /* FIXME addr is supposed to be on a page boundary so we
         * could detect that condition here and set
         * expect_last_syscall_to_fail.
         */
        /* save params in case an undo is needed in post_system_call */
        dcontext->sys_param0 = (reg_t)addr;
        dcontext->sys_param1 = len;
        /* We assume that the unmap will succeed and so are conservative
         * and remove the region from exec areas and flush all fragments
         * prior to issuing the syscall. If the unmap fails, we try to
         * recover in post_system_call() by re-adding the region. This
         * approach has its shortcomings -- see comments below in
         * post_system_call().
         */
        /* Check for unmapping a module. */
        os_get_module_info_lock();
        if (module_overlaps(addr, len)) {
            /* FIXME - handle unmapping more than one module at once, or only unmapping
             * part of a module (for which case should adjust view size? or treat as full
             * unmap?). Theoretical for now as we haven't seen this. */
            module_area_t *ma = module_pc_lookup(addr);
            ASSERT_CURIOSITY(ma != NULL);
            ASSERT_CURIOSITY(addr == ma->start);
            /* XREF 307599 on rounding module end to the next PAGE boundary */
            ASSERT_CURIOSITY((app_pc)ALIGN_FORWARD(addr + len, PAGE_SIZE) == ma->end);
            os_get_module_info_unlock();
            /* i#210:
             * we only think a module is removed if its first memory region
             * is unloaded (unmapped).
             * XREF i#160 to fix the real problem of handling module splitting.
             */
            if (ma != NULL && ma->start == addr)
                module_list_remove(addr, ALIGN_FORWARD(len, PAGE_SIZE));
        } else
            os_get_module_info_unlock();
        app_memory_deallocation(dcontext, (app_pc)addr, len,
                                false /* don't own thread_initexit_lock */,
                                true /* image, FIXME: though not necessarily */);
        /* FIXME: case 4983 use is_elf_so_header() */
#ifndef HAVE_MEMINFO_QUERY
        memcache_lock();
        memcache_remove(addr, addr + len);
        memcache_unlock();
#endif
        break;
    }
#ifdef LINUX
    case SYS_mremap: {
        /* in /usr/src/linux/mm/mmap.c:
           asmlinkage unsigned long sys_mremap(unsigned long addr,
             unsigned long old_len, unsigned long new_len,
             unsigned long flags, unsigned long new_addr)
        */
        dr_mem_info_t info;
        app_pc addr = (void *)sys_param(dcontext, 0);
        size_t old_len = (size_t)sys_param(dcontext, 1);
        size_t new_len = (size_t)sys_param(dcontext, 2);
        DEBUG_DECLARE(bool ok;)
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: mremap addr=" PFX " size=" PFX "\n", addr,
            old_len);
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t)addr;
        dcontext->sys_param1 = old_len;
        dcontext->sys_param2 = new_len;
        /* i#173
         * we need memory type and prot to set the
         * new memory region in the post_system_call
         */
        DEBUG_DECLARE(ok =)
        query_memory_ex(addr, &info);
        ASSERT(ok);
        dcontext->sys_param3 = info.prot;
        dcontext->sys_param4 = info.type;
        DOCHECK(1, {
            /* we don't expect to see remappings of modules */
            os_get_module_info_lock();
            ASSERT_CURIOSITY(!module_overlaps(addr, old_len));
            os_get_module_info_unlock();
        });
        break;
    }
#endif
    case SYS_mprotect: {
        /* in /usr/src/linux/mm/mprotect.c:
           asmlinkage long sys_mprotect(unsigned long start, uint len,
           unsigned long prot)
        */
        uint res;
        DEBUG_DECLARE(size_t size;)
        app_pc addr = (void *)sys_param(dcontext, 0);
        size_t len = (size_t)sys_param(dcontext, 1);
        uint prot = (uint)sys_param(dcontext, 2);
        uint old_memprot = MEMPROT_NONE, new_memprot;
        bool exists UNUSED = true;
        /* save params in case an undo is needed in post_system_call */
        dcontext->sys_param0 = (reg_t)addr;
        dcontext->sys_param1 = len;
        dcontext->sys_param2 = prot;
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: mprotect addr=" PFX " size=" PFX " prot=%s\n", addr, len,
            memprot_string(osprot_to_memprot(prot)));

        if (!get_memory_info(addr, NULL, IF_DEBUG_ELSE(&size, NULL), &old_memprot)) {
            exists = false;
            /* Xref PR 413109, PR 410921: if the start, or any page, is not mapped,
             * this should fail with ENOMEM.  We used to force-fail it to avoid
             * asserts in our own allmem update code, but there are cases where a
             * seemingly unmapped page succeeds (i#1912: next page of grows-down
             * initial stack).  Thus we let it go through.
             */
            LOG(THREAD, LOG_SYSCALLS, 2,
                "\t" PFX " isn't mapped: probably mprotect will fail\n", addr);
        } else {
            /* If mprotect region spans beyond the end of the vmarea then it
             * spans 2 or more vmareas with dissimilar protection (xref
             * PR 410921) or has unallocated regions in between (PR 413109).
             */
            DOCHECK(1, dcontext->mprot_multi_areas = len > size ? true : false;);
        }

        new_memprot = osprot_to_memprot(prot) |
            /* mprotect won't change meta flags */
            (old_memprot & MEMPROT_META_FLAGS);
        res = app_memory_protection_change(dcontext, addr, len, new_memprot, &new_memprot,
                                           NULL, false /*!image*/);
        if (res != DO_APP_MEM_PROT_CHANGE) {
            if (res == FAIL_APP_MEM_PROT_CHANGE) {
                ASSERT_NOT_IMPLEMENTED(false); /* return code? */
            } else {
                ASSERT_NOT_IMPLEMENTED(res != SUBSET_APP_MEM_PROT_CHANGE);
                ASSERT_NOT_REACHED();
            }
            execute_syscall = false;
        } else {
            /* FIXME Store state for undo if the syscall fails. */
            IF_NO_MEMQUERY(memcache_update_locked(addr, addr + len, new_memprot,
                                                  -1 /*type unchanged*/, exists));
        }
        break;
    }
#ifdef ANDROID
    case SYS_prctl:
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
        dcontext->sys_param3 = sys_param(dcontext, 3);
        dcontext->sys_param4 = sys_param(dcontext, 4);
        break;
#endif
#ifdef LINUX
    case SYS_brk: {
        if (DYNAMO_OPTION(emulate_brk)) {
            /* i#1004: emulate brk via a separate mmap */
            byte *new_val = (byte *)sys_param(dcontext, 0);
            byte *res = emulate_app_brk(dcontext, new_val);
            execute_syscall = false;
            /* SYS_brk returns old brk on failure */
            set_success_return_val(dcontext, (reg_t)res);
        } else {
            /* i#91/PR 396352: need to watch SYS_brk to maintain all_memory_areas.
             * We store the old break in the param1 slot.
             */
            DODEBUG(dcontext->sys_param0 = (reg_t)sys_param(dcontext, 0););
            dcontext->sys_param1 = dynamorio_syscall(SYS_brk, 1, 0);
        }
        break;
    }
#    ifdef SYS_uselib
    case SYS_uselib: {
        /* Used to get the kernel to load a share library (legacy system call).
         * Was primarily used when statically linking to dynamically loaded shared
         * libraries that were loaded at known locations.  Shouldn't be used by
         * applications using the dynamic loader (ld) which is currently the only
         * way we can inject so we don't expect to see this.  PR 307621. */
        ASSERT_NOT_IMPLEMENTED(false);
        break;
    }
#    endif
#endif

    /****************************************************************************/
    /* SPAWNING */

#ifdef LINUX
    case SYS_clone3:
    case SYS_clone: execute_syscall = handle_clone_pre(dcontext); break;
#elif defined(MACOS)
    case SYS_bsdthread_create: {
        /* XXX i#1403: we need earlier injection to intercept
         * bsdthread_register in order to capture workqueue threads.
         * For now we settle for intercepting bsd threads at the user thread func.
         * We miss a little user-mode code but this is enough to get started.
         */
        app_pc func = (app_pc)sys_param(dcontext, 0);
        void *func_arg = (void *)sys_param(dcontext, 1);
        void *clone_rec;
        LOG(THREAD, LOG_SYSCALLS, 1,
            "bsdthread_create: thread func " PFX ", arg " PFX "\n", func, func_arg);
        handle_clone(dcontext, CLONE_THREAD | CLONE_VM | CLONE_SIGHAND | SIGCHLD);
        clone_rec = create_clone_record(dcontext, NULL, func, func_arg);
        dcontext->sys_param0 = (reg_t)func;
        dcontext->sys_param1 = (reg_t)func_arg;
        *sys_param_addr(dcontext, 0) = (reg_t)new_bsdthread_intercept;
        *sys_param_addr(dcontext, 1) = (reg_t)clone_rec;
        os_new_thread_pre();
        break;
    }
    case SYS_posix_spawn: {
        /* FIXME i#1644: monitor this call which can be fork or exec */
        ASSERT_NOT_IMPLEMENTED(false);
        break;
    }
#endif

#ifdef SYS_vfork
    case SYS_vfork: {
        /* treat as if sys_clone with flags just as sys_vfork does */
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        uint flags = CLONE_VFORK | CLONE_VM | SIGCHLD;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork\n");
        handle_clone(dcontext, flags);
        cleanup_after_vfork_execve(dcontext);

        /* save for post_system_call, treated as if SYS_clone */
        dcontext->sys_param0 = (reg_t)flags;

        /* vfork has the same needs as clone.  Pass info via a clone_record_t
         * structure to child.  See SYS_clone for info about i#149/PR 403015.
         */
        IF_LINUX(ASSERT(is_thread_create_syscall(dcontext, NULL)));
        dcontext->sys_param1 = mc->xsp; /* for restoring in parent */
#    ifdef MACOS
        create_clone_record(dcontext, (reg_t *)&mc->xsp, NULL, NULL);
#    else
        create_clone_record(dcontext, (reg_t *)&mc->xsp /*child uses parent sp*/, NULL,
                            NULL);
#    endif
        os_clone_pre(dcontext);
        os_new_thread_pre();
        break;
    }
#endif

#ifdef SYS_fork
    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork\n");
        os_fork_pre(dcontext);
        break;
    }
#endif

    case SYS_execve: {
        int ret = handle_execve(dcontext);
        if (ret != 0) {
            execute_syscall = false;
            set_failure_return_val(dcontext, ret);
        }
        break;
    }

        /****************************************************************************/
        /* SIGNALS */

    case IF_MACOS_ELSE(SYS_sigaction, SYS_rt_sigaction): { /* 174 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigaction(int sig, const struct sigaction *act,
             struct sigaction *oact, size_t sigsetsize)
         */
        int sig = (int)sys_param(dcontext, 0);
        const kernel_sigaction_t *act =
            (const kernel_sigaction_t *)sys_param(dcontext, 1);
        prev_sigaction_t *oact = (prev_sigaction_t *)sys_param(dcontext, 2);
        size_t sigsetsize = (size_t)
            /* On Mac there is no size arg (but it doesn't use old sigaction, so
             * closer to rt_ than non-rt_ below).
             */
            IF_MACOS_ELSE(sizeof(kernel_sigset_t), sys_param(dcontext, 3));
        uint res;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: %ssigaction %d " PFX " " PFX " %d\n",
            IF_MACOS_ELSE("", "rt_"), sig, act, oact, sigsetsize);
        /* post_syscall does some work as well */
        dcontext->sys_param0 = (reg_t)sig;
        dcontext->sys_param1 = (reg_t)act;
        dcontext->sys_param2 = (reg_t)oact;
        dcontext->sys_param3 = (reg_t)sigsetsize;
        execute_syscall = handle_sigaction(dcontext, sig, act, oact, sigsetsize, &res);
        if (!execute_syscall) {
            LOG(THREAD, LOG_SYSCALLS, 2, "sigaction emulation => %d\n", -res);
            if (res == 0)
                set_success_return_val(dcontext, 0);
            else
                set_failure_return_val(dcontext, res);
        }
        break;
    }
#if defined(LINUX) && !defined(X64)
    case SYS_sigaction: { /* 67 */
        /* sys_sigaction(int sig, const struct old_sigaction *act,
         *               struct old_sigaction *oact)
         */
        int sig = (int)sys_param(dcontext, 0);
        const old_sigaction_t *act = (const old_sigaction_t *)sys_param(dcontext, 1);
        old_sigaction_t *oact = (old_sigaction_t *)sys_param(dcontext, 2);
        uint res;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: sigaction %d " PFX " " PFX "\n", sig, act,
            oact);
        dcontext->sys_param0 = (reg_t)sig;
        dcontext->sys_param1 = (reg_t)act;
        dcontext->sys_param2 = (reg_t)oact;
        execute_syscall = handle_old_sigaction(dcontext, sig, act, oact, &res);
        if (!execute_syscall) {
            LOG(THREAD, LOG_SYSCALLS, 2, "sigaction emulation => %d\n", -res);
            if (res == 0)
                set_success_return_val(dcontext, 0);
            else
                set_failure_return_val(dcontext, res);
        }
        break;
    }
#endif
#if defined(LINUX) && !defined(X64)
    case SYS_sigreturn: { /* 119 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int sys_sigreturn(unsigned long __unused)
         */
        execute_syscall = handle_sigreturn(dcontext, false);
        /* app will not expect syscall to return, so when handle_sigreturn
         * returns false it always redirects the context, and thus no
         * need to set return val here.
         */
        break;
    }
#endif
#ifdef LINUX
    case SYS_rt_sigreturn: { /* 173 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int sys_rt_sigreturn(unsigned long __unused)
         */
        execute_syscall = handle_sigreturn(dcontext, true);
        /* see comment for SYS_sigreturn on return val */
        break;
    }
#endif
#ifdef MACOS
    case SYS_sigreturn: {
        /* int sigreturn(struct ucontext *uctx, int infostyle) */
        execute_syscall = handle_sigreturn(dcontext, (void *)sys_param(dcontext, 0),
                                           (int)sys_param(dcontext, 1));
        /* see comment for SYS_sigreturn on return val */
        break;
    }
#endif
    case SYS_sigaltstack: { /* 186 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int
           sys_sigaltstack(const stack_t *uss, stack_t *uoss)
        */
        const stack_t *uss = (const stack_t *)sys_param(dcontext, 0);
        stack_t *uoss = (stack_t *)sys_param(dcontext, 1);
        uint res;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: sigaltstack " PFX " " PFX "\n", uss, uoss);
        execute_syscall =
            handle_sigaltstack(dcontext, uss, uoss, get_mcontext(dcontext)->xsp, &res);
        if (!execute_syscall) {
            LOG(THREAD, LOG_SYSCALLS, 2, "sigaltstack emulation => %d\n", -res);
            if (res == 0)
                set_success_return_val(dcontext, res);
            else
                set_failure_return_val(dcontext, res);
        }
        break;
    }
    case IF_MACOS_ELSE(SYS_sigprocmask, SYS_rt_sigprocmask): { /* 175 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset,
             size_t sigsetsize)
         */
        /* we also need access to the params in post_system_call */
        uint error_code = 0;
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
        /* SYS_sigprocmask on MacOS does not have a size arg. So we use the
         * kernel_sigset_t size instead.
         */
        size_t sigsetsize =
            (size_t)IF_MACOS_ELSE(sizeof(kernel_sigset_t), sys_param(dcontext, 3));
        dcontext->sys_param3 = (reg_t)sigsetsize;
        execute_syscall = handle_sigprocmask(dcontext, (int)sys_param(dcontext, 0),
                                             (kernel_sigset_t *)sys_param(dcontext, 1),
                                             (kernel_sigset_t *)sys_param(dcontext, 2),
                                             sigsetsize, &error_code);
        if (!execute_syscall) {
            if (error_code == 0)
                set_success_return_val(dcontext, 0);
            else
                set_failure_return_val(dcontext, error_code);
        }
        break;
    }
#ifdef MACOS
    case SYS_sigsuspend_nocancel:
#endif
    case IF_MACOS_ELSE(SYS_sigsuspend, SYS_rt_sigsuspend): { /* 179 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage int
           sys_rt_sigsuspend(sigset_t *unewset, size_t sigsetsize)
         */
        handle_sigsuspend(dcontext, (kernel_sigset_t *)sys_param(dcontext, 0),
                          (size_t)sys_param(dcontext, 1));
        break;
    }
#ifdef LINUX
#    ifdef SYS_signalfd
    case SYS_signalfd: /* 282/321 */
#    endif
    case SYS_signalfd4: { /* 289 */
        /* int signalfd (int fd, const sigset_t *mask, size_t sizemask) */
        /* int signalfd4(int fd, const sigset_t *mask, size_t sizemask, int flags) */
        ptr_int_t new_result;
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
#    ifdef SYS_signalfd
        if (dcontext->sys_num == SYS_signalfd)
            dcontext->sys_param3 = 0;
        else
#    endif
            dcontext->sys_param3 = sys_param(dcontext, 3);
        new_result = handle_pre_signalfd(
            dcontext, (int)dcontext->sys_param0, (kernel_sigset_t *)dcontext->sys_param1,
            (size_t)dcontext->sys_param2, (int)dcontext->sys_param3);
        execute_syscall = false;
        /* since non-Mac, we can use this even if the call failed */
        set_success_return_val(dcontext, new_result);
        break;
    }
#endif
    case SYS_kill: { /* 37 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_kill(int pid, int sig)
         */
        pid_t pid = (pid_t)sys_param(dcontext, 0);
        uint sig = (uint)sys_param(dcontext, 1);
        LOG(GLOBAL, LOG_TOP | LOG_SYSCALLS, 2,
            "thread " TIDFMT " sending signal %d to pid " PIDFMT "\n",
            d_r_get_thread_id(), sig, pid);
        /* We check whether targeting this process or this process group */
        if (pid == get_process_id() || pid == 0 || pid == -get_process_group_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#if defined(SYS_tkill)
    case SYS_tkill: { /* 238 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_tkill(int pid, int sig)
         */
        pid_t tid = (pid_t)sys_param(dcontext, 0);
        uint sig = (uint)sys_param(dcontext, 1);
        LOG(GLOBAL, LOG_TOP | LOG_SYSCALLS, 2,
            "thread " TIDFMT " sending signal %d to tid %d\n", d_r_get_thread_id(), sig,
            tid);
        if (tid == d_r_get_thread_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#endif
#if defined(SYS_tgkill)
    case SYS_tgkill: { /* 270 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_tgkill(int tgid, int pid, int sig)
         */
        pid_t tgid = (pid_t)sys_param(dcontext, 0);
        pid_t tid = (pid_t)sys_param(dcontext, 1);
        uint sig = (uint)sys_param(dcontext, 2);
        LOG(GLOBAL, LOG_TOP | LOG_SYSCALLS, 2,
            "thread " TIDFMT " sending signal %d to tid %d tgid %d\n",
            d_r_get_thread_id(), sig, tid, tgid);
        /* some kernels support -1 values:
         +   tgkill(-1, tid, sig)  == tkill(tid, sig)
         *   tgkill(tgid, -1, sig) == kill(tgid, sig)
         * the 2nd was proposed but is not in 2.6.20 so I'm ignoring it, since
         * I don't want to kill the thread when the signal is never sent!
         * FIXME: the 1st is in my tkill manpage, but not my 2.6.20 kernel sources!
         */
        if ((tgid == -1 || tgid == get_process_id()) && tid == d_r_get_thread_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#endif
    case SYS_setitimer: /* 104 */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
        handle_pre_setitimer(dcontext, (int)sys_param(dcontext, 0),
                             (const struct itimerval *)sys_param(dcontext, 1),
                             (struct itimerval *)sys_param(dcontext, 2));
        break;
    case SYS_getitimer: /* 105 */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        break;
#if defined(LINUX) && defined(X86)
    case SYS_alarm: /* 27 on x86 and 37 on x64 */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        handle_pre_alarm(dcontext, (unsigned int)dcontext->sys_param0);
        break;
#endif
#if 0
#    ifndef X64
    case SYS_signal: {         /* 48 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage unsigned long
           sys_signal(int sig, __sighandler_t handler)
         */
        break;
    }
    case SYS_sigsuspend: {     /* 72 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int
           sys_sigsuspend(int history0, int history1, old_sigset_t mask)
         */
        break;
    }
    case SYS_sigprocmask: {    /* 126 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_sigprocmask(int how, old_sigset_t *set, old_sigset_t *oset)
         */
        break;
    }
#    endif
#else
        /* until we've implemented them, keep down here to get warning: */
#    if defined(LINUX) && !defined(X64)
#        ifndef ARM
    case SYS_signal:
#        endif
    case SYS_sigsuspend:
    case SYS_sigprocmask:
#    endif
#endif

#if defined(LINUX) && !defined(X64)
    case SYS_sigpending: /* 73 */
#    ifndef ARM
    case SYS_sgetmask: /* 68 */
    case SYS_ssetmask: /* 69 */
#    endif
#endif
#ifdef LINUX
#    ifdef SYS_rt_sigtimedwait_time64
    case SYS_rt_sigtimedwait_time64: /* 421 */
#    endif
    case SYS_rt_sigtimedwait: /* 177 */
    case SYS_rt_sigqueueinfo: /* 178 */
    case SYS_rt_tgsigqueueinfo:
#endif
    case IF_MACOS_ELSE(SYS_sigpending, SYS_rt_sigpending): { /* 176 */
        /* FIXME i#92: handle all of these syscalls! */
        LOG(THREAD, LOG_ASYNCH | LOG_SYSCALLS, 1,
            "WARNING: unhandled signal system call %d\n", dcontext->sys_num);
        SYSLOG_INTERNAL_WARNING_ONCE("unhandled signal system call %d",
                                     dcontext->sys_num);
        break;
    }
#ifdef LINUX
#    ifdef SYS_ppoll_time64
    case SYS_ppoll_time64:
#    endif
    case SYS_ppoll: {
        kernel_sigset_t *sigmask = (kernel_sigset_t *)sys_param(dcontext, 3);
        dcontext->sys_param3 = (reg_t)sigmask;
        if (sigmask == NULL)
            break;
        size_t sizemask = (size_t)sys_param(dcontext, 4);
        /* The original app's sigmask parameter is now NULL effectively making the syscall
         * a non p* version, and the mask's semantics are emulated by DR instead.
         */
        set_syscall_param(dcontext, 3, (reg_t)NULL);
        bool sig_pending = false;
        if (!handle_pre_extended_syscall_sigmasks(dcontext, sigmask, sizemask,
                                                  &sig_pending)) {
            /* In old kernels with sizeof(kernel_sigset_t) != sizemask, we're forcing
             * failure. We're already violating app transparency in other places in DR.
             */
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        if (sig_pending) {
            /* If there had been pending signals, we revert re-writing the app's
             * parameter, but we leave the modified signal mask.
             */
            set_syscall_param(dcontext, 3, dcontext->sys_param3);
            set_failure_return_val(dcontext, EINTR);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }
#    ifdef SYS_pselect6_time64
    case SYS_pselect6_time64:
#    endif
    case SYS_pselect6: {
        typedef struct {
            kernel_sigset_t *sigmask;
            size_t sizemask;
        } data_t;
        dcontext->sys_param3 = sys_param(dcontext, 5);
        data_t *data_param = (data_t *)dcontext->sys_param3;
        data_t data;
        if (data_param == NULL) {
            /* The kernel does not consider a NULL 6th+7th-args struct to be an error but
             * just a NULL sigmask.
             */
            dcontext->sys_param4 = (reg_t)NULL;
            break;
        }
        /* Refer to comments in SYS_ppoll above. Taking extra steps here due to struct
         * argument in pselect6.
         */
        if (!d_r_safe_read(data_param, sizeof(data), &data)) {
            LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EFAULT to app for pselect6\n");
            set_failure_return_val(dcontext, EFAULT);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
            break;
        }
        dcontext->sys_param4 = (reg_t)data.sigmask;
        if (data.sigmask == NULL)
            break;
        kernel_sigset_t *nullsigmaskptr = NULL;
        if (!safe_write_ex((void *)&data_param->sigmask, sizeof(data_param->sigmask),
                           &nullsigmaskptr, NULL)) {
            set_failure_return_val(dcontext, EFAULT);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
            break;
        }
        bool sig_pending = false;
        if (!handle_pre_extended_syscall_sigmasks(dcontext, data.sigmask, data.sizemask,
                                                  &sig_pending)) {
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        if (sig_pending) {
            if (!safe_write_ex((void *)&data_param->sigmask, sizeof(data_param->sigmask),
                               &dcontext->sys_param4, NULL)) {
                set_failure_return_val(dcontext, EFAULT);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                execute_syscall = false;
                break;
            }
            set_failure_return_val(dcontext, EINTR);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }
    case SYS_epoll_pwait: {
        kernel_sigset_t *sigmask = (kernel_sigset_t *)sys_param(dcontext, 4);
        dcontext->sys_param4 = (reg_t)sigmask;
        if (sigmask == NULL)
            break;
        size_t sizemask = (size_t)sys_param(dcontext, 5);
        /* Refer to comments in SYS_ppoll above. */
        set_syscall_param(dcontext, 4, (reg_t)NULL);
        bool sig_pending = false;
        if (!handle_pre_extended_syscall_sigmasks(dcontext, sigmask, sizemask,
                                                  &sig_pending)) {
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        if (sig_pending) {
            set_syscall_param(dcontext, 4, dcontext->sys_param4);
            set_failure_return_val(dcontext, EINTR);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }
#endif

    /****************************************************************************/
    /* FILES */
    /* prevent app from closing our files or opening a new file in our fd space.
     * it's not worth monitoring all syscalls that take in fds from affecting ours.
     */

#ifdef MACOS
    case SYS_close_nocancel:
#endif
#ifdef SYS_close_range
    case SYS_close_range: {
        /* client.file_io indeed tests this for all arch, but it hasn't yet been
         * run on an AArchXX machine that has close_range available.
         */
        IF_AARCHXX(ASSERT_NOT_TESTED());
        uint first_fd = sys_param(dcontext, 0), last_fd = sys_param(dcontext, 1);
        uint flags = sys_param(dcontext, 2);
        bool is_cloexec = TEST(CLOSE_RANGE_CLOEXEC, flags);
        if (is_cloexec) {
            /* client.file_io has a test for CLOSE_RANGE_CLOEXEC, but it hasn't been
             * verified on a system with kernel version >= 5.11 yet.
             */
            ASSERT_NOT_TESTED();
        }
        /* We do not let the app execute their own close_range ever. Instead we
         * make multiple close_range syscalls ourselves, one for each contiguous
         * sub-range of non-DR-private fds in [first, last].
         */
        execute_syscall = false;
        if (first_fd > last_fd) {
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            break;
        }
        uint cur_range_first_fd = 0;
        uint cur_range_last_fd = 0;
        bool cur_range_valid = false;
        int ret = 0;
        for (int i = first_fd; i <= last_fd; i++) {
            /* Do not allow any changes to DR-owned FDs. */
            if ((is_cloexec && fd_is_dr_owned(i)) ||
                (!is_cloexec && !handle_close_range_pre(dcontext, i))) {
                SYSLOG_INTERNAL_WARNING_ONCE("app trying to close private fd(s)");
                if (cur_range_valid) {
                    cur_range_valid = false;
                    ret = dynamorio_syscall(SYS_close_range, 3, cur_range_first_fd,
                                            cur_range_last_fd, flags);
                    if (ret != 0)
                        break;
                }
            } else {
#    ifdef LINUX
                if (!is_cloexec) {
                    signal_handle_close(dcontext, i);
                }
#    endif
                if (cur_range_valid) {
                    ASSERT(cur_range_last_fd == i - 1);
                    cur_range_last_fd = i;
                } else {
                    cur_range_first_fd = i;
                    cur_range_last_fd = i;
                    cur_range_valid = true;
                }
            }
        }
        if (cur_range_valid) {
            ret = dynamorio_syscall(SYS_close_range, 3, cur_range_first_fd,
                                    cur_range_last_fd, flags);
        }
        if (ret != 0) {
            set_failure_return_val(dcontext, ret);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
        } else {
            set_success_return_val(dcontext, 0);
        }
        break;
    }
#endif
    case SYS_close: {
        execute_syscall = handle_close_pre(dcontext);
#ifdef LINUX
        if (execute_syscall)
            signal_handle_close(dcontext, (file_t)sys_param(dcontext, 0));
#endif
        break;
    }
#if defined(SYS_dup2) || defined(SYS_dup3)
#    ifdef SYS_dup3
    case SYS_dup3:
#    endif
#    ifdef SYS_dup2
    case SYS_dup2:
#    endif
    {
        file_t newfd = (file_t)sys_param(dcontext, 1);
        if (fd_is_dr_owned(newfd) || fd_is_in_private_range(newfd)) {
            SYSLOG_INTERNAL_WARNING_ONCE("app trying to dup-close DR file(s)");
            LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
                "WARNING: app trying to dup2/dup3 to %d.  Disallowing.\n", newfd);
            if (DYNAMO_OPTION(fail_on_stolen_fds)) {
                set_failure_return_val(dcontext, EBADF);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            } else
                set_success_return_val(dcontext, 0);
            execute_syscall = false;
        }
        break;
    }
#endif

#ifdef MACOS
    case SYS_fcntl_nocancel:
#endif
    case SYS_fcntl: {
        int cmd = (int)sys_param(dcontext, 1);
        long arg = (long)sys_param(dcontext, 2);
        /* we only check for asking for min in private space: not min below
         * but actual will be above (see notes in os_file_init())
         */
        if ((cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) && fd_is_in_private_range(arg)) {
            SYSLOG_INTERNAL_WARNING_ONCE("app trying to open private fd(s)");
            LOG(THREAD, LOG_TOP | LOG_SYSCALLS, 1,
                "WARNING: app trying to dup to >= %d.  Disallowing.\n", arg);
            set_failure_return_val(dcontext, EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        } else {
            dcontext->sys_param0 = sys_param(dcontext, 0);
            dcontext->sys_param1 = cmd;
        }
        break;
    }

#if defined(X64) || !defined(ARM) || defined(MACOS)
    case SYS_getrlimit:
#endif
#if defined(LINUX) && !defined(X64)
    case SYS_ugetrlimit:
#endif
        /* save for post */
        dcontext->sys_param0 = sys_param(dcontext, 0); /* resource */
        dcontext->sys_param1 = sys_param(dcontext, 1); /* rlimit */
        break;

    case SYS_setrlimit: {
        int resource = (int)sys_param(dcontext, 0);
        if (resource == RLIMIT_NOFILE && DYNAMO_OPTION(steal_fds) > 0) {
#if !defined(ARM) && !defined(X64) && !defined(MACOS)
            struct compat_rlimit rlim;
#else
            struct rlimit rlim;
#endif
            if (!d_r_safe_read((void *)sys_param(dcontext, 1), sizeof(rlim), &rlim)) {
                LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EFAULT to app for prlimit64\n");
                set_failure_return_val(dcontext, EFAULT);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            } else if (rlim.rlim_cur > rlim.rlim_max) {
                LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EINVAL for prlimit64\n");
                set_failure_return_val(dcontext, EINVAL);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            } else if (rlim.rlim_max <= min_dr_fd &&
                       /* Can't raise hard unless have CAP_SYS_RESOURCE capability.
                        * XXX i#2980: should query for that capability.
                        */
                       rlim.rlim_max <= app_rlimit_nofile.rlim_max) {
                /* if the new rlimit is lower, pretend succeed */
                app_rlimit_nofile.rlim_cur = rlim.rlim_cur;
                app_rlimit_nofile.rlim_max = rlim.rlim_max;
                set_success_return_val(dcontext, 0);
            } else {
                LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EPERM to app for setrlimit\n");
                /* don't let app raise limits as that would mess up our fd space */
                set_failure_return_val(dcontext, EPERM);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            }
            execute_syscall = false;
        }
        break;
    }

#ifdef LINUX
    case SYS_prlimit64:
        /* save for post */
        dcontext->sys_param0 = sys_param(dcontext, 0); /* pid */
        dcontext->sys_param1 = sys_param(dcontext, 1); /* resource */
        dcontext->sys_param2 = sys_param(dcontext, 2); /* new rlimit */
        dcontext->sys_param3 = sys_param(dcontext, 3); /* old rlimit */
        if (/* XXX: how do we handle the case of setting rlimit.nofile on another
             * process that is running with DynamoRIO?
             */
            /* XXX: CLONE_FILES allows different processes to share the same file
             * descriptor table, and different threads of the same process have
             * separate file descriptor tables.  POSIX specifies that rlimits are
             * per-process, not per-thread, and Linux follows suit, so the threads
             * with different descriptors will not matter, and the pids sharing
             * descriptors turns into the hard-to-solve IPC problem.
             */
            (dcontext->sys_param0 == 0 || dcontext->sys_param0 == get_process_id()) &&
            dcontext->sys_param1 == RLIMIT_NOFILE &&
            dcontext->sys_param2 != (reg_t)NULL && DYNAMO_OPTION(steal_fds) > 0) {
            rlimit64_t rlim;
            if (!d_r_safe_read((void *)(dcontext->sys_param2), sizeof(rlim), &rlim)) {
                LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EFAULT to app for prlimit64\n");
                set_failure_return_val(dcontext, EFAULT);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            } else {
                LOG(THREAD, LOG_SYSCALLS, 2,
                    "syscall: prlimit64 soft=" INT64_FORMAT_STRING
                    " hard=" INT64_FORMAT_STRING " vs DR %d\n",
                    rlim.rlim_cur, rlim.rlim_max, min_dr_fd);
                if (rlim.rlim_cur > rlim.rlim_max) {
                    LOG(THREAD, LOG_SYSCALLS, 2, "\treturning EINVAL for prlimit64\n");
                    set_failure_return_val(dcontext, EINVAL);
                    DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                } else if (rlim.rlim_max <= min_dr_fd &&
                           /* Can't raise hard unless have CAP_SYS_RESOURCE capability.
                            * XXX i#2980: should query for that capability.
                            */
                           rlim.rlim_max <= app_rlimit_nofile.rlim_max) {
                    /* if the new rlimit is lower, pretend succeed */
                    app_rlimit_nofile.rlim_cur = rlim.rlim_cur;
                    app_rlimit_nofile.rlim_max = rlim.rlim_max;
                    set_success_return_val(dcontext, 0);
                    /* set old rlimit if necessary */
                    if (dcontext->sys_param3 != (reg_t)NULL) {
                        safe_write_ex((void *)(dcontext->sys_param3), sizeof(rlim),
                                      &app_rlimit_nofile, NULL);
                    }
                } else {
                    /* don't let app raise limits as that would mess up our fd space */
                    LOG(THREAD, LOG_SYSCALLS, 2,
                        "\treturning EPERM to app for prlimit64\n");
                    set_failure_return_val(dcontext, EPERM);
                    DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                }
            }
            execute_syscall = false;
        }
        break;
#endif

#ifdef LINUX
#    ifdef SYS_readlink
    case SYS_readlink:
#    endif
    case SYS_readlinkat:
        if (DYNAMO_OPTION(early_inject)) {
            dcontext->sys_param0 = sys_param(dcontext, 0);
            dcontext->sys_param1 = sys_param(dcontext, 1);
            dcontext->sys_param2 = sys_param(dcontext, 2);
            if (dcontext->sys_num == SYS_readlinkat)
                dcontext->sys_param3 = sys_param(dcontext, 3);
        }
        break;

        /* i#107 syscalls that might change/query app's segment */

#    if defined(X86) && defined(X64)
    case SYS_arch_prctl: {
        /* we handle arch_prctl in post_syscall */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        break;
    }
#    endif
#    ifdef X86
    case SYS_set_thread_area: {
        our_modify_ldt_t desc;
        if (INTERNAL_OPTION(mangle_app_seg) &&
            d_r_safe_read((void *)sys_param(dcontext, 0), sizeof(desc), &desc)) {
            if (os_set_app_thread_area(dcontext, &desc) &&
                safe_write_ex((void *)sys_param(dcontext, 0), sizeof(desc), &desc,
                              NULL)) {
                /* check if the range is unlimited */
                ASSERT_CURIOSITY(desc.limit == 0xfffff);
                execute_syscall = false;
                set_success_return_val(dcontext, 0);
            }
        }
        break;
    }
    case SYS_get_thread_area: {
        our_modify_ldt_t desc;
        if (INTERNAL_OPTION(mangle_app_seg) &&
            d_r_safe_read((const void *)sys_param(dcontext, 0), sizeof(desc), &desc)) {
            if (os_get_app_thread_area(dcontext, &desc) &&
                safe_write_ex((void *)sys_param(dcontext, 0), sizeof(desc), &desc,
                              NULL)) {
                execute_syscall = false;
                set_success_return_val(dcontext, 0);
            }
        }
        break;
    }
#    endif /* X86 */
#    ifdef ARM
    case SYS_set_tls: {
        LOG(THREAD, LOG_VMAREAS | LOG_SYSCALLS, 2, "syscall: set_tls " PFX "\n",
            sys_param(dcontext, 0));
        if (os_set_app_tls_base(dcontext, TLS_REG_LIB, (void *)sys_param(dcontext, 0))) {
            execute_syscall = false;
            set_success_return_val(dcontext, 0);
        } else {
            ASSERT_NOT_REACHED();
        }
        break;
    }
    case SYS_cacheflush: {
        /* We assume we don't want to change the executable_areas list or change
         * the selfmod status of this region: else we should call something
         * that invokes handle_modified_code() in a way that handles a bigger
         * region than a single write.
         */
        app_pc start = (app_pc)sys_param(dcontext, 0);
        app_pc end = (app_pc)sys_param(dcontext, 1);
        LOG(THREAD, LOG_VMAREAS | LOG_SYSCALLS, 2,
            "syscall: cacheflush " PFX "-" PFX "\n", start, end);
        flush_fragments_from_region(dcontext, start, end - start,
                                    /* An unlink flush should be fine: the app must
                                     * use synch to ensure other threads see the
                                     * new code.
                                     */
                                    false /*don't force synchall*/,
                                    NULL /*flush_completion_callback*/,
                                    NULL /*user_data*/);
        break;
    }
#    endif /* ARM */
#elif defined(MACOS)
    /* FIXME i#58: handle i386_{get,set}_ldt and thread_fast_set_cthread_self64 */
#endif

#ifdef DEBUG
#    ifdef MACOS
    case SYS_open_nocancel:
#    endif
#    ifdef SYS_open
    case SYS_open: {
        dcontext->sys_param0 = sys_param(dcontext, 0);
        break;
    }
#    endif
#endif
#ifdef SYS_openat2
    case SYS_openat2:
#endif
    case SYS_openat: {
        /* XXX: For completeness we might want to replace paths for SYS_open and
         * possibly others, but SYS_openat is all we need on modern systems so we
         * limit syscall overhead to this single point for now.
         */
        dcontext->sys_param0 = 0;
        dcontext->sys_param1 = sys_param(dcontext, 1);
        const char *path = (const char *)dcontext->sys_param1;
        if (!IS_STRING_OPTION_EMPTY(xarch_root) && !os_file_exists(path, false)) {
            char *buf = heap_alloc(dcontext, MAXIMUM_PATH HEAPACCT(ACCT_OTHER));
            string_option_read_lock();
            snprintf(buf, MAXIMUM_PATH, "%s/%s", DYNAMO_OPTION(xarch_root), path);
            buf[MAXIMUM_PATH - 1] = '\0';
            string_option_read_unlock();
            if (os_file_exists(buf, false)) {
                LOG(THREAD, LOG_SYSCALLS, 2, "SYS_openat: replacing |%s| with |%s|\n",
                    path, buf);
                set_syscall_param(dcontext, 1, (reg_t)buf);
                /* Save for freeing in post. */
                dcontext->sys_param0 = (reg_t)buf;
            } else
                heap_free(dcontext, buf, MAXIMUM_PATH HEAPACCT(ACCT_OTHER));
        }
        break;
    }
#ifdef LINUX
    case SYS_rseq:
        LOG(THREAD, LOG_VMAREAS | LOG_SYSCALLS, 2, "syscall: rseq " PFX " %d %d %d\n",
            sys_param(dcontext, 0), sys_param(dcontext, 1), sys_param(dcontext, 2),
            sys_param(dcontext, 3));
        if (DYNAMO_OPTION(disable_rseq)) {
            set_failure_return_val(dcontext, ENOSYS);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        } else {
            dcontext->sys_param0 = sys_param(dcontext, 0);
        }
        break;
#endif
    default: {
#ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(dcontext->sys_num)) {
            execute_syscall = vmkuw_pre_system_call(dcontext);
            break;
        }
#endif
        break;
    }

    } /* end switch */

    dcontext->whereami = old_whereami;
    return execute_syscall;
}

void
all_memory_areas_lock(void)
{
    IF_NO_MEMQUERY(memcache_lock());
}

void
all_memory_areas_unlock(void)
{
    IF_NO_MEMQUERY(memcache_unlock());
}

void
update_all_memory_areas(app_pc start, app_pc end, uint prot, int type)
{
    IF_NO_MEMQUERY(memcache_update(start, end, prot, type));
}

bool
remove_from_all_memory_areas(app_pc start, app_pc end)
{
    IF_NO_MEMQUERY(return memcache_remove(start, end));
    return true;
}

/* We consider a module load to happen at the first mmap, so we check on later
 * overmaps to ensure things look consistent. */
static bool
mmap_check_for_module_overlap(app_pc base, size_t size, bool readable, uint64 inode,
                              bool at_map)
{
    module_area_t *ma;

    os_get_module_info_lock();
    ma = module_pc_lookup(base);
    if (ma != NULL) {
        /* FIXME - how can we distinguish between the loader mapping the segments
         * over the initial map from someone just mapping over part of a module? If
         * is the latter case need to adjust the view size or remove from module list. */
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "%s mmap overlapping module area : \n"
            "\tmap : base=" PFX " base+size=" PFX " inode=" UINT64_FORMAT_STRING "\n"
            "\tmod : start=" PFX " end=" PFX " inode=" UINT64_FORMAT_STRING "\n",
            at_map ? "new" : "existing", base, base + size, inode, ma->start, ma->end,
            ma->names.inode);
        ASSERT_CURIOSITY(base >= ma->start);
        if (at_map) {
            ASSERT_CURIOSITY(base + size <= ma->end);
        } else {
            /* FIXME - I'm having problems with this check for existing maps.  I
             * haven't been able to get gdb to break in early enough to really get a good
             * look at the early loader behavior.  Two issues:  One case is with our .so
             * for which the anonymous .bss mapping is one page larger than expected
             * (which might be some loader bug in the size calculation? or something? if
             * so should see it trigger the at_map curiosity on some dll and can address
             * then) and the other is that for a few executables the .bss mapping is much
             * larger (~0x20000 larger) then expected when running under DR (but not
             * running natively where it is instead the expected size).  Both could just
             * be the loader merging adjacent identically protected regions though I
             * can't explain the discrepancy between DR and native given that our vmmheap
             * is elsewhere in the address space (so who and how allocated that adjacent
             * memory). I've yet to see any issue with dynamically loaded modules so
             * it's probably the loader merging regions.  Still worth investigating. */
            ASSERT_CURIOSITY(inode == 0 /*see above comment*/ ||
                             module_contains_addr(ma, base + size - 1));
        }
        /* Handle cases like transparent huge pages where there are anon regions on top
         * of the file mapping (i#2566).
         */
        if (ma->names.inode == 0)
            ma->names.inode = inode;
        ASSERT_CURIOSITY(ma->names.inode == inode || inode == 0 /* for .bss */);
        DOCHECK(1, {
            if (readable && module_is_header(base, size)) {
                /* Case 8879: For really small modules, to save disk space, the same
                 * disk page could hold both RO and .data, occupying just 1 page of
                 * disk space, e.g. /usr/lib/httpd/modules/mod_auth_anon.so.  When
                 * such a module is mapped in, the os maps the same disk page twice,
                 * one readonly and one copy-on-write (see pg.  96, Sec 4.4 from
                 * Linkers and Loaders by John R.  Levine). It also possible for
                 * such small modules to have multiple LOAD data segments. Since all
                 * these segments are mapped from a single disk page they will all have an
                 * elf_header satisfying the check above. So, if the new mmap overlaps an
                 * elf_area and it is also a header, then make sure the offsets (from the
                 * beginning of the backing file) of all the segments up to the currect
                 * one are within the page size. Note, if it is a header of a different
                 * module, then we'll not have an overlap, so we will not hit this case.
                 */
                bool cur_seg_found = false;
                int seg_id = 0;
                while (seg_id < ma->os_data.num_segments &&
                       ma->os_data.segments[seg_id].start <= base) {
                    cur_seg_found = ma->os_data.segments[seg_id].start == base;
                    ASSERT_CURIOSITY(
                        ma->os_data.segments[seg_id].offset <
                        PAGE_SIZE
                            /* On Mac we walk the dyld module list before the
                             * address space, so we often hit modules we already
                             * know about. */
                            IF_MACOS(|| !dynamo_initialized && ma->start == base));
                    ++seg_id;
                }
                ASSERT_CURIOSITY(cur_seg_found);
            }
        });
    }
    os_get_module_info_unlock();
#ifdef ANDROID
    /* i#1860: we need to keep looking for the segment with .dynamic as Android's
     * loader does not map the whole file up front.
     */
    if (ma != NULL && at_map && readable)
        os_module_update_dynamic_info(base, size, at_map);
#endif
    return ma != NULL;
}

static void
os_add_new_app_module(dcontext_t *dcontext, bool at_map, app_pc base, size_t size,
                      uint memprot)
{
    memquery_iter_t iter;
    bool found_map = false;
    uint64 inode = 0;
    const char *filename = "";
    size_t mod_size = size;

    if (!at_map) {
        /* the size is the first seg size, get the whole module size instead */
        app_pc first_seg_base = NULL;
        app_pc first_seg_end = NULL;
        app_pc last_seg_end = NULL;
        if (module_walk_program_headers(base, size, at_map, false, &first_seg_base,
                                        &first_seg_end, &last_seg_end, NULL, NULL)) {
            ASSERT_CURIOSITY(size ==
                                 (ALIGN_FORWARD(first_seg_end, PAGE_SIZE) -
                                  (ptr_uint_t)first_seg_base) ||
                             base == vdso_page_start || base == vsyscall_page_start);
            mod_size =
                ALIGN_FORWARD(last_seg_end, PAGE_SIZE) - (ptr_uint_t)first_seg_base;
        }
    }
    LOG(THREAD, LOG_SYSCALLS | LOG_VMAREAS, 2, "dlopen " PFX "-" PFX "%s\n", base,
        base + mod_size, TEST(MEMPROT_EXEC, memprot) ? " +x" : "");

    /* Mapping in a new module.  From what we've observed of the loader's
     * behavior, it first maps the file in with size equal to the final
     * memory image size (I'm not sure how it gets that size without reading
     * in the elf header and then walking through all the program headers to
     * get the largest virtual offset).  This is necessary to reserve all the
     * space that will be needed.  It then walks through the program headers
     * mapping over the the previously mapped space with the appropriate
     * permissions and offsets.  Note that the .bss portion is mapped over
     * as anonymous.  It may also, depending on the program headers, make some
     * areas read-only after fixing up their relocations etc. NOTE - at
     * no point are the section headers guaranteed to be mapped in so we can't
     * reliably walk sections (only segments) without looking to disk.
     */
    /* FIXME - when should we add the module to our list?  At the first map
     * seems to be the best choice as we know the bounds and it's difficult to
     * tell when the loader is finished.  The downside is that at the initial map
     * the memory layout isn't finalized (memory beyond the first segment will
     * be shifted for page alignment reasons), so we have to be careful and
     * make adjustments to read anything beyond the first segment until the
     * loader finishes. This goes for the client too as it gets notified when we
     * add to the list.  FIXME we could try to track the expected segment overmaps
     * and only notify the client after the last one (though that's still before
     * linking and relocation, but that's true on Windows too). */
    /* Get filename & inode for the list. */
    memquery_iterator_start(&iter, base, true /* plan to alloc a module_area_t */);
    while (memquery_iterator_next(&iter)) {
        if (iter.vm_start == base) {
            ASSERT_CURIOSITY(iter.inode != 0 || base == vdso_page_start ||
                             base == vsyscall_page_start);
            ASSERT_CURIOSITY(iter.offset == 0); /* first map shouldn't have offset */
            /* XREF 307599 on rounding module end to the next PAGE boundary */
            ASSERT_CURIOSITY(
                (iter.vm_end - iter.vm_start == ALIGN_FORWARD(size, PAGE_SIZE)));
            inode = iter.inode;
            filename = dr_strdup(iter.comment HEAPACCT(ACCT_OTHER));
            found_map = true;
            break;
        }
    }
    memquery_iterator_stop(&iter);
#ifdef HAVE_MEMINFO
    /* barring weird races we should find this map except */
    ASSERT_CURIOSITY(found_map);
#else  /* HAVE_MEMINFO */
    /* Without /proc/maps or other memory querying interface available at
     * library map time, there is no way to find out the name of the file
     * that was mapped, thus its inode isn't available either.
     *
     * Just module_list_add with no filename will still result in
     * library name being extracted from the .dynamic section and added
     * to the module list.  However, this name may not always exist, thus
     * we might have a library with no file name available at all!
     *
     * Note: visor implements vsi mem maps that give file info, but, no
     *       path, should be ok.  xref PR 401580.
     *
     * Once PR 235433 is implemented in visor then fix memquery_iterator*() to
     * use vsi to find out page protection info, file name & inode.
     */
#endif /* HAVE_MEMINFO */
    /* XREF 307599 on rounding module end to the next PAGE boundary */
    if (found_map) {
        module_list_add(base, ALIGN_FORWARD(mod_size, PAGE_SIZE), at_map, filename,
                        inode);
        dr_strfree(filename HEAPACCT(ACCT_OTHER));
    }
}

void
os_check_new_app_module(dcontext_t *dcontext, app_pc pc)
{
    module_area_t *ma;
    os_get_module_info_lock();
    ma = module_pc_lookup(pc);
    /* ma might be NULL due to dynamic generated code or custom loaded modules */
    if (ma == NULL) {
        dr_mem_info_t info;
        /* i#1760: an app module loaded by custom loader (e.g., bionic libc)
         * might not be detected by DynamoRIO in process_mmap.
         */
        if (query_memory_ex_from_os(pc, &info) && info.type == DR_MEMTYPE_IMAGE) {
            /* add the missing module */
            os_get_module_info_unlock();
            os_add_new_app_module(get_thread_private_dcontext(), false /*!at_map*/,
                                  info.base_pc, info.size, info.prot);
            os_get_module_info_lock();
        }
    }
    os_get_module_info_unlock();
}

/* All processing for mmap and mmap2. */
static void
process_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
             uint flags _IF_DEBUG(const char *map_type))
{
    bool image = false;
    uint memprot = osprot_to_memprot(prot);
#ifdef ANDROID
    /* i#1861: avoid merging file-backed w/ anon regions */
    if (!TEST(MAP_ANONYMOUS, flags))
        memprot |= MEMPROT_HAS_COMMENT;
#endif

    LOG(THREAD, LOG_SYSCALLS, 4, "process_mmap(" PFX "," PFX ",0x%x,%s,%s)\n", base, size,
        flags, memprot_string(memprot), map_type);
    /* Notes on how ELF SOs are mapped in.
     *
     * o The initial mmap for an ELF file specifies enough space for
     *   all segments (and their constituent sections) in the file.
     *   The protection bits for that section are used for the entire
     *   region, and subsequent mmaps for subsequent segments within
     *   the region modify their portion's protection bits as needed.
     *   So if the prot bits for the first segment are +x, the entire
     *   region is +x. ** Note that our primary concern is adjusting
     *   exec areas to reflect the prot bits of subsequent
     *   segments. ** The region is added to the all-memory areas
     *   and also to exec areas (as determined by app_memory_allocation()).
     *
     * o Any subsequent segment sub-mappings specify their own protection
     *   bits and therefore are added to the exec areas via normal
     *   processing. They are also "naturally" added to the all-mems list.
     *   We do a little extra processing when mapping into a previously
     *   mapped region and the prot bits mismatch; if the new mapping is
     *   not +x, flushing needs to occur.
     */
    /* process_mmap can be called with PROT_NONE, so we need to check if we
     * can read the memory to see if it is a elf_header
     */
    /* XXX: get inode for check */
    if (TEST(MAP_ANONYMOUS, flags)) {
        /* not an ELF mmap */
        LOG(THREAD, LOG_SYSCALLS, 4, "mmap " PFX ": anon\n", base);
    } else if (mmap_check_for_module_overlap(base, size, TEST(MEMPROT_READ, memprot), 0,
                                             true)) {
        /* FIXME - how can we distinguish between the loader mapping the segments
         * over the initial map from someone just mapping over part of a module? If
         * is the latter case need to adjust the view size or remove from module list. */
        image = true;
        DODEBUG({ map_type = "ELF SO"; });
        LOG(THREAD, LOG_SYSCALLS, 4, "mmap " PFX ": overlaps image\n", base);
    } else if (TEST(MEMPROT_READ, memprot) &&
               /* i#727: We can still get SIGBUS on mmap'ed files that can't be
                * read, so pass size=0 to use a safe_read.
                */
               module_is_header(base, 0)) {
#ifdef ANDROID
        /* The Android loader's initial all-segment-covering mmap is anonymous */
        dr_mem_info_t info;
        if (query_memory_ex_from_os((byte *)ALIGN_FORWARD(base + size, PAGE_SIZE),
                                    &info) &&
            info.prot == MEMPROT_NONE && info.type == DR_MEMTYPE_DATA) {
            LOG(THREAD, LOG_SYSCALLS, 4, "mmap " PFX ": Android elf\n", base);
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
            os_add_new_app_module(dcontext, true /*at_map*/, base,
                                  /* pass segment size, not whole module size */
                                  size, memprot);
        } else
#endif
            if (module_is_partial_map(base, size, memprot)) {
            /* i#1240: App might read first page of ELF header using mmap, which
             * might accidentally be treated as a module load. Heuristically
             * distinguish this by saying that if this is the first mmap for an ELF
             * (i.e., it doesn't overlap with a previous map), and if it's small,
             * then don't treat it as a module load.
             */
            LOG(THREAD, LOG_SYSCALLS, 4, "mmap " PFX ": partial\n", base);
        } else {
            LOG(THREAD, LOG_SYSCALLS, 4, "mmap " PFX ": elf header\n", base);
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
            os_add_new_app_module(dcontext, true /*at_map*/, base, size, memprot);
        }
    }

    LOG(THREAD, LOG_SYSCALLS, 4, "\t try app_mem_alloc\n");
    IF_NO_MEMQUERY(memcache_handle_mmap(dcontext, base, size, memprot, image));
    if (app_memory_allocation(dcontext, base, size, memprot, image _IF_DEBUG(map_type)))
        STATS_INC(num_app_code_modules);
    LOG(THREAD, LOG_SYSCALLS, 4, "\t app_mem_alloc -- DONE\n");
}

#ifdef LINUX
/* Call right after the system call.
 * i#173: old_prot and old_type should be from before the system call
 */
static bool
handle_app_mremap(dcontext_t *dcontext, byte *base, size_t size, byte *old_base,
                  size_t old_size, uint old_prot, uint old_type)
{
    if (!mmap_syscall_succeeded(base))
        return false;
    if (base != old_base || size < old_size) { /* take action only if
                                                * there was a change */
        DEBUG_DECLARE(bool ok;)
        /* fragments were shifted...don't try to fix them, just flush */
        app_memory_deallocation(dcontext, (app_pc)old_base, old_size,
                                false /* don't own thread_initexit_lock */,
                                false /* not image, FIXME: somewhat arbitrary */);
        DOCHECK(1, {
            /* we don't expect to see remappings of modules */
            os_get_module_info_lock();
            ASSERT_CURIOSITY(!module_overlaps(base, size));
            os_get_module_info_unlock();
        });
        /* Verify that the current prot on the new region (according to
         * the os) is the same as what the prot used to be for the old
         * region.
         */
        DOCHECK(1, {
            uint memprot;
            ok = get_memory_info_from_os(base, NULL, NULL, &memprot);
            /* allow maps to have +x,
             * +x may be caused by READ_IMPLIES_EXEC set in personality flag (i#262)
             */
            ASSERT(ok &&
                   (memprot == old_prot || (memprot & (~MEMPROT_EXEC)) == old_prot));
        });
        app_memory_allocation(dcontext, base, size, old_prot,
                              old_type == DR_MEMTYPE_IMAGE _IF_DEBUG("mremap"));
        IF_NO_MEMQUERY(memcache_handle_mremap(dcontext, base, size, old_base, old_size,
                                              old_prot, old_type));
    }
    return true;
}

static void
handle_app_brk(dcontext_t *dcontext, byte *lowest_brk /*if known*/, byte *old_brk,
               byte *new_brk)
{
    /* i#851: the brk might not be page aligned */
    old_brk = (app_pc)ALIGN_FORWARD(old_brk, PAGE_SIZE);
    new_brk = (app_pc)ALIGN_FORWARD(new_brk, PAGE_SIZE);
    if (new_brk < old_brk) {
        /* Usually the heap is writable, so we don't really need to call
         * this here: but seems safest to do so, esp if someone made part of
         * the heap read-only and then put code there.
         */
        app_memory_deallocation(dcontext, new_brk, old_brk - new_brk,
                                false /* don't own thread_initexit_lock */,
                                false /* not image */);
    } else if (new_brk > old_brk) {
        /* No need to call app_memory_allocation() as doesn't interact
         * w/ security policies.
         */
    }
    IF_NO_MEMQUERY(memcache_handle_app_brk(lowest_brk, old_brk, new_brk));
}
#endif

/* This routine is *not* called is pre_system_call() returns false to skip
 * the syscall.
 */
/* XXX: split out specific handlers into separate routines
 */
void
post_system_call(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    /* registers have been clobbered, so sysnum is kept in dcontext */
    int sysnum = dcontext->sys_num;
    /* We expect most syscall failures to return < 0, so >= 0 is success.
     * Some syscall return addresses that have the sign bit set and so
     * appear to be failures but are not. They are handled on a
     * case-by-case basis in the switch statement below.
     */
    ptr_int_t result = (ptr_int_t)MCXT_SYSCALL_RES(mc); /* signed */
    bool success = syscall_successful(mc, sysnum);
    app_pc base;
    size_t size;
    uint prot;
    dr_where_am_i_t old_whereami;
    DEBUG_DECLARE(bool ok;)

    RSTATS_INC(post_syscall);

    old_whereami = dcontext->whereami;
    dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;

#if defined(LINUX) && defined(X86)
    /* PR 313715: restore xbp since for some vsyscall sequences that use
     * the syscall instruction its value is needed:
     *   0xffffe400 <__kernel_vsyscall+0>:       push   %ebp
     *   0xffffe401 <__kernel_vsyscall+1>:       mov    %ecx,%ebp
     *   0xffffe403 <__kernel_vsyscall+3>:       syscall
     *   0xffffe405 <__kernel_vsyscall+5>:       mov    $0x2b,%ecx
     *   0xffffe40a <__kernel_vsyscall+10>:      movl   %ecx,%ss
     *   0xffffe40c <__kernel_vsyscall+12>:      mov    %ebp,%ecx
     *   0xffffe40e <__kernel_vsyscall+14>:      pop    %ebp
     *   0xffffe40f <__kernel_vsyscall+15>:      ret
     */
    if (should_syscall_method_be_sysenter() && !dcontext->sys_was_int) {
        mc->xbp = dcontext->sys_xbp;
    }
#endif

    /* handle fork, try to do it early before too much logging occurs */
    if (false
#ifdef SYS_fork
        || sysnum ==
            SYS_fork
#endif
                IF_LINUX(||
                         (sysnum == SYS_clone && !TEST(CLONE_VM, dcontext->sys_param0)) ||
                         (sysnum == SYS_clone3 &&
                          !TEST(CLONE_VM, get_stored_clone3_flags(dcontext))))) {
        if (result == 0) {
            /* we're the child */
            thread_id_t child = get_sys_thread_id();
#ifdef DEBUG
            thread_id_t parent = get_parent_id();
            SYSLOG_INTERNAL_INFO("-- parent %d forked child %d --", parent, child);
#endif
            /* first, fix TLS of dcontext */
            ASSERT(parent != 0);
            /* change parent pid to our pid */
            replace_thread_id(dcontext->owning_thread, child);
            dcontext->owning_thread = child;
            dcontext->owning_process = get_process_id();

            /* now let dynamo initialize new shared memory, logfiles, etc.
             * need access to static vars in dynamo.c, that's why we don't do it. */
            /* FIXME - xref PR 246902 - d_r_dispatch runs a lot of code before
             * getting to post_system_call() is any of that going to be messed up
             * by waiting till here to fixup the child logfolder/file and tid?
             */
            dynamorio_fork_init(dcontext);

            LOG(THREAD, LOG_SYSCALLS, 1,
                "after fork-like syscall: parent is %d, child is %d\n", parent, child);
        } else {
            /* we're the parent */
            os_fork_post(dcontext, true /*parent*/);
        }
    }

    LOG(THREAD, LOG_SYSCALLS, 2, "post syscall: sysnum=" PFX ", result=" PFX " (%d)\n",
        sysnum, MCXT_SYSCALL_RES(mc), (int)MCXT_SYSCALL_RES(mc));

    switch (sysnum) {

        /****************************************************************************/
        /* MEMORY REGIONS */

#ifdef DEBUG
#    ifdef MACOS
    case SYS_open_nocancel:
#    endif
#    ifdef SYS_open
    case SYS_open: {
        if (success) {
            /* useful for figuring out what module was loaded that then triggers
             * module.c elf curiosities
             */
            LOG(THREAD, LOG_SYSCALLS, 2, "SYS_open %s => %d\n", dcontext->sys_param0,
                (int)result);
        }
        break;
    }
#    endif
#endif

#if defined(LINUX) && !defined(X64) && !defined(ARM)
    case SYS_mmap:
#endif
    case IF_MACOS_ELSE(SYS_mmap, IF_X64_ELSE(SYS_mmap, SYS_mmap2)): {
        uint flags;
        DEBUG_DECLARE(const char *map_type;)
        RSTATS_INC(num_app_mmaps);
        base = (app_pc)MCXT_SYSCALL_RES(mc); /* For mmap, it's NOT arg->addr! */
        /* mmap isn't simply a user-space wrapper for mmap2. It's called
         * directly when dynamically loading an SO, i.e., dlopen(). */
#ifdef LINUX /* MacOS success is in CF */
        success = mmap_syscall_succeeded((app_pc)result);
        /* The syscall either failed OR the retcode is less than the
         * largest uint value of any errno and the addr returned is
         * page-aligned.
         */
        ASSERT_CURIOSITY(
            !success ||
            ((app_pc)result < (app_pc)(ptr_int_t)-0x1000 && ALIGNED(base, PAGE_SIZE)));
#else
        ASSERT_CURIOSITY(!success || ALIGNED(base, PAGE_SIZE));
#endif
        if (!success)
            goto exit_post_system_call;
#if defined(LINUX) && !defined(X64) && !defined(ARM)
        if (sysnum == SYS_mmap) {
            /* The syscall succeeded so the read of 'arg' should be
             * safe. */
            mmap_arg_struct_t *arg = (mmap_arg_struct_t *)dcontext->sys_param0;
            size = (size_t)arg->len;
            prot = (uint)arg->prot;
            flags = (uint)arg->flags;
            DEBUG_DECLARE(map_type = "mmap";)
        } else {
#endif
            size = (size_t)dcontext->sys_param1;
            prot = (uint)dcontext->sys_param2;
            flags = (uint)dcontext->sys_param3;
            DEBUG_DECLARE(map_type = IF_X64_ELSE("mmap2", "mmap");)
#if defined(LINUX) && !defined(X64) && !defined(ARM)
        }
#endif
        process_mmap(dcontext, base, size, prot, flags _IF_DEBUG(map_type));
        break;
    }
    case SYS_munmap: {
        app_pc addr = (app_pc)dcontext->sys_param0;
        size_t len = (size_t)dcontext->sys_param1;
        /* We assumed in pre_system_call() that the unmap would succeed
         * and flushed fragments and removed the region from exec areas.
         * If the unmap failed, we re-add the region to exec areas.
         * For zero-length unmaps we don't need to re-add anything,
         * and we hit an assert in vmareas.c if we try (i#4031).
         *
         * The same logic can be used on Windows (but isn't yet).
         */
        /* FIXME There are shortcomings to the approach. If another thread
         * executes in the region after our pre_system_call processing
         * but before the re-add below, it will get a security violation.
         * That's less than ideal but at least isn't a security hole.
         * The overall shortcoming is that we lose the state from our
         * stateful security policies -- future exec list, tables used
         * for RCT (.C/.E/.F) -- which can't be easily restored. Also,
         * the re-add could add a region that wasn't on the exec list
         * previously.
         *
         * See case 7559 for a better approach.
         */
        if (!success && len != 0) {
            dr_mem_info_t info;
            /* must go to os to get real memory since we already removed */
            DEBUG_DECLARE(ok =)
            query_memory_ex_from_os(addr, &info);
            ASSERT(ok);
            app_memory_allocation(dcontext, addr, len, info.prot,
                                  info.type ==
                                      DR_MEMTYPE_IMAGE _IF_DEBUG("failed munmap"));
            IF_NO_MEMQUERY(
                memcache_update_locked((app_pc)ALIGN_BACKWARD(addr, PAGE_SIZE),
                                       (app_pc)ALIGN_FORWARD(addr + len, PAGE_SIZE),
                                       info.prot, info.type, false /*add back*/));
        }
        break;
    }
#ifdef LINUX
    case SYS_mremap: {
        app_pc old_base = (app_pc)dcontext->sys_param0;
        size_t old_size = (size_t)dcontext->sys_param1;
        base = (app_pc)MCXT_SYSCALL_RES(mc);
        size = (size_t)dcontext->sys_param2;
        /* even if no shift, count as munmap plus mmap */
        RSTATS_INC(num_app_munmaps);
        RSTATS_INC(num_app_mmaps);
        success =
            handle_app_mremap(dcontext, base, size, old_base, old_size,
                              /* i#173: use memory prot and type
                               * obtained from pre_system_call
                               */
                              (uint)dcontext->sys_param3, (uint)dcontext->sys_param4);
        /* The syscall either failed OR the retcode is less than the
         * largest uint value of any errno and the addr returned is
         * is page-aligned.
         */
        ASSERT_CURIOSITY(
            !success ||
            ((app_pc)result < (app_pc)(ptr_int_t)-0x1000 && ALIGNED(base, PAGE_SIZE)));
        if (!success)
            goto exit_post_system_call;
        break;
    }
#endif
    case SYS_mprotect: {
        base = (app_pc)dcontext->sys_param0;
        size = dcontext->sys_param1;
        prot = dcontext->sys_param2;
#ifdef VMX86_SERVER
        /* PR 475111: workaround for PR 107872 */
        if (os_in_vmkernel_userworld() && result == -EBUSY && prot == PROT_NONE) {
            result = mprotect_syscall(base, size, PROT_READ);
            /* since non-Mac, we can use this even if the call failed */
            set_success_return_val(dcontext, result);
            success = (result >= 0);
            LOG(THREAD, LOG_VMAREAS, 1,
                "re-doing mprotect -EBUSY for " PFX "-" PFX " => %d\n", base, base + size,
                (int)result);
            SYSLOG_INTERNAL_WARNING_ONCE("re-doing mprotect for PR 475111, PR 107872");
        }
#endif
        /* FIXME i#143: we need to tweak the returned oldprot for
         * writable areas we've made read-only
         */
        if (!success) {
            uint memprot = 0;
            /* Revert the prot bits if needed. */
            if (!get_memory_info_from_os(base, NULL, NULL, &memprot))
                memprot = PROT_NONE;
            LOG(THREAD, LOG_SYSCALLS, 3,
                "syscall: mprotect failed: " PFX "-" PFX " prot->%d\n", base, base + size,
                osprot_to_memprot(prot));
            LOG(THREAD, LOG_SYSCALLS, 3, "\told prot->%d\n", memprot);
            if (prot != memprot_to_osprot(memprot)) {
                /* We're trying to reverse the prot change, assuming that
                 * this action doesn't have any unexpected side effects
                 * when doing so (such as not reversing some bit of internal
                 * state).
                 */
                uint new_memprot;
                DEBUG_DECLARE(uint res =)
                app_memory_protection_change(dcontext, base, size,
                                             osprot_to_memprot(prot), &new_memprot, NULL,
                                             false /*!image*/);
                ASSERT_NOT_IMPLEMENTED(res != SUBSET_APP_MEM_PROT_CHANGE);
                ASSERT(res == DO_APP_MEM_PROT_CHANGE ||
                       res == PRETEND_APP_MEM_PROT_CHANGE);

                /* PR 410921 - Revert the changes to all-mems list.
                 * FIXME: This fix assumes the whole region had the prot &
                 * type, which is true in the cases we have seen so far, but
                 * theoretically may not be true.  If it isn't true, multiple
                 * memory areas with different types/protections might have
                 * been changed in pre_system_call(), so will have to keep a
                 * list of all vmareas changed.  This might be expensive for
                 * each mprotect syscall to guard against a rare theoretical bug.
                 */
                ASSERT_CURIOSITY(!dcontext->mprot_multi_areas);
                IF_NO_MEMQUERY(memcache_update_locked(
                    base, base + size, memprot, -1 /*type unchanged*/, true /*exists*/));
            }
        }
        break;
    }
#ifdef ANDROID
    case SYS_prctl: {
        int code = (int)dcontext->sys_param0;
        int subcode = (ulong)dcontext->sys_param1;
        if (success && code == PR_SET_VMA && subcode == PR_SET_VMA_ANON_NAME) {
            byte *addr = (byte *)dcontext->sys_param2;
            size_t len = (size_t)dcontext->sys_param3;
            IF_DEBUG(const char *comment = (const char *)dcontext->sys_param4;)
            uint memprot = 0;
            if (!get_memory_info_from_os(addr, NULL, NULL, &memprot))
                memprot = MEMPROT_NONE;
            /* We're post-syscall so from_os should match the prctl */
            ASSERT((comment == NULL && !TEST(MEMPROT_HAS_COMMENT, memprot)) ||
                   (comment != NULL && TEST(MEMPROT_HAS_COMMENT, memprot)));
            LOG(THREAD, LOG_SYSCALLS, 2,
                "syscall: prctl PR_SET_VMA_ANON_NAME base=" PFX " size=" PFX
                " comment=%s\n",
                addr, len, comment == NULL ? "<null>" : comment);
            IF_NO_MEMQUERY(memcache_update_locked(
                addr, addr + len, memprot, -1 /*type unchanged*/, true /*exists*/));
        }
        break;
    }
#endif
#ifdef LINUX
    case SYS_brk: {
        /* i#91/PR 396352: need to watch SYS_brk to maintain all_memory_areas.
         * This code should work regardless of whether syscall failed
         * (if it failed, the old break will be returned).  We stored
         * the old break in sys_param1 in pre-syscall.
         */
        app_pc old_brk = (app_pc)dcontext->sys_param1;
        app_pc new_brk = (app_pc)result;
        DEBUG_DECLARE(app_pc req_brk = (app_pc)dcontext->sys_param0;);
        ASSERT(!DYNAMO_OPTION(emulate_brk)); /* shouldn't get here */
#    ifdef DEBUG
        if (DYNAMO_OPTION(early_inject) &&
            req_brk != NULL /* Ignore calls that don't increase brk. */) {
            DO_ONCE({
                ASSERT_CURIOSITY(new_brk > old_brk &&
                                 "i#1004: first brk() "
                                 "allocation failed with -early_inject");
            });
        }
#    endif
        handle_app_brk(dcontext, NULL, old_brk, new_brk);
        break;
    }
#endif

    /****************************************************************************/
    /* SPAWNING -- fork mostly handled above */

#ifdef LINUX
    case SYS_clone3:
    case SYS_clone: {
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: clone returned " PFX "\n",
            MCXT_SYSCALL_RES(mc));
        /* TODO i#5221: Handle clone3 returning errors other than ENOSYS. */
        /* We switch the lib tls segment back to dr's privlib segment.
         * Please refer to comment on os_switch_lib_tls.
         * It is only called in parent thread.
         * The child thread's tls setup is done in os_tls_app_seg_init.
         */
        if (was_thread_create_syscall(dcontext)) {
            if (INTERNAL_OPTION(private_loader))
                os_switch_lib_tls(dcontext, false /*to dr*/);
            /* i#2089: we already restored the DR tls in os_clone_post() */

            if (sysnum == SYS_clone3) {
                /* Free DR's copy of clone_args and restore the pointer to the
                 * app's copy in the SYSCALL_PARAM_CLONE3_CLONE_ARGS reg.
                 * sys_param1 contains the pointer to DR's clone_args, and
                 * sys_param0 contains the pointer to the app's original
                 * clone_args.
                 */
#    ifdef X86
                ASSERT(sys_param(dcontext, SYSCALL_PARAM_CLONE3_CLONE_ARGS) ==
                       dcontext->sys_param1);
                set_syscall_param(dcontext, SYSCALL_PARAM_CLONE3_CLONE_ARGS,
                                  dcontext->sys_param0);
#    else
                /* On AArchXX r0 is used to pass the first arg to the syscall as well as
                 * to hold its return value. As the clone_args pointer isn't available
                 * post-syscall natively anyway, there's no need to restore here.
                 */
#    endif
                uint app_clone_args_size = (uint)dcontext->sys_param2;
                heap_free(dcontext, (clone3_syscall_args_t *)dcontext->sys_param1,
                          app_clone_args_size HEAPACCT(ACCT_OTHER));
            } else if (sysnum == SYS_clone) {
                set_syscall_param(dcontext, SYSCALL_PARAM_CLONE_STACK,
                                  dcontext->sys_param1);
            }
        }
        break;
    }
#elif defined(MACOS) && !defined(X64)
    case SYS_bsdthread_create: {
        /* restore stack values we clobbered */
        ASSERT(*sys_param_addr(dcontext, 0) == (reg_t)new_bsdthread_intercept);
        *sys_param_addr(dcontext, 0) = dcontext->sys_param0;
        *sys_param_addr(dcontext, 1) = dcontext->sys_param1;
        break;
    }
#endif

#ifdef SYS_fork
    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork returned " PFX "\n",
            MCXT_SYSCALL_RES(mc));
        break;
    }
#endif

#ifdef SYS_vfork
    case SYS_vfork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork returned " PFX "\n",
            MCXT_SYSCALL_RES(mc));
        IF_LINUX(ASSERT(was_thread_create_syscall(dcontext)));
        /* restore xsp in parent */
        LOG(THREAD, LOG_SYSCALLS, 2, "vfork: restoring xsp from " PFX " to " PFX "\n",
            mc->xsp, dcontext->sys_param1);
        mc->xsp = dcontext->sys_param1;

        if (MCXT_SYSCALL_RES(mc) != 0) {
            /* We switch the lib tls segment back to dr's segment.
             * Please refer to comment on os_switch_lib_tls.
             * It is only called in parent thread.
             * The child thread's tls setup is done in os_tls_app_seg_init.
             */
            if (INTERNAL_OPTION(private_loader)) {
                os_switch_lib_tls(dcontext, false /*to dr*/);
            }
            /* i#2089: we already restored the DR tls in os_clone_post() */
        }
        break;
    }
#endif

    case SYS_execve: {
        /* if we get here it means execve failed (doesn't return on success) */
        success = false;
        mark_thread_execve(dcontext->thread_record, false);
        ASSERT(result < 0);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: execve failed\n");
        handle_execve_post(dcontext);
        /* Don't 'break' as we have an ASSERT(success) just below
         * the switch(). */
        goto exit_post_system_call;
        break; /* unnecessary but good form so keep it */
    }

        /****************************************************************************/
        /* SIGNALS */

    case IF_MACOS_ELSE(SYS_sigaction, SYS_rt_sigaction): { /* 174 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigaction(int sig, const struct sigaction *act,
             struct sigaction *oact, size_t sigsetsize)
         */
        /* FIXME i#148: Handle syscall failure. */
        int sig = (int)dcontext->sys_param0;
        const kernel_sigaction_t *act = (const kernel_sigaction_t *)dcontext->sys_param1;
        prev_sigaction_t *oact = (prev_sigaction_t *)dcontext->sys_param2;
        size_t sigsetsize = (size_t)dcontext->sys_param3;
        uint res;
        res = handle_post_sigaction(dcontext, success, sig, act, oact, sigsetsize);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: %ssigaction => %d\n",
            IF_MACOS_ELSE("", "rt_"), -res);
        if (res != 0)
            set_failure_return_val(dcontext, res);
        if (!success || res != 0)
            goto exit_post_system_call;
        break;
    }
#if defined(LINUX) && !defined(X64)
    case SYS_sigaction: { /* 67 */
        int sig = (int)dcontext->sys_param0;
        const old_sigaction_t *act = (const old_sigaction_t *)dcontext->sys_param1;
        old_sigaction_t *oact = (old_sigaction_t *)dcontext->sys_param2;
        uint res = handle_post_old_sigaction(dcontext, success, sig, act, oact);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: sigaction => %d\n", -res);
        if (res != 0)
            set_failure_return_val(dcontext, res);
        if (!success || res != 0)
            goto exit_post_system_call;
        break;
    }
#endif
    case IF_MACOS_ELSE(SYS_sigprocmask, SYS_rt_sigprocmask): { /* 175 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset,
             size_t sigsetsize)
         */
        /* FIXME i#148: Handle syscall failure. */
        int status = handle_post_sigprocmask(
            dcontext, (int)dcontext->sys_param0, (kernel_sigset_t *)dcontext->sys_param1,
            (kernel_sigset_t *)dcontext->sys_param2, (size_t)dcontext->sys_param3);
        if (status != 0) {
            set_failure_return_val(dcontext, status);
        }
        break;
    }
#if defined(LINUX) && !defined(X64)
    case SYS_sigreturn: /* 119 */
#endif
    case IF_MACOS_ELSE(SYS_sigreturn, SYS_rt_sigreturn): /* 173 */
        /* there is no return value: it's just the value of eax, so avoid
         * assert below
         */
        success = true;
        break;

    case SYS_setitimer: /* 104 */
        handle_post_setitimer(dcontext, success, (int)dcontext->sys_param0,
                              (const struct itimerval *)dcontext->sys_param1,
                              (struct itimerval *)dcontext->sys_param2);
        break;
    case SYS_getitimer: /* 105 */
        handle_post_getitimer(dcontext, success, (int)dcontext->sys_param0,
                              (struct itimerval *)dcontext->sys_param1);
        break;
#if defined(LINUX) && defined(X86)
    case SYS_alarm: /* 27 on x86 and 37 on x64 */
        handle_post_alarm(dcontext, success, (unsigned int)dcontext->sys_param0);
        break;
#endif
#if defined(LINUX) && defined(X86) && defined(X64)
    case SYS_arch_prctl: {
        if (success && INTERNAL_OPTION(mangle_app_seg)) {
            tls_handle_post_arch_prctl(dcontext, dcontext->sys_param0,
                                       dcontext->sys_param1);
        }
        break;
    }
#endif
#ifdef LINUX
#    ifdef SYS_ppoll_time64
    case SYS_ppoll_time64:
#    endif
    case SYS_ppoll: {
        if (dcontext->sys_param3 == (reg_t)NULL)
            break;
        handle_post_extended_syscall_sigmasks(dcontext, success);
        set_syscall_param(dcontext, 3, dcontext->sys_param3);
        break;
    }
#    ifdef SYS_pselect6_time64
    case SYS_pselect6_time64:
#    endif
    case SYS_pselect6: {
        if (dcontext->sys_param4 == (reg_t)NULL)
            break;
        typedef struct {
            kernel_sigset_t *sigmask;
            size_t sizemask;
        } data_t;
        data_t *data_param = (data_t *)dcontext->sys_param3;
        handle_post_extended_syscall_sigmasks(dcontext, success);
        if (!safe_write_ex((void *)&data_param->sigmask, sizeof(data_param->sigmask),
                           &dcontext->sys_param4, NULL)) {
            LOG(THREAD, LOG_SYSCALLS, 2, "\tEFAULT for pselect6 post syscall\n");
        }
        break;
    }
    case SYS_epoll_pwait: {
        if (dcontext->sys_param4 == (reg_t)NULL)
            break;
        handle_post_extended_syscall_sigmasks(dcontext, success);
        set_syscall_param(dcontext, 4, dcontext->sys_param4);
        break;
    }
#endif

    /****************************************************************************/
    /* FILES */

#ifdef SYS_dup2
    case SYS_dup2: IF_LINUX(case SYS_dup3:) {
#    ifdef LINUX
            if (success) {
                signal_handle_dup(dcontext, (file_t)sys_param(dcontext, 1),
                                  (file_t)result);
            }
#    endif
            break;
        }
#endif

#ifdef MACOS
    case SYS_fcntl_nocancel:
#endif
    case SYS_fcntl: {
#ifdef LINUX /* Linux-only since only for signalfd */
        if (success) {
            file_t fd = (long)dcontext->sys_param0;
            int cmd = (int)dcontext->sys_param1;
            if ((cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC))
                signal_handle_dup(dcontext, fd, (file_t)result);
        }
        break;
#endif
    }

    case IF_MACOS_ELSE(SYS_getrlimit, IF_X64_ELSE(SYS_getrlimit, SYS_ugetrlimit)): {
        int resource = dcontext->sys_param0;
        if (success && resource == RLIMIT_NOFILE) {
            /* we stole some space: hide it from app */
            struct rlimit *rlim = (struct rlimit *)dcontext->sys_param1;
            safe_write_ex(&rlim->rlim_cur, sizeof(rlim->rlim_cur),
                          &app_rlimit_nofile.rlim_cur, NULL);
            safe_write_ex(&rlim->rlim_max, sizeof(rlim->rlim_max),
                          &app_rlimit_nofile.rlim_max, NULL);
        }
        break;
    }
#if !defined(ARM) && !defined(X64) && !defined(MACOS)
    /* Old struct w/ smaller fields */
    case SYS_getrlimit: {
        int resource = dcontext->sys_param0;
        if (success && resource == RLIMIT_NOFILE) {
            struct compat_rlimit *rlim = (struct compat_rlimit *)dcontext->sys_param1;
            safe_write_ex(&rlim->rlim_cur, sizeof(rlim->rlim_cur),
                          &app_rlimit_nofile.rlim_cur, NULL);
            safe_write_ex(&rlim->rlim_max, sizeof(rlim->rlim_max),
                          &app_rlimit_nofile.rlim_max, NULL);
        }
        break;
    }
#endif

#ifdef LINUX
    case SYS_prlimit64: {
        int resource = dcontext->sys_param1;
        rlimit64_t *rlim = (rlimit64_t *)dcontext->sys_param3;
        if (success && resource == RLIMIT_NOFILE && rlim != NULL &&
            /* XXX: xref pid discussion in pre_system_call SYS_prlimit64 */
            (dcontext->sys_param0 == 0 || dcontext->sys_param0 == get_process_id())) {
            safe_write_ex(rlim, sizeof(*rlim), &app_rlimit_nofile, NULL);
        }
        break;
    }
#endif

#ifdef LINUX
#    ifdef SYS_readlink
    case SYS_readlink:
#    endif
    case SYS_readlinkat:
        if (success && DYNAMO_OPTION(early_inject)) {
            bool is_at = (sysnum == SYS_readlinkat);
            /* i#907: /proc/self/exe is a symlink to libdynamorio.so.  We need
             * to fix it up if the app queries.  Any thread id can be passed to
             * /proc/%d/exe, so we have to check.  We could instead look for
             * libdynamorio.so in the result but we've tweaked our injector
             * in the past to exec different binaries so this seems more robust.
             */
            if (symlink_is_self_exe((const char *)(is_at ? dcontext->sys_param1
                                                         : dcontext->sys_param0))) {
                char *tgt = (char *)(is_at ? dcontext->sys_param2 : dcontext->sys_param1);
                size_t tgt_sz =
                    (size_t)(is_at ? dcontext->sys_param3 : dcontext->sys_param2);
                int len = snprintf(tgt, tgt_sz, "%s", get_application_name());
                if (len > 0)
                    set_success_return_val(dcontext, len);
                else {
                    set_failure_return_val(dcontext, EINVAL);
                    DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                }
            }
        }
        break;
#    ifdef SYS_openat2
    case SYS_openat2:
#    endif
    case SYS_openat:
        if (dcontext->sys_param0 != 0) {
            heap_free(dcontext, (void *)dcontext->sys_param0,
                      MAXIMUM_PATH HEAPACCT(ACCT_OTHER));
        }
        break;
    case SYS_rseq:
        /* Lazy rseq handling. */
        if (success) {
            rseq_process_syscall(dcontext);
        }
        break;
#endif
    default:
#ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(sysnum)) {
            vmkuw_post_system_call(dcontext);
            break;
        }
#endif
        break;

    } /* switch */

    DODEBUG({
        if (ignorable_system_call_normalized(sysnum)) {
            STATS_INC(post_syscall_ignorable);
        } else {
            /* Many syscalls can fail though they aren't ignored.  However, they
             * shouldn't happen without us knowing about them.  See PR 402769
             * for SYS_close case.
             */
            if (!(success || sysnum == SYS_close ||
                  IF_MACOS(sysnum == SYS_close_nocancel ||)
                      dcontext->expect_last_syscall_to_fail)) {
                LOG(THREAD, LOG_SYSCALLS, 1,
                    "Unexpected failure of non-ignorable syscall %d\n", sysnum);
            }
        }
    });

exit_post_system_call:

    /* The instrument_post_syscall should be called after DR finishes all
     * its operations, since DR needs to know the real syscall results,
     * and any changes made by the client are simply to fool the app.
     * Also, dr_syscall_invoke_another() needs to set eax, which shouldn't
     * affect the result of the 1st syscall. Xref i#1.
     */
    /* after restore of xbp so client sees it as though was sysenter */
    instrument_post_syscall(dcontext, sysnum);

    dcontext->whereami = old_whereami;
}

#ifdef LINUX
#    ifdef STATIC_LIBRARY
/* Static libraries may optionally define two linker variables
 * (dynamorio_so_start and dynamorio_so_end) to help mitigate
 * edge cases in detecting DR's library bounds. They are optional.
 *
 * If not specified, the variables' location will default to
 * weak_dynamorio_so_bounds_filler and they will not be used.
 * Note that referencing the value of these symbols will crash:
 * always use the address only.
 */
extern int dynamorio_so_start WEAK
    __attribute__((alias("weak_dynamorio_so_bounds_filler")));
extern int dynamorio_so_end WEAK
    __attribute__((alias("weak_dynamorio_so_bounds_filler")));
static int weak_dynamorio_so_bounds_filler;

#    else  /* !STATIC_LIBRARY */
/* For non-static linux we always get our bounds from linker-provided symbols.
 * Note that referencing the value of these symbols will crash: always use the
 * address only.
 */
extern int dynamorio_so_start, dynamorio_so_end;
#    endif /* STATIC_LIBRARY */
#endif     /* LINUX */

/* get_dynamo_library_bounds initializes dynamorio library bounds, using a
 * release-time assert if there is a problem doing so. It does not use any
 * heap, and we assume it is called prior to find_executable_vm_areas in a
 * single thread.
 */
static void
get_dynamo_library_bounds(void)
{
    if (dynamorio_library_filepath[0] != '\0') /* Already cached. */
        return;
    /* Note that we're not counting DYNAMORIO_PRELOAD_NAME as a DR area, to match
     * Windows, so we should unload it like we do there. The other reason not to
     * count it is so is_in_dynamo_dll() can be the only exception to the
     * never-execute-from-DR-areas list rule
     */
    app_pc check_start, check_end;
    bool do_memquery = true;
#ifdef STATIC_LIBRARY
#    ifdef LINUX
    /* For static+linux, we might have linker vars to help us and we definitely
     * know our "library name" since we are in the app. When we have both we
     * don't need to do a memquery.
     */
    if (&dynamorio_so_start != &weak_dynamorio_so_bounds_filler &&
        &dynamorio_so_end != &weak_dynamorio_so_bounds_filler) {

        do_memquery = false;
        dynamo_dll_start = (app_pc)&dynamorio_so_start;
        dynamo_dll_end = (app_pc)ALIGN_FORWARD(&dynamorio_so_end, PAGE_SIZE);
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "Using dynamorio_so_start and dynamorio_so_end for library bounds"
            "\n");
        const char *dr_path = get_application_name();
        strncpy(dynamorio_library_filepath, dr_path,
                BUFFER_SIZE_ELEMENTS(dynamorio_library_filepath));
        NULL_TERMINATE_BUFFER(dynamorio_library_filepath);

        const char *slash = strrchr(dr_path, '/');
        ASSERT(slash != NULL);
        /* Include the slash in the library path */
        size_t copy_chars = 1 + slash - dr_path;
        ASSERT(copy_chars < BUFFER_SIZE_ELEMENTS(dynamorio_library_path));
        strncpy(dynamorio_library_path, dr_path, copy_chars);
        dynamorio_library_path[copy_chars] = '\0';
    }
#    endif
    if (do_memquery) {
        /* No linker vars, so we need to find bound using an internal PC */
        check_start = (app_pc)&get_dynamo_library_bounds;
    }
#else /* !STATIC_LIBRARY */
#    ifdef LINUX
    /* PR 361594: we get our bounds from linker-provided symbols.
     * Note that referencing the value of these symbols will crash:
     * always use the address only.
     */
    extern int dynamorio_so_start, dynamorio_so_end;
    dynamo_dll_start = (app_pc)&dynamorio_so_start;
    dynamo_dll_end = (app_pc)ALIGN_FORWARD(&dynamorio_so_end, PAGE_SIZE);
#    elif defined(MACOS)
    dynamo_dll_start = module_dynamorio_lib_base();
#    endif
    check_start = dynamo_dll_start;
#endif /* STATIC_LIBRARY */

    if (do_memquery) {
        DEBUG_DECLARE(int res =)
        memquery_library_bounds(NULL, &check_start, &check_end, dynamorio_library_path,
                                BUFFER_SIZE_ELEMENTS(dynamorio_library_path),
                                dynamorio_libname_buf,
                                BUFFER_SIZE_ELEMENTS(dynamorio_libname_buf));
        ASSERT(res > 0);
#ifndef STATIC_LIBRARY
        dynamorio_libname = IF_UNIT_TEST_ELSE(UNIT_TEST_EXE_NAME, dynamorio_libname_buf);
#endif /* STATIC_LIBRARY */

        snprintf(dynamorio_library_filepath,
                 BUFFER_SIZE_ELEMENTS(dynamorio_library_filepath), "%s%s",
                 dynamorio_library_path, dynamorio_libname);
        NULL_TERMINATE_BUFFER(dynamorio_library_filepath);
#if !defined(STATIC_LIBRARY) && defined(LINUX)
        ASSERT(check_start == dynamo_dll_start && check_end == dynamo_dll_end);
#elif defined(MACOS)
        ASSERT(check_start == dynamo_dll_start);
        dynamo_dll_end = check_end;
#else
        dynamo_dll_start = check_start;
        dynamo_dll_end = check_end;
#endif
    }

    LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " library path: %s\n",
        dynamorio_library_path);
    LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " library file path: %s\n",
        dynamorio_library_filepath);
    LOG(GLOBAL, LOG_VMAREAS, 1, "DR library bounds: " PFX " to " PFX "\n",
        dynamo_dll_start, dynamo_dll_end);

    if (dynamo_dll_start == NULL || dynamo_dll_end == NULL) {
        REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_FIND_DR_BOUNDS, 2, get_application_name(),
                                    get_application_pid());
    }
}

/* Determines and caches the alternative-bitwidth libdynamorio path.
 * Assumed to be called at single-threaded initialization time as it sets
 * global buffers.
 * get_dynamo_library_bounds() must be called first.
 */
static void
get_alt_dynamo_library_bounds(void)
{
    if (dynamorio_alt_arch_filepath[0] != '\0') /* Already cached. */
        return;
    /* Set by get_dynamo_library_bounds(). */
    ASSERT(dynamorio_library_path[0] != '\0');
    ASSERT(d_r_config_initialized());

    const char *config_alt_path = get_config_val(DYNAMORIO_VAR_ALTINJECT);
    if (config_alt_path != NULL && config_alt_path[0] != '\0') {
        strncpy(dynamorio_alt_arch_filepath, config_alt_path,
                BUFFER_SIZE_ELEMENTS(dynamorio_alt_arch_filepath));
        NULL_TERMINATE_BUFFER(dynamorio_alt_arch_filepath);
        /* We don't really need just the dir (used for the old LD_PRELOAD) but
         * we compute it for the legacy code.
         */
        const char *sep = strrchr(dynamorio_alt_arch_filepath, '/');
        if (sep != NULL) {
            strncpy(dynamorio_alt_arch_path, dynamorio_alt_arch_filepath,
                    sep - dynamorio_alt_arch_filepath);
            NULL_TERMINATE_BUFFER(dynamorio_alt_arch_path);
        }
        LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " alt arch filepath: %s\n",
            dynamorio_alt_arch_filepath);
        LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " alt arch path: %s\n",
            dynamorio_alt_arch_path);
    } else {
        /* Construct a path under assumptions of build-time path names. */
        strncpy(dynamorio_alt_arch_path, dynamorio_library_path,
                BUFFER_SIZE_ELEMENTS(dynamorio_alt_arch_path));
        /* Assumption: libdir name is not repeated elsewhere in path */
        char *libdir =
            strstr(dynamorio_alt_arch_path, IF_X64_ELSE(DR_LIBDIR_X64, DR_LIBDIR_X86));
        if (libdir != NULL) {
            const char *newdir = IF_X64_ELSE(DR_LIBDIR_X86, DR_LIBDIR_X64);
            /* do NOT place the NULL */
            strncpy(libdir, newdir, strlen(newdir));
        } else {
            SYSLOG_INTERNAL_WARNING("unable to determine lib path for cross-arch execve");
        }
        NULL_TERMINATE_BUFFER(dynamorio_alt_arch_path);
        LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " alt arch path: %s\n",
            dynamorio_alt_arch_path);
        snprintf(dynamorio_alt_arch_filepath,
                 BUFFER_SIZE_ELEMENTS(dynamorio_alt_arch_filepath), "%s%s",
                 dynamorio_alt_arch_path, dynamorio_libname);
        NULL_TERMINATE_BUFFER(dynamorio_alt_arch_filepath);
        LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME " alt arch filepath: %s\n",
            dynamorio_alt_arch_filepath);
    }
}

/* get full path to our own library, (cached), used for forking and message file name */
char *
get_dynamorio_library_path(void)
{
    if (!dynamorio_library_filepath[0]) { /* not cached */
        get_dynamo_library_bounds();
    }
    return dynamorio_library_filepath;
}

#ifdef LINUX
/* Get full path+name of executable file from /proc/self/exe.  Returns an empty
 * string on error.
 * FIXME i#47: This will return DR's path when using early injection.
 */
static char *
read_proc_self_exe(bool ignore_cache)
{
    static char exepath[MAXIMUM_PATH];
    static bool tried = false;
#    ifdef MACOS
    ASSERT_NOT_IMPLEMENTED(false);
#    endif
    if (!tried || ignore_cache) {
        tried = true;
        /* assume we have /proc/self/exe symlink: could add HAVE_PROC_EXE
         * but we have no alternative solution except assuming the first
         * /proc/self/maps entry is the executable
         */
        ssize_t res;
        DEBUG_DECLARE(int len =)
        snprintf(exepath, BUFFER_SIZE_ELEMENTS(exepath), "/proc/%d/exe",
                 get_process_id());
        ASSERT(len > 0);
        NULL_TERMINATE_BUFFER(exepath);
        /* i#960: readlink does not null terminate, so we do it. */
#    ifdef SYS_readlink
        res = dynamorio_syscall(SYS_readlink, 3, exepath, exepath,
                                BUFFER_SIZE_ELEMENTS(exepath) - 1);
#    else
        res = dynamorio_syscall(SYS_readlinkat, 4, AT_FDCWD, exepath, exepath,
                                BUFFER_SIZE_ELEMENTS(exepath) - 1);
#    endif
        ASSERT(res < BUFFER_SIZE_ELEMENTS(exepath));
        exepath[MAX(res, 0)] = '\0';
        NULL_TERMINATE_BUFFER(exepath);
    }
    return exepath;
}
#endif /* LINUX */

app_pc
get_application_base(void)
{
    if (executable_start == NULL) {
#if defined(STATIC_LIBRARY)
        /* When compiled statically, the app and the DR's "library" are the same. */
        executable_start = get_dynamorio_dll_start();
        executable_end = get_dynamorio_dll_end();
#elif defined(HAVE_MEMINFO)
        /* Haven't done find_executable_vm_areas() yet so walk maps ourselves */
        const char *name = get_application_name();
        if (name != NULL && name[0] != '\0') {
            DEBUG_DECLARE(int count =)
            memquery_library_bounds(name, &executable_start, &executable_end, NULL, 0,
                                    NULL, 0);
            ASSERT(count > 0 && executable_start != NULL);
        }
#else
        /* We have to fail.  Should we dl_iterate this early? */
#endif
    }
    return executable_start;
}

app_pc
get_application_end(void)
{
    if (executable_end == NULL)
        get_application_base();
    return executable_end;
}

app_pc
get_image_entry()
{
    static app_pc image_entry_point = NULL;
    if (image_entry_point == NULL && executable_start != NULL) {
        module_area_t *ma;
        os_get_module_info_lock();
        ma = module_pc_lookup(executable_start);
        ASSERT(ma != NULL);
        if (ma != NULL) {
            ASSERT(executable_start == ma->start);
            SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            image_entry_point = ma->entry_point;
            SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        }
        os_get_module_info_unlock();
    }
    return image_entry_point;
}

#ifdef DEBUG
void
mem_stats_snapshot()
{
    /* FIXME: NYI */
}
#endif

bool
is_in_dynamo_dll(app_pc pc)
{
    ASSERT(dynamo_dll_start != NULL);
#ifdef VMX86_SERVER
    /* We want to consider vmklib as part of the DR lib for allowing
     * execution (_init calls os_in_vmkernel_classic()) and for
     * reporting crashes as our fault
     */
    if (vmk_in_vmklib(pc))
        return true;
#endif
    return (pc >= dynamo_dll_start && pc < dynamo_dll_end);
}

app_pc
get_dynamorio_dll_start()
{
    if (dynamo_dll_start == NULL)
        get_dynamo_library_bounds();
    ASSERT(dynamo_dll_start != NULL);
    return dynamo_dll_start;
}

app_pc
get_dynamorio_dll_end()
{
    if (dynamo_dll_end == NULL)
        get_dynamo_library_bounds();
    ASSERT(dynamo_dll_end != NULL);
    return dynamo_dll_end;
}

app_pc
get_dynamorio_dll_preferred_base()
{
    /* on Linux there is no preferred base if we're PIC,
     * therefore is always equal to dynamo_dll_start  */
    return get_dynamorio_dll_start();
}

static void
found_vsyscall_page(memquery_iter_t *iter _IF_DEBUG(OUT const char **map_type))
{
#ifndef X64
    /* We assume no vsyscall page for x64; thus, checking the
     * hardcoded address shouldn't have any false positives.
     */
    ASSERT(iter->vm_end - iter->vm_start == PAGE_SIZE ||
           /* i#1583: recent kernels have 2-page vdso */
           iter->vm_end - iter->vm_start == 2 * PAGE_SIZE);
    ASSERT(!dynamo_initialized); /* .data should be +w */
    /* we're not considering as "image" even if part of ld.so (xref i#89) and
     * thus we aren't adjusting our code origins policies to remove the
     * vsyscall page exemption.
     */
    DODEBUG({ *map_type = "VDSO"; });
    /* On re-attach, the vdso can be split into two entries (from DR's hook),
     * so take just the first one as the start (xref i#2157).
     */
    if (vdso_page_start == NULL) {
        vdso_page_start = iter->vm_start;
        vdso_size = iter->vm_end - iter->vm_start;
    }
    /* The vsyscall page can be on the 2nd page inside the vdso, but until we
     * see a syscall we don't know and we point it at the vdso start.
     */
    if (vsyscall_page_start == NULL)
        vsyscall_page_start = iter->vm_start;
    LOG(GLOBAL, LOG_VMAREAS, 1, "found vdso/vsyscall pages @ " PFX " %s\n",
        vsyscall_page_start, iter->comment);
#else
    /* i#172
     * fix bugs for OS where vdso page is set unreadable as below
     * ffffffffff600000-ffffffffffe00000 ---p 00000000 00:00 0 [vdso]
     * but it is readable indeed.
     */
    /* i#430
     * fix bugs for OS where vdso page is set unreadable as below
     * ffffffffff600000-ffffffffffe00000 ---p 00000000 00:00 0 [vsyscall]
     * but it is readable indeed.
     */
    if (!TESTALL((PROT_READ | PROT_EXEC), iter->prot))
        iter->prot |= (PROT_READ | PROT_EXEC);
    /* i#1908: vdso and vsyscall pages are now split */
    if (strncmp(iter->comment, VSYSCALL_PAGE_MAPS_NAME,
                strlen(VSYSCALL_PAGE_MAPS_NAME)) == 0)
        vdso_page_start = iter->vm_start;
    else if (strncmp(iter->comment, VSYSCALL_REGION_MAPS_NAME,
                     strlen(VSYSCALL_REGION_MAPS_NAME)) == 0)
        vsyscall_page_start = iter->vm_start;
#endif
}

#ifndef HAVE_MEMINFO_QUERY
static void
add_to_memcache(byte *region_start, byte *region_end, void *user_data)
{
    memcache_update_locked(region_start, region_end, MEMPROT_NONE, DR_MEMTYPE_DATA,
                           false /*!exists*/);
}
#endif

int
os_walk_address_space(memquery_iter_t *iter, bool add_modules)
{
    int count = 0;
#ifdef MACOS
    app_pc shared_start, shared_end;
    bool have_shared = module_dyld_shared_region(&shared_start, &shared_end);
#endif
#ifdef RETURN_AFTER_CALL
    dcontext_t *dcontext = get_thread_private_dcontext();
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
#endif

#ifndef HAVE_MEMINFO_QUERY
    /* We avoid tracking the innards of vmheap for all_memory_areas by
     * adding a single no-access region for the whole vmheap.
     * Queries from heap routines use _from_os.
     * Queries in check_thread_vm_area are fine getting "noaccess": wants
     * any DR memory not on exec areas list to be noaccess.
     * Queries from clients: should be ok to hide innards.  Marking noaccess
     * should be safer than marking free, as unruly client might try to mmap
     * something in the free space: better to have it think it's reserved but
     * not yet used memory.  FIXME: we're not marking beyond-vmheap DR regions
     * as noaccess!
     */
    iterate_vmm_regions(add_to_memcache, NULL);
#endif

#ifndef HAVE_MEMINFO
    count = find_vm_areas_via_probe();
#else
    while (memquery_iterator_next(iter)) {
        bool image = false;
        size_t size = iter->vm_end - iter->vm_start;
        /* i#479, hide private module and match Windows's behavior */
        bool skip = dynamo_vm_area_overlap(iter->vm_start, iter->vm_end) &&
            !is_in_dynamo_dll(iter->vm_start) /* our own text section is ok */
            /* client lib text section is ok (xref i#487) */
            && !is_in_client_lib(iter->vm_start);
        DEBUG_DECLARE(const char *map_type = "Private");
        /* we can't really tell what's a stack and what's not, but we rely on
         * our passing NULL preventing rwx regions from being added to executable
         * or future list, even w/ -executable_if_alloc
         */

        LOG(GLOBAL, LOG_VMAREAS, 2, "start=" PFX " end=" PFX " prot=%x comment=%s\n",
            iter->vm_start, iter->vm_end, iter->prot, iter->comment);
        /* Issue 89: the vdso might be loaded inside ld.so as below,
         * which causes ASSERT_CURIOSITY in mmap_check_for_module_overlap fail.
         * b7fa3000-b7fbd000 r-xp 00000000 08:01 108679     /lib/ld-2.8.90.so
         * b7fbd000-b7fbe000 r-xp b7fbd000 00:00 0          [vdso]
         * b7fbe000-b7fbf000 r--p 0001a000 08:01 108679     /lib/ld-2.8.90.so
         * b7fbf000-b7fc0000 rw-p 0001b000 08:01 108679     /lib/ld-2.8.90.so
         * So we always first check if it is a vdso page before calling
         * mmap_check_for_module_overlap.
         * Update: with i#160/PR 562667 handling non-contiguous modules like
         * ld.so we now gracefully handle other objects like vdso in gaps in
         * module, but it's simpler to leave this ordering here.
         */
        if (skip) {
            /* i#479, hide private module and match Windows's behavior */
            LOG(GLOBAL, LOG_VMAREAS, 2, PFX "-" PFX " skipping: internal DR region\n",
                iter->vm_start, iter->vm_end);
#    ifdef MACOS
        } else if (have_shared && iter->vm_start >= shared_start &&
                   iter->vm_start < shared_end) {
            /* Skip modules we happen to find inside the dyld shared cache,
             * as we'll fail to identify the library.  We add them
             * in module_walk_dyld_list instead.
             */
            image = true;
#    endif
        } else if (strncmp(iter->comment, VSYSCALL_PAGE_MAPS_NAME,
                           strlen(VSYSCALL_PAGE_MAPS_NAME)) == 0 ||
                   IF_X64_ELSE(strncmp(iter->comment, VSYSCALL_REGION_MAPS_NAME,
                                       strlen(VSYSCALL_REGION_MAPS_NAME)) == 0,
                               /* Older kernels do not label it as "[vdso]", but it is
                                * hardcoded there.
                                */
                               /* 32-bit */
                               iter->vm_start == VSYSCALL_PAGE_START_HARDCODED)) {
            if (add_modules) {
                found_vsyscall_page(iter _IF_DEBUG(&map_type));
                /* We'd like to add vsyscall to the module list too but when it's
                 * separate from vdso it has no ELF header which is too complex
                 * to force into the module list.
                 */
                if (module_is_header(iter->vm_start, iter->vm_end - iter->vm_start)) {
                    module_list_add(iter->vm_start, iter->vm_end - iter->vm_start, false,
                                    iter->comment, iter->inode);
                }
            }
        } else if (add_modules &&
                   mmap_check_for_module_overlap(iter->vm_start, size,
                                                 TEST(MEMPROT_READ, iter->prot),
                                                 iter->inode, false)) {
            /* we already added the whole image region when we hit the first map for it */
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
        } else if (TEST(MEMPROT_READ, iter->prot) &&
                   module_is_header(iter->vm_start, size)) {
            DEBUG_DECLARE(size_t image_size = size;)
            app_pc mod_base, mod_first_end, mod_max_end;
            char *exec_match;
            bool found_exec = false;
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "Found already mapped module first segment :\n"
                "\t" PFX "-" PFX "%s inode=" UINT64_FORMAT_STRING " name=%s\n",
                iter->vm_start, iter->vm_end, TEST(MEMPROT_EXEC, iter->prot) ? " +x" : "",
                iter->inode, iter->comment);
#    ifdef LINUX
            /* Mapped images should have inodes, except for cases where an anon
             * map is placed on top (i#2566)
             */
            ASSERT_CURIOSITY(iter->inode != 0 || iter->comment[0] == '\0');
#    endif
            ASSERT_CURIOSITY(iter->offset == 0); /* first map shouldn't have offset */
            /* Get size by walking the program headers.  This includes .bss. */
            if (module_walk_program_headers(iter->vm_start, size, false,
                                            true, /* i#1589: ld.so relocated .dynamic */
                                            &mod_base, &mod_first_end, &mod_max_end, NULL,
                                            NULL)) {
                DODEBUG(image_size = mod_max_end - mod_base;);
            } else {
                ASSERT_NOT_REACHED();
            }
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "Found already mapped module total module :\n"
                "\t" PFX "-" PFX " inode=" UINT64_FORMAT_STRING " name=%s\n",
                iter->vm_start, iter->vm_start + image_size, iter->inode, iter->comment);

            if (add_modules) {
                const char *modpath = iter->comment;
                /* look for executable */
#    ifdef LINUX
                exec_match = get_application_name();
                if (exec_match != NULL && exec_match[0] != '\0')
                    found_exec = (strcmp(iter->comment, exec_match) == 0);
                /* Handle an anon region for the header (i#2566) */
                if (!found_exec && executable_start != NULL &&
                    executable_start == iter->vm_start) {
                    found_exec = true;
                    /* The maps file's first entry may not have the path, in the
                     * presence of mremapping for hugepages (i#2566; i#3387) (this
                     * could happen for libraries too, but we don't have alternatives
                     * there).  Or, it may have an incorrect path.  Prefer the path
                     * we recorded in early injection or obtained from
                     * /proc/self/exe.
                     */
                    modpath = get_application_name();
                }
#    else
                /* We don't have a nice normalized name: it can have ./ or ../ inside
                 * it.  But, we can distinguish an exe from a lib here, even for PIE,
                 * so we go with that plus a basename comparison.
                 */
                exec_match = (char *)get_application_short_name();
                if (module_is_executable(iter->vm_start) && exec_match != NULL &&
                    exec_match[0] != '\0') {
                    const char *iter_basename = strrchr(iter->comment, '/');
                    if (iter_basename == NULL)
                        iter_basename = iter->comment;
                    else
                        iter_basename++;
                    found_exec = (strcmp(iter_basename, exec_match) == 0);
                }
#    endif
                if (found_exec) {
                    if (executable_start == NULL)
                        executable_start = iter->vm_start;
                    else
                        ASSERT(iter->vm_start == executable_start);
                    LOG(GLOBAL, LOG_VMAREAS, 2,
                        "Found executable %s @" PFX "-" PFX " %s\n",
                        get_application_name(), iter->vm_start,
                        iter->vm_start + image_size, iter->comment);
                }
                /* We don't yet know whether contiguous so we have to settle for the
                 * first segment's size.  We'll update it in module_list_add().
                 */
                module_list_add(iter->vm_start, mod_first_end - mod_base, false, modpath,
                                iter->inode);

#    ifdef MACOS
                /* look for dyld */
                if (strcmp(iter->comment, "/usr/lib/dyld") == 0)
                    module_walk_dyld_list(iter->vm_start);
#    endif
            }
        } else if (iter->inode != 0) {
            DODEBUG({ map_type = "Mapped File"; });
        }

        /* add all regions (incl. dynamo_areas and stack) to all_memory_areas */
#    ifndef HAVE_MEMINFO_QUERY
        /* Don't add if we're using one single vmheap entry. */
        if (!is_vmm_reserved_address(iter->vm_start, iter->vm_end - iter->vm_start, NULL,
                                     NULL)) {
            LOG(GLOBAL, LOG_VMAREAS, 4,
                "os_walk_address_space: adding: " PFX "-" PFX " prot=%d\n",
                iter->vm_start, iter->vm_end, iter->prot);
            memcache_update_locked(iter->vm_start, iter->vm_end, iter->prot,
                                   image ? DR_MEMTYPE_IMAGE : DR_MEMTYPE_DATA,
                                   false /*!exists*/);
        }
#    endif

        /* FIXME: best if we could pass every region to vmareas, but
         * it has no way of determining if this is a stack b/c we don't have
         * a dcontext at this point -- so we just don't pass the stack
         */
        if (!skip /* i#479, hide private module and match Windows's behavior */ &&
            add_modules &&
            app_memory_allocation(NULL, iter->vm_start, (iter->vm_end - iter->vm_start),
                                  iter->prot, image _IF_DEBUG(map_type))) {
            count++;
        }
    }
#endif /* !HAVE_MEMINFO */

#ifndef HAVE_MEMINFO_QUERY
    DOLOG(4, LOG_VMAREAS, memcache_print(GLOBAL, "init: all memory areas:\n"););
#endif

#ifdef RETURN_AFTER_CALL
    /* Find the bottom of the stack of the initial (native) entry */
    ostd->stack_bottom_pc = find_stack_bottom();
    LOG(THREAD, LOG_ALL, 1, "Stack bottom pc = " PFX "\n", ostd->stack_bottom_pc);
#endif

    /* now that we've walked memory print all modules */
    LOG(GLOBAL, LOG_VMAREAS, 2, "Module list after memory walk\n");
    DOLOG(1, LOG_VMAREAS, {
        if (add_modules)
            print_modules(GLOBAL, DUMP_NOT_XML);
    });

    return count;
}

/* assumed to be called after find_dynamo_library_vm_areas() */
int
find_executable_vm_areas(void)
{
    int count;
    memquery_iter_t iter;
    memquery_iterator_start(&iter, NULL, true /*may alloc*/);
    count = os_walk_address_space(&iter, true);
    memquery_iterator_stop(&iter);

    STATS_ADD(num_app_code_modules, count);

    /* now that we have the modules set up, query libc */
    get_libc_errno_location(true /*force init*/);

    return count;
}

/* initializes dynamorio library bounds.
 * does not use any heap.
 * assumed to be called prior to find_executable_vm_areas.
 */
int
find_dynamo_library_vm_areas(void)
{
#ifndef STATIC_LIBRARY
    /* We didn't add inside get_dynamo_library_bounds b/c it was called pre-alloc.
     * We don't bother to break down the sub-regions.
     * Assumption: we don't need to have the protection flags for DR sub-regions.
     * For static library builds, DR's code is in the exe and isn't considered
     * to be a DR area.
     */
    add_dynamo_vm_area(get_dynamorio_dll_start(), get_dynamorio_dll_end(),
                       MEMPROT_READ | MEMPROT_WRITE | MEMPROT_EXEC,
                       true /* from image */ _IF_DEBUG(dynamorio_library_filepath));
#endif
#ifdef VMX86_SERVER
    if (os_in_vmkernel_userworld())
        vmk_add_vmklib_to_dynamo_areas();
#endif
    return 1;
}

bool
get_stack_bounds(dcontext_t *dcontext, byte **base, byte **top)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    if (ostd->stack_base == NULL) {
        /* initialize on-demand since don't have app esp handy in os_thread_init()
         * FIXME: the comment here -- ignoring it for now, if hit cases confirming
         * it the right thing will be to merge adjacent rwx regions and assume
         * their union is the stack -- otherwise have to have special stack init
         * routine called from x86.asm new_thread_dynamo_start and internal_dynamo_start,
         * and the latter is not a do-once...
         */
        size_t size = 0;
        bool ok;
        /* store stack info at thread startup, since stack can get fragmented in
         * /proc/self/maps w/ later mprotects and it can be hard to piece together later
         */
        if (IF_MEMQUERY_ELSE(false, DYNAMO_OPTION(use_all_memory_areas))) {
            ok = get_memory_info((app_pc)get_mcontext(dcontext)->xsp, &ostd->stack_base,
                                 &size, NULL);
        } else {
            ok = get_memory_info_from_os((app_pc)get_mcontext(dcontext)->xsp,
                                         &ostd->stack_base, &size, NULL);
        }
        if (!ok) {
            /* This can happen with dr_prepopulate_cache() before we start running
             * the app.
             */
            ASSERT(!dynamo_started);
            return false;
        }
        ostd->stack_top = ostd->stack_base + size;
        LOG(THREAD, LOG_THREADS, 1, "App stack is " PFX "-" PFX "\n", ostd->stack_base,
            ostd->stack_top);
    }
    if (base != NULL)
        *base = ostd->stack_base;
    if (top != NULL)
        *top = ostd->stack_top;
    return true;
}

#ifdef RETURN_AFTER_CALL
initial_call_stack_status_t
at_initial_stack_bottom(dcontext_t *dcontext, app_pc target_pc)
{
    /* We can't rely exclusively on finding the true stack bottom
     * b/c we can't always walk the call stack (PR 608990) so we
     * use the image entry as our primary trigger
     */
    if (executable_start != NULL /*defensive*/ && reached_image_entry_yet()) {
        return INITIAL_STACK_EMPTY;
    } else {
        /* If our stack walk ends early we could have false positives, but
         * that's better than false negatives if we miss the image entry
         * or we were unable to find the executable_start
         */
        os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
        if (target_pc == ostd->stack_bottom_pc) {
            return INITIAL_STACK_BOTTOM_REACHED;
        } else {
            return INITIAL_STACK_BOTTOM_NOT_REACHED;
        }
    }
}
#endif /* RETURN_AFTER_CALL */

/* Uses our cached data structures (if in use, else raw query) to retrieve memory info */
bool
query_memory_ex(const byte *pc, OUT dr_mem_info_t *out_info)
{
#ifdef HAVE_MEMINFO_QUERY
    return query_memory_ex_from_os(pc, out_info);
#else
    return memcache_query_memory(pc, out_info);
#endif
}

bool
query_memory_cur_base(const byte *pc, OUT dr_mem_info_t *info)
{
    return query_memory_ex(pc, info);
}

/* Use our cached data structures (if in use, else raw query) to retrieve memory info */
bool
get_memory_info(const byte *pc, byte **base_pc, size_t *size,
                uint *prot /* OUT optional, returns MEMPROT_* value */)
{
    dr_mem_info_t info;
    if (is_vmm_reserved_address((byte *)pc, 1, NULL, NULL)) {
        if (!query_memory_ex_from_os(pc, &info) || info.type == DR_MEMTYPE_FREE)
            return false;
    } else {
        if (!query_memory_ex(pc, &info) || info.type == DR_MEMTYPE_FREE)
            return false;
    }
    if (base_pc != NULL)
        *base_pc = info.base_pc;
    if (size != NULL)
        *size = info.size;
    if (prot != NULL)
        *prot = info.prot;
    return true;
}

/* We assume that this routine might be called instead of query_memory_ex()
 * b/c the caller is in a fragile location and cannot acquire locks, so
 * we try to do the same here.
 */
bool
query_memory_ex_from_os(const byte *pc, OUT dr_mem_info_t *info)
{
    bool have_type = false;
    bool res = memquery_from_os(pc, info, &have_type);
    if (!res) {
        /* No other failure types for now */
        info->type = DR_MEMTYPE_ERROR;
    } else if (res && !have_type) {
        /* We pass 0 instead of info->size b/c even if marked as +r we can still
         * get SIGBUS if beyond end of mmapped file: not uncommon if querying
         * in middle of library load before .bss fully set up (PR 528744).
         * However, if there is no fault handler, is_elf_so_header's safe_read will
         * recurse to here, so in that case we use info->size but we assume
         * it's only at init or exit and so not in the middle of a load
         * and less likely to be querying a random mmapped file.
         * The cleaner fix is to allow safe_read to work w/o a dcontext or
         * fault handling: i#350/PR 529066.
         */
        if (TEST(MEMPROT_READ, info->prot) &&
            module_is_header(info->base_pc, fault_handling_initialized ? 0 : info->size))
            info->type = DR_MEMTYPE_IMAGE;
        else {
            /* FIXME: won't quite match find_executable_vm_areas marking as
             * image: can be doubly-mapped so; don't want to count vdso; etc.
             */
            info->type = DR_MEMTYPE_DATA;
        }
    }
    return res;
}

bool
get_memory_info_from_os(const byte *pc, byte **base_pc, size_t *size,
                        uint *prot /* OUT optional, returns MEMPROT_* value */)
{
    dr_mem_info_t info;
    if (!query_memory_ex_from_os(pc, &info) || info.type == DR_MEMTYPE_FREE)
        return false;
    if (base_pc != NULL)
        *base_pc = info.base_pc;
    if (size != NULL)
        *size = info.size;
    if (prot != NULL)
        *prot = info.prot;
    return true;
}

/* in utils.c, exported only for our hack! */
extern void
deadlock_avoidance_unlock(mutex_t *lock, bool ownable);

void
mutex_wait_contended_lock(mutex_t *lock, priv_mcontext_t *mc)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool set_client_safe_for_synch =
        ((dcontext != NULL) && IS_CLIENT_THREAD(dcontext) &&
         ((mutex_t *)dcontext->client_data->client_grab_mutex == lock));
    if (mc != NULL) {
        ASSERT(dcontext != NULL);
        /* set_safe_for_sync can't be true at the same time as passing
         * an mcontext to return into: nothing would be able to reset the
         * client_thread_safe_for_sync flag.
         */
        ASSERT(!set_client_safe_for_synch);
        *get_mcontext(dcontext) = *mc;
    }

    /* i#96/PR 295561: use futex(2) if available */
    if (ksynch_kernel_support()) {
        /* Try to get the lock.  If already held, it's fine to store any value
         * > LOCK_SET_STATE (we don't rely on paired incs/decs) so that
         * the next unlocker will call mutex_notify_released_lock().
         */
        ptr_int_t res;
#ifndef LINUX /* we actually don't use this for Linux: see below */
        KSYNCH_TYPE *event = mutex_get_contended_event(lock);
        ASSERT(event != NULL && ksynch_var_initialized(event));
#endif
        while (atomic_exchange_int(&lock->lock_requests, LOCK_CONTENDED_STATE) !=
               LOCK_FREE_STATE) {
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = true;
            if (mc != NULL)
                set_synch_state(dcontext, THREAD_SYNCH_VALID_MCONTEXT);

                /* Unfortunately the synch semantics are different for Linux vs Mac.
                 * We have to use lock_requests as the futex to avoid waiting if
                 * lock_requests changes, while on Mac the underlying synch prevents
                 * a wait there.
                 */
#ifdef LINUX
            /* We'll abort the wait if lock_requests has changed at all.
             * We can't have a series of changes that result in no apparent
             * change w/o someone acquiring the lock, b/c
             * mutex_notify_released_lock() sets lock_requests to LOCK_FREE_STATE.
             */
            res = ksynch_wait(&lock->lock_requests, LOCK_CONTENDED_STATE, 0);
#else
            res = ksynch_wait(event, 0, 0);
#endif
            if (res != 0 && res != -EWOULDBLOCK)
                os_thread_yield();
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = false;
            if (mc != NULL)
                set_synch_state(dcontext, THREAD_SYNCH_NONE);

            /* we don't care whether properly woken (res==0), var mismatch
             * (res==-EWOULDBLOCK), or error: regardless, someone else
             * could have acquired the lock, so we try again
             */
        }
    } else {
        /* we now have to undo our earlier request */
        atomic_dec_and_test(&lock->lock_requests);

        while (!d_r_mutex_trylock(lock)) {
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = true;
            if (mc != NULL)
                set_synch_state(dcontext, THREAD_SYNCH_VALID_MCONTEXT);

            os_thread_yield();
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = false;
            if (mc != NULL)
                set_synch_state(dcontext, THREAD_SYNCH_NONE);
        }

#ifdef DEADLOCK_AVOIDANCE
        /* HACK: trylock's success causes it to do DEADLOCK_AVOIDANCE_LOCK, so to
         * avoid two in a row (causes assertion on owner) we unlock here
         * In the future we will remove the trylock here and this will go away.
         */
        deadlock_avoidance_unlock(lock, true);
#endif
    }

    return;
}

void
mutex_notify_released_lock(mutex_t *lock)
{
    /* i#96/PR 295561: use futex(2) if available. */
    if (ksynch_kernel_support()) {
        /* Set to LOCK_FREE_STATE to avoid concurrent lock attempts from
         * resulting in a futex_wait value match w/o anyone owning the lock
         */
        lock->lock_requests = LOCK_FREE_STATE;
        /* No reason to wake multiple threads: just one */
#ifdef LINUX
        ksynch_wake(&lock->lock_requests);
#else
        ksynch_wake(&lock->contended_event);
#endif
    } /* else nothing to do */
}

/* read_write_lock_t implementation doesn't expect the contention path
   helpers to guarantee the lock is held (unlike mutexes) so simple
   yields are still acceptable.
*/
void
rwlock_wait_contended_writer(read_write_lock_t *rwlock)
{
    os_thread_yield();
}

void
rwlock_notify_writer(read_write_lock_t *rwlock)
{
    /* nothing to do here */
}

void
rwlock_wait_contended_reader(read_write_lock_t *rwlock)
{
    os_thread_yield();
}

void
rwlock_notify_readers(read_write_lock_t *rwlock)
{
    /* nothing to do here */
}

/***************************************************************************/

/* events are un-signaled when successfully waited upon. */
typedef struct linux_event_t {
    /* Any function that sets this flag must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    KSYNCH_TYPE signaled;
    mutex_t lock;
    bool broadcast;
} linux_event_t;

/* FIXME: this routine will need to have a macro wrapper to let us
 * assign different ranks to all events for DEADLOCK_AVOIDANCE.
 * Currently a single rank seems to work.
 */
event_t
create_event(void)
{
    event_t e = (event_t)global_heap_alloc(sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
    ksynch_init_var(&e->signaled);
    ASSIGN_INIT_LOCK_FREE(e->lock, event_lock); /* FIXME: pass the event name here */
    e->broadcast = false;
    return e;
}

event_t
create_broadcast_event(void)
{
    event_t e = create_event();
    e->broadcast = true;
    return e;
}

void
destroy_event(event_t e)
{
    DELETE_LOCK(e->lock);
    ksynch_free_var(&e->signaled);
    global_heap_free(e, sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
}

void
signal_event(event_t e)
{
    d_r_mutex_lock(&e->lock);
    ksynch_set_value(&e->signaled, 1);
    if (e->broadcast)
        ksynch_wake_all(&e->signaled);
    else
        ksynch_wake(&e->signaled);
    LOG(THREAD_GET, LOG_THREADS, 3, "thread " TIDFMT " signalling event " PFX "\n",
        d_r_get_thread_id(), e);
    d_r_mutex_unlock(&e->lock);
}

void
reset_event(event_t e)
{
    d_r_mutex_lock(&e->lock);
    ksynch_set_value(&e->signaled, 0);
    LOG(THREAD_GET, LOG_THREADS, 3, "thread " TIDFMT " resetting event " PFX "\n",
        d_r_get_thread_id(), e);
    d_r_mutex_unlock(&e->lock);
}

bool
wait_for_event(event_t e, int timeout_ms)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif
    uint64 start_time, cur_time;
    if (timeout_ms > 0)
        start_time = query_time_millis();
    /* Use a user-space event on Linux, a kernel event on Windows. */
    LOG(THREAD, LOG_THREADS, 3, "thread " TIDFMT " waiting for event " PFX "\n",
        d_r_get_thread_id(), e);
    do {
        if (ksynch_get_value(&e->signaled) == 1) {
            d_r_mutex_lock(&e->lock);
            if (ksynch_get_value(&e->signaled) == 0) {
                /* some other thread beat us to it */
                LOG(THREAD, LOG_THREADS, 3,
                    "thread " TIDFMT " was beaten to event " PFX "\n",
                    d_r_get_thread_id(), e);
                d_r_mutex_unlock(&e->lock);
            } else {
                if (!e->broadcast) {
                    /* reset the event */
                    ksynch_set_value(&e->signaled, 0);
                }
                d_r_mutex_unlock(&e->lock);
                LOG(THREAD, LOG_THREADS, 3,
                    "thread " TIDFMT " finished waiting for event " PFX "\n",
                    d_r_get_thread_id(), e);
                return true;
            }
        } else {
            /* Waits only if the signaled flag is not set as 1. Return value
             * doesn't matter because the flag will be re-checked.
             */
            ksynch_wait(&e->signaled, 0, timeout_ms);
        }
        if (ksynch_get_value(&e->signaled) == 0) {
            /* If it still has to wait, give up the cpu. */
            os_thread_yield();
        }
        if (timeout_ms > 0)
            cur_time = query_time_millis();
    } while (timeout_ms <= 0 || cur_time - start_time < timeout_ms);
    return false;
}

/***************************************************************************
 * DIRECTORY ITERATOR
 */

/* These structs are written to the buf that we pass to getdents.  We can
 * iterate them by adding d_reclen to the current buffer offset and interpreting
 * that as the next entry.
 */
struct linux_dirent {
#ifdef SYS_getdents
    /* Adapted from struct old_linux_dirent in linux/fs/readdir.c: */
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[];
#else
    /* Adapted from struct linux_dirent64 in linux/include/linux/dirent.h: */
    uint64 d_ino;
    int64 d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
#endif
};

#define CURRENT_DIRENT(iter) ((struct linux_dirent *)(&iter->buf[iter->off]))

static void
os_dir_iterator_start(dir_iterator_t *iter, file_t fd)
{
    iter->fd = fd;
    iter->off = 0;
    iter->end = 0;
}

static bool
os_dir_iterator_next(dir_iterator_t *iter)
{
#ifdef MACOS
    /* We can use SYS_getdirentries, but do we even need a dir iterator?
     * On Linux it's only used to enumerate /proc/pid/task.
     */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
#else
    if (iter->off < iter->end) {
        /* Have existing dents, get the next offset. */
        iter->off += CURRENT_DIRENT(iter)->d_reclen;
        ASSERT(iter->off <= iter->end);
    }
    if (iter->off == iter->end) {
        /* Do a getdents syscall.  Unlike when reading a file, the kernel will
         * not read a partial linux_dirent struct, so we don't need to shift the
         * left over bytes to the buffer start.  See the getdents manpage for
         * the example code that this is based on.
         */
        iter->off = 0;
#    ifdef SYS_getdents
        iter->end =
            dynamorio_syscall(SYS_getdents, 3, iter->fd, iter->buf, sizeof(iter->buf));
#    else
        iter->end =
            dynamorio_syscall(SYS_getdents64, 3, iter->fd, iter->buf, sizeof(iter->buf));
#    endif
        ASSERT(iter->end <= sizeof(iter->buf));
        if (iter->end <= 0) { /* No more dents, or error. */
            iter->name = NULL;
            if (iter->end < 0) {
                LOG(GLOBAL, LOG_SYSCALLS, 1, "getdents syscall failed with errno %d\n",
                    -iter->end);
            }
            return false;
        }
    }
    iter->name = CURRENT_DIRENT(iter)->d_name;
    return true;
#endif
}

/***************************************************************************
 * THREAD TAKEOVER
 */

/* Record used to synchronize thread takeover. */
typedef struct _takeover_record_t {
    thread_id_t tid;
    event_t event;
} takeover_record_t;

/* When attempting thread takeover, we store an array of thread id and event
 * pairs here.  Each thread we signal is supposed to enter DR control and signal
 * this event after it has added itself to all_threads.
 *
 * XXX: What we really want is to be able to use SYS_rt_tgsigqueueinfo (Linux >=
 * 2.6.31) to pass the event_t to each thread directly, rather than using this
 * side data structure.
 */
static takeover_record_t *thread_takeover_records;
static uint num_thread_takeover_records;

/* This is the dcontext of the thread that initiated the takeover.  We read the
 * owning_thread and signal_field threads from it in the signaled threads to
 * set up siginfo sharing.
 */
static dcontext_t *takeover_dcontext;

/* Lists active threads in the process.
 * XXX: The /proc man page says /proc/pid/task is only available if the main
 * thread is still alive, but experiments on 2.6.38 show otherwise.
 */
static thread_id_t *
os_list_threads(dcontext_t *dcontext, uint *num_threads_out)
{
    dir_iterator_t iter;
    file_t task_dir;
    uint tids_alloced = 10;
    uint num_threads = 0;
    thread_id_t *new_tids;
    thread_id_t *tids;

    ASSERT(num_threads_out != NULL);

#ifdef MACOS
    /* XXX i#58: NYI.
     * We may want SYS_proc_info with PROC_INFO_PID_INFO and PROC_PIDLISTTHREADS,
     * or is that just BSD threads and instead we want process_set_tasks()
     * and task_info() as in 7.3.1.3 in Singh's OSX book?
     */
    *num_threads_out = 0;
    return NULL;
#endif

    tids =
        HEAP_ARRAY_ALLOC(dcontext, thread_id_t, tids_alloced, ACCT_THREAD_MGT, PROTECTED);
    task_dir = os_open_directory("/proc/self/task", OS_OPEN_READ);
    ASSERT(task_dir != INVALID_FILE);
    os_dir_iterator_start(&iter, task_dir);
    while (os_dir_iterator_next(&iter)) {
        thread_id_t tid;
        DEBUG_DECLARE(int r;)
        if (strcmp(iter.name, ".") == 0 || strcmp(iter.name, "..") == 0)
            continue;
        IF_DEBUG(r =)
        sscanf(iter.name, "%u", &tid);
        ASSERT_MESSAGE(CHKLVL_ASSERTS, "failed to parse /proc/pid/task entry", r == 1);
        if (tid <= 0)
            continue;
        if (num_threads == tids_alloced) {
            /* realloc, essentially.  Less expensive than counting first. */
            new_tids = HEAP_ARRAY_ALLOC(dcontext, thread_id_t, tids_alloced * 2,
                                        ACCT_THREAD_MGT, PROTECTED);
            memcpy(new_tids, tids, sizeof(thread_id_t) * tids_alloced);
            HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, tids_alloced, ACCT_THREAD_MGT,
                            PROTECTED);
            tids = new_tids;
            tids_alloced *= 2;
        }
        tids[num_threads++] = tid;
    }
    ASSERT(iter.end == 0); /* No reading errors. */
    os_close(task_dir);

    /* realloc back down to num_threads for caller simplicity. */
    new_tids =
        HEAP_ARRAY_ALLOC(dcontext, thread_id_t, num_threads, ACCT_THREAD_MGT, PROTECTED);
    memcpy(new_tids, tids, sizeof(thread_id_t) * num_threads);
    HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, tids_alloced, ACCT_THREAD_MGT,
                    PROTECTED);
    tids = new_tids;
    *num_threads_out = num_threads;
    return tids;
}

/* List the /proc/self/task directory and add all unknown thread ids to the
 * all_threads hashtable in dynamo.c.  Returns true if we found any unknown
 * threads and false otherwise.  We assume that since we don't know about them
 * they are not under DR and have no dcontexts.
 */
bool
os_take_over_all_unknown_threads(dcontext_t *dcontext)
{
    uint i;
    uint num_threads;
    thread_id_t *tids;
    uint threads_to_signal = 0, threads_timed_out = 0;

    /* We do not want to re-takeover a thread that's in between notifying us on
     * the last call to this routine and getting onto the all_threads list as
     * we'll self-interpret our own code leading to a lot of problems.
     * XXX: should we use an event to avoid this inefficient loop?  We expect
     * this to only happen in rare cases during attach when threads are in flux.
     */
    while (uninit_thread_count > 0) /* relying on volatile */
        os_thread_yield();

    /* This can only happen if we had already taken over a thread, because there is
     * full synchronization at detach. The same thread may now already be on its way
     * to exit, and its thread record might be gone already and make it look like a
     * new native thread below. If we rely on the thread to self-detect that it was
     * interrupted at a DR address we may run into a deadlock (i#2694). In order to
     * avoid this, we wait here. This is expected to be uncommon, and can only happen
     * with very short-lived threads.
     * XXX: if this loop turns out to be too inefficient, we could support detecting
     * the lock function's address bounds along w/ is_dynamo_address.
     */
    while (exiting_thread_count > 0)
        os_thread_yield();

    d_r_mutex_lock(&thread_initexit_lock);
    CLIENT_ASSERT(thread_takeover_records == NULL,
                  "Only one thread should attempt app take over!");

#ifdef LINUX
    /* Check this thread for rseq in between setup and start. */
    if (rseq_is_registered_for_current_thread())
        rseq_locate_rseq_regions(false);
#endif

    /* Find tids for which we have no thread record, meaning they are not under
     * our control.  Shift them to the beginning of the tids array.
     */
    tids = os_list_threads(dcontext, &num_threads);
    if (tids == NULL) {
        d_r_mutex_unlock(&thread_initexit_lock);
        return false; /* have to assume no unknown */
    }
    for (i = 0; i < num_threads; i++) {
        thread_record_t *tr = thread_lookup(tids[i]);
        if (tr == NULL ||
            /* Re-takeover known threads that are currently native as well.
             * XXX i#95: we need a synchall-style loop for known threads as
             * they can be in DR for syscall hook handling.
             * Update: we now remove the hook for start/stop: but native_exec
             * or other individual threads going native could still hit this.
             */
            (is_thread_currently_native(tr) && !IS_CLIENT_THREAD(tr->dcontext)))
            tids[threads_to_signal++] = tids[i];
    }

    LOG(GLOBAL, LOG_THREADS, 1, "TAKEOVER: %d threads to take over\n", threads_to_signal);
    if (threads_to_signal > 0) {
        takeover_record_t *records;

        /* Assuming pthreads, prepare signal_field for sharing. */
        handle_clone(dcontext, PTHREAD_CLONE_FLAGS);

        /* Create records with events for all the threads we want to signal. */
        LOG(GLOBAL, LOG_THREADS, 1, "TAKEOVER: publishing takeover records\n");
        records = HEAP_ARRAY_ALLOC(dcontext, takeover_record_t, threads_to_signal,
                                   ACCT_THREAD_MGT, PROTECTED);
        for (i = 0; i < threads_to_signal; i++) {
            LOG(GLOBAL, LOG_THREADS, 1, "TAKEOVER: will signal thread " TIDFMT "\n",
                tids[i]);
            records[i].tid = tids[i];
            records[i].event = create_event();
        }

        /* Publish the records and the initial take over dcontext. */
        thread_takeover_records = records;
        num_thread_takeover_records = threads_to_signal;
        takeover_dcontext = dcontext;

        /* Signal the other threads. */
        for (i = 0; i < threads_to_signal; i++) {
            send_suspend_signal(NULL, get_process_id(), records[i].tid);
        }
        d_r_mutex_unlock(&thread_initexit_lock);

        /* Wait for all the threads we signaled. */
        ASSERT_OWN_NO_LOCKS();
        for (i = 0; i < threads_to_signal; i++) {
            static const int progress_period = 50;
            if (i % progress_period == 0) {
                char buf[16];
                /* +1 to include the attach request thread to match the final msg. */
                snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%d/%d", i + 1,
                         threads_to_signal + 1);
                NULL_TERMINATE_BUFFER(buf);
                SYSLOG(SYSLOG_VERBOSE, INFO_ATTACHED, 3, buf, get_application_name(),
                       get_application_pid());
            }
            /* We split the wait up so that we'll break early on an exited thread. */
            static const int wait_ms = 25;
            int max_attempts =
                /* Integer division rounding down is fine since we always wait 25ms. */
                DYNAMO_OPTION(takeover_timeout_ms) / wait_ms;
            int attempts = 0;
            while (!wait_for_event(records[i].event, wait_ms)) {
                /* The thread may have exited (i#2601).  We assume no tid re-use. */
                char task[64];
                snprintf(task, BUFFER_SIZE_ELEMENTS(task), "/proc/self/task/%d", tids[i]);
                NULL_TERMINATE_BUFFER(task);
                if (!os_file_exists(task, false /*!is dir*/)) {
                    SYSLOG_INTERNAL_WARNING_ONCE("thread exited while attaching");
                    break;
                }
                if (++attempts > max_attempts) {
                    if (DYNAMO_OPTION(unsafe_ignore_takeover_timeout)) {
                        SYSLOG(
                            SYSLOG_VERBOSE, THREAD_TAKEOVER_TIMED_OUT, 3,
                            get_application_name(), get_application_pid(),
                            "Continuing since -unsafe_ignore_takeover_timeout is set.");
                        ++threads_timed_out;
                    } else {
                        SYSLOG(
                            SYSLOG_VERBOSE, THREAD_TAKEOVER_TIMED_OUT, 3,
                            get_application_name(), get_application_pid(),
                            "Aborting. Use -unsafe_ignore_takeover_timeout to ignore.");
                        REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_TAKE_OVER_THREADS, 2,
                                                    get_application_name(),
                                                    get_application_pid());
                    }
                    break;
                }
                /* Else try again. */
            }
        }

        /* Now that we've taken over the other threads, we can safely free the
         * records and reset the shared globals.
         */
        d_r_mutex_lock(&thread_initexit_lock);
        LOG(GLOBAL, LOG_THREADS, 1,
            "TAKEOVER: takeover complete, unpublishing records\n");
        thread_takeover_records = NULL;
        num_thread_takeover_records = 0;
        takeover_dcontext = NULL;
        for (i = 0; i < threads_to_signal; i++) {
            destroy_event(records[i].event);
        }
        HEAP_ARRAY_FREE(dcontext, records, takeover_record_t, threads_to_signal,
                        ACCT_THREAD_MGT, PROTECTED);
    }

    d_r_mutex_unlock(&thread_initexit_lock);
    HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, num_threads, ACCT_THREAD_MGT, PROTECTED);

    ASSERT(threads_to_signal >= threads_timed_out);
    return (threads_to_signal - threads_timed_out) > 0;
}

bool
os_thread_re_take_over(void)
{
#ifdef X86
    /* i#2089: is_thread_initialized() will fail for a currently-native app.
     * We bypass the magic field checks here of is_thread_tls_initialized().
     * XXX: should this be inside is_thread_initialized()?  But that may mislead
     * other callers: the caller has to restore the TLs.  Some old code also
     * used get_thread_private_dcontext() being NULL to indicate an unknown thread:
     * that should also call here.
     */
    if (!is_thread_initialized() && is_thread_tls_allocated()) {
        /* It's safe to call thread_lookup() for ourself. */
        thread_record_t *tr = thread_lookup(get_sys_thread_id());
        if (tr != NULL) {
            ASSERT(is_thread_currently_native(tr));
            LOG(GLOBAL, LOG_THREADS, 1, "\tretakeover for cur-native thread " TIDFMT "\n",
                get_sys_thread_id());
            LOG(tr->dcontext->logfile, LOG_THREADS, 1,
                "\nretakeover for cur-native thread " TIDFMT "\n", get_sys_thread_id());
            os_swap_dr_tls(tr->dcontext, false /*to dr*/);
            ASSERT(is_thread_initialized());
            return true;
        }
    }
#endif
    return false;
}

static void
os_thread_signal_taken_over(void)
{
    thread_id_t mytid;
    event_t event = NULL;
    uint i;
    /* Wake up the thread that initiated the take over. */
    mytid = d_r_get_thread_id();
    ASSERT(thread_takeover_records != NULL);
    for (i = 0; i < num_thread_takeover_records; i++) {
        if (thread_takeover_records[i].tid == mytid) {
            event = thread_takeover_records[i].event;
            break;
        }
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS, "mytid not present in takeover records!",
                   event != NULL);
    signal_event(event);
}

/* Takes over the current thread from the signal handler.  We notify the thread
 * that signaled us by signalling our event in thread_takeover_records.
 * If it returns, it returns false, and the thread should be let go.
 */
bool
os_thread_take_over(priv_mcontext_t *mc, kernel_sigset_t *sigset)
{
    dcontext_t *dcontext;
    priv_mcontext_t *dc_mc;

    LOG(GLOBAL, LOG_THREADS, 1, "TAKEOVER: received signal in thread " TIDFMT "\n",
        get_sys_thread_id());

    /* Do standard DR thread initialization.  Mirrors code in
     * create_clone_record and new_thread_setup, except we're not putting a
     * clone record on the dstack.
     */
    os_thread_re_take_over();
    if (!is_thread_initialized()) {
        /* If this is a thread on its way to init, don't self-interp (i#2688). */
        if (is_dynamo_address(mc->pc)) {
            os_thread_signal_taken_over();
            return false;
        }
        dcontext = init_thread_with_shared_siginfo(mc, takeover_dcontext);
        ASSERT(dcontext != NULL);
    } else {
        /* Re-takeover a thread that we let go native */
        dcontext = get_thread_private_dcontext();
        ASSERT(dcontext != NULL);
    }
    signal_set_mask(dcontext, sigset);
    signal_swap_mask(dcontext, true /*to app*/);
    dynamo_thread_under_dynamo(dcontext);
    dc_mc = get_mcontext(dcontext);
    *dc_mc = *mc;
    dcontext->whereami = DR_WHERE_APP;
    dcontext->next_tag = mc->pc;

    os_thread_signal_taken_over();

    DOLOG(2, LOG_TOP, {
        byte *cur_esp;
        GET_STACK_PTR(cur_esp);
        LOG(THREAD, LOG_TOP, 2,
            "%s: next_tag=" PFX ", cur xsp=" PFX ", mc->xsp=" PFX "\n", __FUNCTION__,
            dcontext->next_tag, cur_esp, mc->xsp);
    });
#ifdef LINUX
    /* See whether we should initiate lazy rseq handling, and avoid treating
     * regions as rseq when the rseq syscall is never set up.
     */
    if (rseq_is_registered_for_current_thread()) {
        rseq_locate_rseq_regions(false);
        rseq_thread_attach(dcontext);
    }
#endif

    /* Start interpreting from the signal context. */
    call_switch_stack(dcontext, dcontext->dstack, (void (*)(void *))d_r_dispatch,
                      NULL /*not on d_r_initstack*/, false /*shouldn't return*/);
    ASSERT_NOT_REACHED();
    return true; /* make compiler happy */
}

bool
os_thread_take_over_suspended_native(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    if (!is_thread_currently_native(dcontext->thread_record) ||
        ksynch_get_value(&ostd->suspended) < 0)
        return false;
    /* Thread is sitting in suspend signal loop so we just set a flag
     * for when it resumes:
     */
    /* XXX: there's no event for a client to trigger this on so not yet
     * tested.  i#721 may help.
     */
    ASSERT_NOT_TESTED();
    ostd->retakeover = true;
    return true;
}

/* Called for os-specific takeover of a secondary thread from the one
 * that called dr_app_setup().
 */
dcontext_t *
os_thread_take_over_secondary(priv_mcontext_t *mc)
{
    thread_record_t **list;
    int num_threads;
    int i;
    dcontext_t *dcontext;
    /* We want to share with the thread that called dr_app_setup. */
    d_r_mutex_lock(&thread_initexit_lock);
    get_list_of_threads(&list, &num_threads);
    ASSERT(num_threads >= 1);
    for (i = 0; i < num_threads; i++) {
        /* Find a thread that's already set up */
        if (is_thread_signal_info_initialized(list[i]->dcontext))
            break;
    }
    ASSERT(i < num_threads);
    ASSERT(list[i]->id != get_sys_thread_id());
    /* Assuming pthreads, prepare signal_field for sharing. */
    handle_clone(list[i]->dcontext, PTHREAD_CLONE_FLAGS);
    dcontext = init_thread_with_shared_siginfo(mc, list[i]->dcontext);
    d_r_mutex_unlock(&thread_initexit_lock);
    global_heap_free(list,
                     num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    return dcontext;
}

/***************************************************************************/

uint
os_random_seed(void)
{
    uint seed;
    /* reading from /dev/urandom for a non-blocking random */
    int urand = os_open("/dev/urandom", OS_OPEN_READ);
    DEBUG_DECLARE(int read =) os_read(urand, &seed, sizeof(seed));
    ASSERT(read == sizeof(seed));
    os_close(urand);

    return seed;
}

#ifdef RCT_IND_BRANCH
/* Analyze a range in a possibly new module
 * return false if not a code section in a module
 * otherwise returns true and adds all valid targets for rct_ind_branch_check
 */
bool
rct_analyze_module_at_violation(dcontext_t *dcontext, app_pc target_pc)
{
    /* FIXME: note that this will NOT find the data section corresponding to the given PC
     * we don't yet have a corresponding get_allocation_size or an ELF header walk routine
     * on linux
     */
    app_pc code_start;
    size_t code_size;
    uint prot;

    if (!get_memory_info(target_pc, &code_start, &code_size, &prot))
        return false;
    /* TODO: in almost all cases expect the region at module_base+module_size to be
     * the corresponding data section.
     * Writable yet initialized data indeed needs to be processed.
     */

    if (code_size > 0) {
        app_pc code_end = code_start + code_size;

        app_pc data_start;
        size_t data_size;

        ASSERT(TESTALL(MEMPROT_READ | MEMPROT_EXEC, prot)); /* code */

        if (!get_memory_info(code_end, &data_start, &data_size, &prot))
            return false;

        ASSERT(data_start == code_end);
        ASSERT(TESTALL(MEMPROT_READ | MEMPROT_WRITE, prot)); /* data */

        app_pc text_start = code_start;
        app_pc text_end = data_start + data_size;

        /* TODO: performance: do this only in case relocation info is not present */
        DEBUG_DECLARE(uint found =)
        find_address_references(dcontext, text_start, text_end, code_start, code_end);
        LOG(GLOBAL, LOG_RCT, 2, PFX "-" PFX " : %d ind targets of %d code size",
            text_start, text_end, found, code_size);
        return true;
    }
    return false;
}

#    ifdef X64
bool
rct_add_rip_rel_addr(dcontext_t *dcontext, app_pc tgt _IF_DEBUG(app_pc src))
{
    /* FIXME PR 276762: not implemented */
    return false;
}
#    endif
#endif /* RCT_IND_BRANCH */

#ifdef HOT_PATCHING_INTERFACE
void *
get_drmarker_hotp_policy_status_table()
{
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
set_drmarker_hotp_policy_status_table(void *new_table)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

byte *
hook_text(byte *hook_code_buf, const app_pc image_addr, intercept_function_t hook_func,
          const void *callee_arg, const after_intercept_action_t action_after,
          const bool abort_if_hooked, const bool ignore_cti, byte **app_code_copy_p,
          byte **alt_exit_tgt_p)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
}

void
unhook_text(byte *hook_code_buf, app_pc image_addr)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

void
insert_jmp_at_tramp_entry(dcontext_t *dcontext, byte *trampoline, byte *target)
{
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif /* HOT_PATCHING_INTERFACE */

bool
aslr_is_possible_attack(app_pc target)
{
    /* FIXME: ASLR not implemented */
    return false;
}

app_pc
aslr_possible_preferred_address(app_pc target_addr)
{
    /* FIXME: ASLR not implemented */
    return NULL;
}

void
take_over_primary_thread()
{
    /* nothing to do here */
}

bool
os_current_user_directory(char *directory_prefix /* INOUT */, uint directory_len,
                          bool create)
{
    /* XXX: could share some of this code w/ corresponding windows routine */
    uid_t uid = dynamorio_syscall(SYS_getuid, 0);
    char *directory = directory_prefix;
    char *dirend = directory_prefix + strlen(directory_prefix);
    snprintf(dirend, directory_len - (dirend - directory_prefix), "%cdpc-%d", DIRSEP,
             uid);
    directory_prefix[directory_len - 1] = '\0';
    if (!os_file_exists(directory, true /*is dir*/) && create) {
        /* XXX: we should ensure we do not follow symlinks */
        /* XXX: should add support for CREATE_DIR_FORCE_OWNER */
        if (!os_create_dir(directory, CREATE_DIR_REQUIRE_NEW)) {
            LOG(GLOBAL, LOG_CACHE, 2, "\terror creating per-user dir %s\n", directory);
            return false;
        } else {
            LOG(GLOBAL, LOG_CACHE, 2, "\tcreated per-user dir %s\n", directory);
        }
    }
    return true;
}

bool
os_validate_user_owned(file_t file_or_directory_handle)
{
    /* note on Linux this scheme should never be used */
    ASSERT(false && "chown Alice evilfile");
    return false;
}

bool
os_check_option_compatibility(void)
{
    /* no options are Linux OS version dependent */
    return false;
}

#ifdef X86_32
/* Emulate uint64 modulo and division by uint32 on ia32.
 * XXX: Does *not* handle 64-bit divisors!
 */
static uint64
uint64_divmod(uint64 dividend, uint64 divisor64, uint32 *remainder)
{
    /* Assumes little endian, which x86 is. */
    union {
        uint64 v64;
        struct {
            uint32 lo;
            uint32 hi;
        };
    } res;
    uint32 upper;
    uint32 divisor = (uint32)divisor64;

    /* Our uses don't use large divisors. */
    ASSERT(divisor64 <= UINT_MAX && "divisor is larger than uint32 can hold");

    /* Divide out the high bits first. */
    res.v64 = dividend;
    upper = res.hi;
    res.hi = upper / divisor;
    upper %= divisor;

    /* Use the unsigned div instruction, which uses EDX:EAX to form a 64-bit
     * dividend.  We only get a 32-bit quotient out, which is why we divide out
     * the high bits first.  The quotient will fit in EAX.
     *
     * DIV r/m32    F7 /6  Unsigned divide EDX:EAX by r/m32, with result stored
     *                     in EAX <- Quotient, EDX <- Remainder.
     * inputs:
     *   EAX = res.lo
     *   EDX = upper
     *   rm = divisor
     * outputs:
     *   res.lo = EAX
     *   *remainder = EDX
     * The outputs precede the inputs in gcc inline asm syntax, and so to put
     * inputs in EAX and EDX we use "0" and "1".
     */
    asm("divl %2"
        : "=a"(res.lo), "=d"(*remainder)
        : "rm"(divisor), "0"(res.lo), "1"(upper));
    return res.v64;
}

/* Match libgcc's prototype. */
uint64
__udivdi3(uint64 dividend, uint64 divisor)
{
    uint32 remainder;
    return uint64_divmod(dividend, divisor, &remainder);
}

/* Match libgcc's prototype. */
uint64
__umoddi3(uint64 dividend, uint64 divisor)
{
    uint32 remainder;
    uint64_divmod(dividend, divisor, &remainder);
    return (uint64)remainder;
}

/* Same thing for signed. */
static int64
int64_divmod(int64 dividend, int64 divisor64, int *remainder)
{
    union {
        int64 v64;
        struct {
            int lo;
            int hi;
        };
    } res;
    int upper;
    int divisor = (int)divisor64;

    /* Our uses don't use large divisors. */
    ASSERT(divisor64 <= INT_MAX && divisor64 >= INT_MIN && "divisor too large for int");

    /* Divide out the high bits first. */
    res.v64 = dividend;
    upper = res.hi;
    res.hi = upper / divisor;
    upper %= divisor;

    /* Like above but with the signed div instruction, which does a signed divide
     * on edx:eax by r/m32 => quotient in eax, remainder in edx.
     */
    asm("idivl %2"
        : "=a"(res.lo), "=d"(*remainder)
        : "rm"(divisor), "0"(res.lo), "1"(upper));
    return res.v64;
}

/* Match libgcc's prototype. */
int64
__divdi3(int64 dividend, int64 divisor)
{
    int remainder;
    return int64_divmod(dividend, divisor, &remainder);
}

/* __moddi3 is coming from third_party/libgcc for x86 as well as arm. */

#elif defined(ARM)
/* i#1566: for ARM, __aeabi versions are used instead of udivdi3 and umoddi3.
 * We link with __aeabi routines from libgcc via third_party/libgcc.
 */
#endif /* X86_32 */

/****************************************************************************
 * Tests
 */

#if defined(STANDALONE_UNIT_TEST)

void
test_uint64_divmod(void)
{
#    ifdef X86_32
    uint64 quotient;
    uint32 remainder;

    /* Simple division below 2^32. */
    quotient = uint64_divmod(9, 3, &remainder);
    EXPECT(quotient == 3, true);
    EXPECT(remainder == 0, true);
    quotient = uint64_divmod(10, 3, &remainder);
    EXPECT(quotient == 3, true);
    EXPECT(remainder == 1, true);

    /* Division when upper bits are less than the divisor. */
    quotient = uint64_divmod(45ULL << 31, 1U << 31, &remainder);
    EXPECT(quotient == 45, true);
    EXPECT(remainder == 0, true);

    /* Division when upper bits are greater than the divisor. */
    quotient = uint64_divmod(45ULL << 32, 15, &remainder);
    EXPECT(quotient == 3ULL << 32, true);
    EXPECT(remainder == 0, true);
    quotient = uint64_divmod((45ULL << 32) + 13, 15, &remainder);
    EXPECT(quotient == 3ULL << 32, true);
    EXPECT(remainder == 13, true);

    /* Try calling the intrinsics.  Don't divide by powers of two, gcc will
     * lower that to a shift.
     */
    quotient = (45ULL << 32);
    quotient /= 15;
    EXPECT(quotient == (3ULL << 32), true);
    quotient = (45ULL << 32) + 13;
    remainder = quotient % 15;
    EXPECT(remainder == 13, true);
#    endif /* X86_32 */
}

void
unit_test_os(void)
{
    test_uint64_divmod();
}

#endif /* STANDALONE_UNIT_TEST */
