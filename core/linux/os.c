/* *******************************************************************************
 * Copyright (c) 2010-2013 Google, Inc.  All rights reserved.
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
#define _LARGEFILE64_SOURCE
/* for mmap-related #defines */
#include <sys/types.h>
#include <sys/mman.h>
/* for open */
#include <sys/stat.h>
#include <fcntl.h>
#include "../globals.h"
#include "../hashtable.h"
#include <string.h>
#include <unistd.h> /* for write and usleep and _exit */
#include <limits.h>
#include <sys/sysinfo.h>        /* for get_nprocs_conf */
#include <sys/vfs.h> /* for statfs */
#include <dirent.h>

/* for getrlimit */
#include <sys/time.h>
#include <sys/resource.h>

#include <linux/futex.h> /* for futex op code */

/* For clone and its flags, the manpage says to include sched.h with _GNU_SOURCE
 * defined.  _GNU_SOURCE brings in unwanted extensions and causes name
 * conflicts.  Instead, we include linux/sched.h which comes from the Linux
 * kernel headers.
 */
#include <linux/sched.h>

#include "module.h" /* elf */

#ifndef F_DUPFD_CLOEXEC /* in linux 2.6.24+ */
# define F_DUPFD_CLOEXEC 1030
#endif

#ifndef SYS_dup3
# ifdef X64
#  define SYS_dup3 292
# else
#  define SYS_dup3 330
# endif
#endif /* SYS_dup3 */

/* Cross arch syscall nums for use with struct stat64. */
#ifdef X64
# define SYSNUM_STAT SYS_stat
# define SYSNUM_FSTAT SYS_fstat
#else
# define SYSNUM_STAT SYS_stat64
# define SYSNUM_FSTAT SYS_fstat64
#endif

/* Prototype for all functions in .init_array. */
typedef int (*init_fn_t)(int argc, char **argv, char **envp);

/* i#46: Private __environ pointer.  Points at the environment variable array
 * on the stack, which is different from what libc __environ may point at.  We
 * use the environment for following children and setting options, so its OK
 * that we don't see what libc says.
 */
char **our_environ;
#include <errno.h>
/* avoid problems with use of errno as var name in rest of file */
#undef errno
/* we define __set_errno below */

#ifdef X86
/* must be prior to <link.h> => <elf.h> => INT*_{MIN,MAX} */
# include "instr.h" /* for get_app_segment_base() */
#endif

#include "decode_fast.h" /* decode_cti: maybe os_handle_mov_seg should be ifdef X86? */

#ifndef HAVE_PROC_MAPS
/* must be prior to including dlfcn.h */
# define _GNU_SOURCE 1
/* despite man page I had to define this in addition to prev line */
# define __USE_GNU 1
/* this relies on not having -Icore/ to avoid including core/link.h
 * (pre-cmake we used -iquote instead of -I when we did have -Icore/)
 */
# include <link.h> /* struct dl_phdr_info */
#endif

#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>             /* vsyslog */
#include "../vmareas.h"
#ifdef RCT_IND_BRANCH
#  include "../rct.h"
#endif
#include "include/syscall.h"            /* our own local copy */
#include "../module_shared.h"
#include "os_private.h"
#include "../synch.h"

#ifdef CLIENT_INTERFACE
# include "instrument.h"
#endif

#ifdef NOT_DYNAMORIO_CORE_PROPER
# undef ASSERT
# undef ASSERT_NOT_IMPLEMENTED
# undef ASSERT_NOT_TESTED
# undef ASSERT_CURIOSITY
# define ASSERT(x) /* nothing */
# define ASSERT_NOT_IMPLEMENTED(x) /* nothing */
# define ASSERT_NOT_TESTED(x) /* nothing */
# define ASSERT_CURIOSITY(x) /* nothing */
# undef LOG
# undef DOSTATS
# define LOG(...) /* nothing */
# define DOSTATS(...) /* nothing */
#else /* !NOT_DYNAMORIO_CORE_PROPER: around most of file, to exclude preload */

#include <asm/ldt.h>
/* The ldt struct in ldt.h used to be just "struct modify_ldt_ldt_s"; then that
 * was also typdef-ed as modify_ldt_t; then it was just user_desc.
 * To compile on old and new we inline our own copy of the struct:
 */
typedef struct _our_modify_ldt_t {
    unsigned int  entry_number;
    unsigned int  base_addr;
    unsigned int  limit;
    unsigned int  seg_32bit:1;
    unsigned int  contents:2;
    unsigned int  read_exec_only:1;
    unsigned int  limit_in_pages:1;
    unsigned int  seg_not_present:1;
    unsigned int  useable:1;
} our_modify_ldt_t;

#ifdef X64
/* Linux GDT layout in x86_64: 
 * #define GDT_ENTRY_TLS_MIN 12
 * #define GDT_ENTRY_TLS_MAX 14
 * #define GDT_ENTRY_TLS 1
 * TLS indexes for 64-bit, hardcode in arch_prctl
 * #define FS_TLS 0
 * #define GS_TLS 1
 * #define GS_TLS_SEL ((GDT_ENTRY_TLS_MIN+GS_TLS)*8 + 3)
 * #define FS_TLS_SEL ((GDT_ENTRY_TLS_MIN+FS_TLS)*8 + 3)
 */
# define FS_TLS 0 /* used in arch_prctl handling */
# define GS_TLS 1 /* used in arch_prctl handling */
#else
/* Linux GDT layout in x86_32
 * 6 - TLS segment #1 0x33 [ glibc's TLS segment ]
 * 7 - TLS segment #2 0x3b [ Wine's %fs Win32 segment ]
 * 8 - TLS segment #3 0x43 
 * FS and GS is not hardcode.
 */
#endif 
#define GDT_NUM_TLS_SLOTS 3
#define GDT_ENTRY_TLS_MIN_32 6
#define GDT_ENTRY_TLS_MIN_64 12
/* when x86-64 emulate i386, it still use 12-14, so using ifdef x64
 * cannot detect the right value.
 * The actual value will be updated later in os_tls_app_seg_init.
 */
static uint gdt_entry_tls_min = IF_X64_ELSE(GDT_ENTRY_TLS_MIN_64,
                                            GDT_ENTRY_TLS_MIN_32);

/* Indicates that on the next request for a GDT entry, we should return the GDT
 * entry we stole for private library TLS.  The entry index is in
 * lib_tls_gdt_index.
 * FIXME i#107: For total segment transparency, we can use the same approach
 * with tls_gdt_index.
 */
static bool return_stolen_lib_tls_gdt;
/* Guards data written by os_set_app_thread_area(). */
DECLARE_CXTSWPROT_VAR(static mutex_t set_thread_area_lock,
                      INIT_LOCK_FREE(set_thread_area_lock));

#ifndef HAVE_TLS
/* We use a table lookup to find a thread's dcontext */
/* Our only current no-TLS target, VMKernel (VMX86_SERVER), doesn't have apps with
 * tons of threads anyway
 */
#define MAX_THREADS 512
typedef struct _tls_slot_t {
    thread_id_t tid;
    dcontext_t *dcontext;
} tls_slot_t;
/* Stored in heap for self-prot */
static tls_slot_t *tls_table;
/* not static so deadlock_avoidance_unlock() can look for it */
DECLARE_CXTSWPROT_VAR(mutex_t tls_lock, INIT_LOCK_FREE(tls_lock));
#endif

#define MAX_NUM_CLIENT_TLS 64
#ifdef CLIENT_INTERFACE
/* Should we place this in a client header?  Currently mentioned in
 * dr_raw_tls_calloc() docs.
 */
static bool client_tls_allocated[MAX_NUM_CLIENT_TLS];
DECLARE_CXTSWPROT_VAR(static mutex_t client_tls_lock, INIT_LOCK_FREE(client_tls_lock));
#endif

#include <stddef.h> /* for offsetof */

#include <sys/utsname.h> /* for struct utsname */

/* forward decl */
static void handle_execve_post(dcontext_t *dcontext);
static bool os_switch_lib_tls(dcontext_t *dcontext, bool to_app);
static bool os_switch_seg_to_context(dcontext_t *dcontext, reg_id_t seg, bool to_app);

/* full path to our own library, used for execve */
static char dynamorio_library_path[MAXIMUM_PATH];
/* Issue 20: path to other architecture */
static char dynamorio_alt_arch_path[MAXIMUM_PATH];
/* Makefile passes us LIBDIR_X{86,64} defines */
#define DR_LIBDIR_X86 STRINGIFY(LIBDIR_X86)
#define DR_LIBDIR_X64 STRINGIFY(LIBDIR_X64)

/* pc values delimiting dynamo dll image */
static app_pc dynamo_dll_start = NULL;
static app_pc dynamo_dll_end = NULL; /* open-ended */

static app_pc executable_start = NULL;

/* Used by get_application_name(). */
static char executable_path[MAXIMUM_PATH];
static char *executable_basename;

/* does the kernel provide tids that must be used to distinguish threads in a group? */
static bool kernel_thread_groups;

/* Does the kernel support SYS_futex syscall? Safe to initialize assuming 
 * no futex support.
 */
bool kernel_futex_support = false;

static bool kernel_64bit;

pid_t pid_cached;

static bool fault_handling_initialized;

#ifdef PROFILE_RDTSC
uint kilo_hertz; /* cpu clock speed */
#endif

/* lock to guard reads from /proc/self/maps in get_memory_info_from_os(). */
static mutex_t memory_info_buf_lock = INIT_LOCK_FREE(memory_info_buf_lock);
/* lock for iterator where user needs to allocate memory */
static mutex_t maps_iter_buf_lock = INIT_LOCK_FREE(maps_iter_buf_lock);

/* Xref PR 258731, dup of STDOUT/STDERR in case app wants to close them. */
DR_API file_t our_stdout = STDOUT_FILENO;
DR_API file_t our_stderr = STDERR_FILENO;
DR_API file_t our_stdin = STDIN_FILENO;

/* we steal fds from the app */
static struct rlimit app_rlimit_nofile;

/* we store all DR files so we can prevent the app from changing them,
 * and so we can close them in a child of fork.
 * the table key is the fd and the payload is the set of DR_FILE_* flags.
 */
static generic_table_t *fd_table;
#define INIT_HTABLE_SIZE_FD 6 /* should remain small */
static void fd_table_add(file_t fd, uint flags);

/* Track all memory regions seen by DR. We track these ourselves to prevent
 * repeated reads of /proc/self/maps (case 3771). An allmem_info_t struct is
 * stored in the custom field.
 * all_memory_areas is updated along with dynamo_areas, due to cyclic
 * dependencies.
 */
/* exported for debug to avoid rank order in print_vm_area() */
IF_DEBUG_ELSE(,static) vm_area_vector_t *all_memory_areas;

typedef struct _allmem_info_t {
    uint prot;
    dr_mem_type_t type;
    bool shareable;
} allmem_info_t;

static void allmem_info_free(void *data);
static void *allmem_info_dup(void *data);
static bool allmem_should_merge(bool adjacent, void *data1, void *data2);
static void *allmem_info_merge(void *dst_data, void *src_data);

/* HACK to make all_memory_areas->lock recursive 
 * protected for both read and write by all_memory_areas->lock
 * FIXME: provide general rwlock w/ write portion recursive
 * FIXME: eliminate duplicate code (see dynamo_areas_recursion)
 */
DECLARE_CXTSWPROT_VAR(uint all_memory_areas_recursion, 0);

static bool
is_readable_without_exception_internal(const byte *pc, size_t size, bool query_os);

static void
process_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
             uint flags _IF_DEBUG(const char *map_type));

static char *
read_proc_self_exe(bool ignore_cache);

/* Iterator over /proc/self/maps
 * Called at arbitrary places, so cannot use fopen.
 * The whole thing is serialized, so entries cannot be referenced
 * once the iteration ends.
 */
typedef struct _maps_iter_t {
    app_pc vm_start;
    app_pc vm_end;
    uint prot;
    size_t offset; /* offset into the file being mapped */
    uint64 inode; /* FIXME - use ino_t?  We need to know what size code to use for the
                   * scanf and I don't trust that we, the maps file, and any clients
                   * will all agree on its size since it seems to be defined differently
                   * depending on whether large file support is compiled in etc.  Just
                   * using uint64 might be safer (see also inode in module_names_t). */
    const char *comment; /* usually file name with path, but can also be [vsdo] for the
                          * vsyscall page */
    /* for internal use by the iterator.  we could make these static,
     * since we only support one concurrent iterator, but then we have
     * .data protection complexities.
     */
    bool may_alloc;
    file_t maps;
    char *newline;
    int bufread;
    int bufwant;
    char *buf;
    char *comment_buffer;
} maps_iter_t;

static bool maps_iterator_start(maps_iter_t *iter, bool may_alloc);
static void maps_iterator_stop(maps_iter_t *iter);
static bool maps_iterator_next(maps_iter_t *iter);

/* Libc independent directory iterator, similar to readdir.  If we ever need
 * this on Windows we should generalize it and export it to clients.
 */
typedef struct _dir_iterator_t {
    file_t fd;
    int off;
    int end;
    const char *name;            /* Name of the current entry. */
    char buf[4 * MAXIMUM_PATH];  /* Expect stack alloc, so not too big. */
} dir_iterator_t;

static void os_dir_iterator_start(dir_iterator_t *iter, file_t fd);
static bool os_dir_iterator_next(dir_iterator_t *iter);
/* XXX: If we generalize to Windows, will we need os_dir_iterator_stop()? */

static int
get_library_bounds(const char *name, app_pc *start/*IN/OUT*/, app_pc *end/*OUT*/,
                   char *fullpath/*OPTIONAL OUT*/, size_t path_size);

/* vsyscall page.  hardcoded at 0xffffe000 in earlier kernels, but
 * randomly placed since fedora2.
 * marked rx then: FIXME: should disallow this guy when that's the case!
 * random vsyscall page is identified in maps files as "[vdso]"
 * (kernel-provided fake shared library or Virt Dyn Shared Object)
 */
app_pc vsyscall_page_start = NULL;
/* pc of the end of the syscall instr itself */
app_pc vsyscall_syscall_end_pc = NULL;
/* pc where kernel returns control after sysenter vsyscall */
app_pc vsyscall_sysenter_return_pc = NULL;
#define VSYSCALL_PAGE_START_HARDCODED ((app_pc)(ptr_uint_t) 0xffffe000)
#ifdef X64
/* i#430, in Red Hat Enterprise Server 5.6, vysycall region is marked 
 * not executable
 * ffffffffff600000-ffffffffffe00000 ---p 00000000 00:00 0  [vsyscall]
 */
# define VSYSCALL_REGION_MAPS_NAME "[vsyscall]"
#endif

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
__errno_location(void) {
    /* Each dynamo thread should have a separate errno */
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return &init_errno;
    else {
        /* WARNING: init_errno is in data segment so can be RO! */
        return &(dcontext->upcontext_ptr->errno);
    }
}
#endif /* !STANDALONE_UNIT_TEST && !STATIC_LIBRARY */

#if defined(HAVE_TLS) && defined(CLIENT_INTERFACE)
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
static int libc_errno_tls_offs;
static int *
our_libc_errno_loc(void)
{
    void *app_tls = os_get_app_seg_base(NULL, LIB_SEG_TLS);
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
                libc_errno_loc = (errno_loc_t)
                    get_proc_address(area->start, "__errno_location");
                ASSERT(libc_errno_loc != NULL);
                LOG(GLOBAL, LOG_THREADS, 2, "libc errno loc func: "PFX"\n",
                    libc_errno_loc);
#ifdef CLIENT_INTERFACE
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
#endif
                if (found)
                    break;
            }
        }
        module_iterator_stop(mi);
#if defined(HAVE_TLS) && defined(CLIENT_INTERFACE)
        /* i#598: init the libc errno's offset.  If we didn't find libc above,
         * then we don't need to do this.
         */
        if (INTERNAL_OPTION(private_loader) && libc_errno_loc != NULL) {
            void *dr_lib_tls_base = os_get_dr_seg_base(NULL, LIB_SEG_TLS);
            ASSERT(dr_lib_tls_base != NULL);
            libc_errno_tls_offs = (void *)libc_errno_loc() - dr_lib_tls_base;
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
#ifdef STANDALONE_UNIT_TEST
    errno_loc_t func = __errno_location;
#else
    errno_loc_t func = get_libc_errno_location(false);
#endif
    if (func == NULL) {
        /* libc hasn't been loaded yet or we're doing early injection. */
        return 0;
    } else {
        int *loc = (*func)();
        ASSERT(loc != NULL);
        LOG(THREAD_GET, LOG_THREADS, 5, "libc errno loc: "PFX"\n", loc);
        if (loc != NULL)
            return *loc;
    }
    return 0;
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
    size_t len;
    char **ep;
    
    if (name == NULL || *name == '\0' || strchr (name, '=') != NULL) {
        return -1;
    }
    ASSERT(our_environ != NULL);
    if (our_environ == NULL)
        return -1;
    
    len = strlen (name);
    
    /* FIXME: glibc code grabs a lock here, we don't have access to that lock
     * LOCK;
     */
    
    ep = our_environ;
    while (*ep != NULL)
        if (!strncmp (*ep, name, len) && (*ep)[len] == '=') {
            /* Found it.  Remove this pointer by moving later ones back.  */
            char **dp = ep;
            
            do {
                dp[0] = dp[1];
            } while (*dp++);
            /* Continue the loop in case NAME appears again.  */
        } else
            ++ep;

    /* FIXME: glibc code unlocks here, we don't have access to that lock
     * UNLOCK;
     */
    
    return 0;
}

/* i#46: Private getenv.
 */
char *
getenv(const char *name)
{
    char **ep = our_environ;
    size_t i;
    size_t name_len;
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        return NULL;
    }
    ASSERT_MESSAGE(CHKLVL_ASSERTS, "our_environ is missing.  _init() or "
                   "dynamorio_set_envp() were not called", our_environ != NULL);
    if (our_environ == NULL)
        return NULL;
    name_len = strlen(name);
    for (i = 0; ep[i] != NULL; i++) {
        if (strncmp(ep[i], name, name_len) == 0 && ep[i][name_len] == '=') {
            return ep[i] + name_len + 1;
        }
    }
    return NULL;
}

/* Work around drpreload's _init going first.  We can get envp in our own _init
 * routine down below, but drpreload.so comes first and calls
 * dynamorio_app_init before our own _init routine gets called.  Apps using the
 * app API are unaffected because our _init routine will have run by then.  For
 * STATIC_LIBRARY, we simply set our_environ below in our_init().
 */
DYNAMORIO_EXPORT
void
dynamorio_set_envp(char **envp)
{
    our_environ = envp;
}

/* shared library init */
int
our_init(int argc, char **argv, char **envp)
{
    /* if do not want to use drpreload.so, we can take over here */
    extern void dynamorio_app_take_over(void);
    bool takeover = false;
#ifdef INIT_TAKE_OVER
    takeover = true;
#endif
#ifdef VMX86_SERVER
    /* PR 391765: take over here instead of using preload */
    takeover = os_in_vmkernel_classic();
#endif
    if (our_environ != NULL) {
        /* Set by dynamorio_set_envp above.  These should agree. */
        ASSERT(our_environ == envp);
    } else {
        our_environ = envp;
    }
    if (!takeover) {
        const char *takeover_env = getenv("DYNAMORIO_TAKEOVER_IN_INIT");
        if (takeover_env != NULL && strcmp(takeover_env, "1") == 0) {
            takeover = true;
        }
    }
    if (takeover) {
        if (dynamorio_app_init() == 0 /* success */) {
            dynamorio_app_take_over();
        }
    }
    return 0;
}

#if defined(STATIC_LIBRARY) || defined(STANDALONE_UNIT_TEST)
/* If we're getting linked into a binary that already has an _init definition
 * like the app's exe or unit_tests, we add a pointer to our_init() to the
 * .init_array section.  We can't use the constructor attribute because not all
 * toolchains pass the args and environment to the constructor.
 */
static init_fn_t
__attribute__ ((section (".init_array"), aligned (sizeof (void *)), used))
init_array[] = {
    our_init
};
#else
/* If we're a normal shared object, then we override _init.
 */
int
_init(int argc, char **argv, char **envp)
{
    return our_init(argc, argv, envp);
}
#endif

bool
kernel_is_64bit(void)
{
    return kernel_64bit;
}

static void
get_uname(void)
{
    /* assumption: only called at init, so we don't need any synch
     * or .data unprot
     */
    static struct utsname uinfo; /* can be large, avoid stack overflow */
    DEBUG_DECLARE(int res =)
        dynamorio_syscall(SYS_uname, 1, (ptr_uint_t)&uinfo);
    ASSERT(res >= 0);
    LOG(GLOBAL, LOG_TOP, 1, "uname:\n\tsysname: %s\n", uinfo.sysname);
    LOG(GLOBAL, LOG_TOP, 1, "\tnodename: %s\n", uinfo.nodename);
    LOG(GLOBAL, LOG_TOP, 1, "\trelease: %s\n", uinfo.release);
    LOG(GLOBAL, LOG_TOP, 1, "\tversion: %s\n", uinfo.version);
    LOG(GLOBAL, LOG_TOP, 1, "\tmachine: %s\n", uinfo.machine);
    if (strncmp(uinfo.machine, "x86_64", sizeof("x86_64")) == 0)
        kernel_64bit = true;
}

/* os-specific initializations */
void
os_init(void)
{
    /* Determines whether the kernel supports SYS_futex syscall or not.
     * From man futex(2): initial futex support was merged in 2.5.7, in current six
     * argument format since 2.6.7.
     */
    volatile int futex_for_test = 0;
    ptr_int_t res = dynamorio_syscall(SYS_futex, 6, &futex_for_test, FUTEX_WAKE, 1,
                                      NULL, NULL, 0);
    kernel_futex_support = (res >= 0); 
    ASSERT_CURIOSITY(kernel_futex_support);

    get_uname();

    /* Populate global data caches. */
    get_application_name();

    /* determine whether gettid is provided and needed for threads,
     * or whether getpid suffices.  even 2.4 kernels have gettid
     * (maps to getpid), don't have an old enough target to test this.
     */
    kernel_thread_groups = (dynamorio_syscall(SYS_gettid, 0) >= 0);
    LOG(GLOBAL, LOG_TOP|LOG_STATS, 1, "thread id is from %s\n",
        kernel_thread_groups ? "gettid" : "getpid");
    ASSERT_CURIOSITY(kernel_thread_groups);

    pid_cached = get_process_id();

#ifdef VMX86_SERVER
    vmk_init();
#endif
    
    signal_init();
    /* We now set up an early fault handler for safe_read() (i#350) */
    fault_handling_initialized = true;

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        ASSERT_NOT_TESTED();
        kilo_hertz = get_timer_frequency();
        LOG(GLOBAL, LOG_TOP|LOG_STATS, 1, "CPU MHz is %d\n", kilo_hertz/1000);
    }
#endif /* PROFILE_RDTSC */
    /* Need to be after heap_init */
    VMVECTOR_ALLOC_VECTOR(all_memory_areas, GLOBAL_DCONTEXT,
                          VECTOR_SHARED, all_memory_areas);
    vmvector_set_callbacks(all_memory_areas, allmem_info_free, allmem_info_dup,
                           allmem_should_merge, allmem_info_merge);

    /* we didn't have heap in os_file_init() so create and add global logfile now */
    fd_table = generic_hash_create(GLOBAL_DCONTEXT, INIT_HTABLE_SIZE_FD,
                                   80 /* load factor: not perf-critical */,
                                   HASHTABLE_SHARED | HASHTABLE_PERSISTENT,
                                   NULL _IF_DEBUG("fd table"));
#ifdef DEBUG
    if (GLOBAL != INVALID_FILE)
        fd_table_add(GLOBAL, OS_OPEN_CLOSE_ON_FORK);
#endif

    /* Ensure initialization */
    get_dynamorio_dll_start();
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
     * after all we probably don't need all -steal_fds, and if we realy need fds
     * we typically open them at startup.  We also don't bother watching all
     * syscalls that take in fds from affecting our fds.
     */
    if (DYNAMO_OPTION(steal_fds) > 0) {
        struct rlimit rlimit_nofile;
        if (dynamorio_syscall(SYS_getrlimit, 2, RLIMIT_NOFILE, &rlimit_nofile) != 0) {
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
            app_rlimit_nofile.rlim_max = rlimit_nofile.rlim_max - DYNAMO_OPTION(steal_fds);
            app_rlimit_nofile.rlim_cur = app_rlimit_nofile.rlim_max;

            rlimit_nofile.rlim_cur = rlimit_nofile.rlim_max;
            if (dynamorio_syscall(SYS_setrlimit, 2, RLIMIT_NOFILE, &rlimit_nofile) != 0)
                SYSLOG_INTERNAL_WARNING("unable to raise RLIMIT_NOFILE soft limit");
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
        snprintf(pidstr, sizeof(pidstr)-1, "%d", pid);
    }
    return pidstr;
}

/* get application pid, (cached), used for event logging */
char*
get_application_pid()
{
    return get_application_pid_helper(false);
}

/* i#907: Called during early injection before data section protection to avoid
 * issues with /proc/self/exe.
 */
void
set_executable_path(const char *exe_path)
{
    strncpy(executable_path, exe_path, BUFFER_SIZE_ELEMENTS(executable_path));
    NULL_TERMINATE_BUFFER(executable_path);
}

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
            /* Populate cache from /proc/self/exe link. */
            strncpy(executable_path, read_proc_self_exe(ignore_cache),
                    BUFFER_SIZE_ELEMENTS(executable_path));
            NULL_TERMINATE_BUFFER(executable_path);
            /* FIXME: Fall back on /proc/self/cmdline and maybe argv[0] from
             * _init().
             */
            ASSERT(strlen(executable_path) > 0 &&
                   "readlink /proc/self/exe failed");
        }
    }

    /* Get basename. */
    if (executable_basename == NULL || ignore_cache) {
        executable_basename = strrchr(executable_path, '/');
        executable_basename = (executable_basename == NULL ?
                               executable_path : executable_basename + 1);
    }
    return (full_path ? executable_path : executable_basename);
}

/* get application name, (cached), used for event logging */
char *
get_application_name(void)
{
    return get_application_name_helper(false, true /* full path */);
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

/* Processor information provided by kernel */
#define PROC_CPUINFO "/proc/cpuinfo"
#define CPUMHZ_LINE_LENGTH  64
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
        return 1000 * 1000;  /* 1 GHz */

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
            LOG(GLOBAL, LOG_ALL, 2, "Processor speed exactly %lu.%03luMHz\n",
                cpu_mhz, cpu_khz);
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
    return (uint) dynamorio_syscall(SYS_time, 1, NULL) + UTC_TO_EPOCH_SECONDS;
}

/* milliseconds since 1601 */
uint64
query_time_millis()
{
    struct timeval current_time;
    if (dynamorio_syscall(SYS_gettimeofday, 2, &current_time, NULL) == 0) {
        uint64 res = (((uint64)current_time.tv_sec) * 1000) +
            (current_time.tv_usec / 1000);
        res += UTC_TO_EPOCH_SECONDS * 1000;
        return res;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
}

#ifdef RETURN_AFTER_CALL
/* Finds the bottom of the call stack, presumably at program startup. */
/* This routine is a copycat of internal_dump_callstack and makes assumptions about program state, 
   i.e. that frame pointers are valid and should be used only in well known points for release build.
*/
static app_pc
find_stack_bottom()
{
    app_pc retaddr = 0;
    int depth = 0;
    reg_t *fp;
    /* from dump_dr_callstack() */
    asm("mov  %%"ASM_XBP", %0" : "=m"(fp));

    LOG(THREAD_GET, LOG_ALL, 3, "Find stack bottom:\n");
    while (fp != NULL && is_readable_without_exception((byte *)fp, sizeof(reg_t)*2)) {
        retaddr = (app_pc)*(fp+1);      /* presumably also readable */
        LOG(THREAD_GET, LOG_ALL, 3,
            "\tframe ptr "PFX" => parent "PFX", ret = "PFX"\n", fp, *fp, retaddr);
        depth++;
        /* yes I've seen weird recursive cases before */
        if (fp == (reg_t *) *fp || depth > 100)
            break;
        fp = (reg_t *) *fp;
    }
    return retaddr;
}
#endif /* RETURN_AFTER_CALL */

/* os-specific atexit cleanup */
void
os_slow_exit(void)
{
    signal_exit();

    generic_hash_destroy(GLOBAL_DCONTEXT, fd_table);
    fd_table = NULL;

    DELETE_LOCK(memory_info_buf_lock);
    DELETE_LOCK(maps_iter_buf_lock);
    DELETE_LOCK(set_thread_area_lock);
#ifdef CLIENT_INTERFACE
    DELETE_LOCK(client_tls_lock);
#endif
    vmvector_delete_vector(GLOBAL_DCONTEXT, all_memory_areas);
    all_memory_areas = NULL;
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
    /* XXX: TERMINATE_THREAD not supported */
    ASSERT_NOT_IMPLEMENTED(TEST(TERMINATE_PROCESS, flags));
    if (TEST(TERMINATE_CLEANUP, flags)) {
        /* we enter from several different places, so rewind until top-level kstat */
        KSTOP_REWIND_UNTIL(thread_measured);
        cleanup_and_terminate(dcontext, SYS_exit_group, exit_code, 0,
                              true/*whole process*/);
    } else {
        /* clean up may be impossible - just terminate */
        config_exit(); /* delete .1config file */
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

#if defined(X64) && !defined(ARCH_SET_GS)
# define ARCH_SET_GS 0x1001
# define ARCH_SET_FS 0x1002
# define ARCH_GET_FS 0x1003
# define ARCH_GET_GS 0x1004
#endif

/* We support 3 different methods of creating a segment (see os_tls_init()) */
typedef enum {
    TLS_TYPE_NONE,
    TLS_TYPE_LDT,
    TLS_TYPE_GDT,
#ifdef X64
    TLS_TYPE_ARCH_PRCTL,
#endif
} tls_type_t;

static tls_type_t tls_type;
#ifdef X64
static bool tls_using_msr;
#endif

#ifdef HAVE_TLS
/* GDT slot we use for set_thread_area.
 * This depends on the kernel, not on the app!
 */
static int tls_gdt_index = -1;
/* GDT slot we use for private library TLS. */
static int lib_tls_gdt_index = -1;
# define GDT_NO_SIZE_LIMIT  0xfffff
# ifdef DEBUG
#  define GDT_32BIT  8 /*  6=NPTL, 7=wine */
#  define GDT_64BIT 14 /* 12=NPTL, 13=wine */
# endif

static int
modify_ldt_syscall(int func, void *ptr, unsigned long bytecount)
{
    return dynamorio_syscall(SYS_modify_ldt, 3, func, ptr, bytecount);
}

/* reading ldt entries gives us the raw format, not struct modify_ldt_ldt_s */
typedef struct {
    unsigned int limit1500:16;
    unsigned int base1500:16;
    unsigned int base2316:8;
    unsigned int type:4;
    unsigned int not_system:1;
    unsigned int privilege_level:2;
    unsigned int seg_present:1;
    unsigned int limit1916:4;
    unsigned int custom:1;
    unsigned int zero:1;
    unsigned int seg_32bit:1;
    unsigned int limit_in_pages:1;
    unsigned int base3124:8;
} raw_ldt_entry_t;

enum {
    LDT_TYPE_CODE      = 0x8,
    LDT_TYPE_DOWN      = 0x4,
    LDT_TYPE_WRITE     = 0x2,
    LDT_TYPE_ACCESSED  = 0x1,
};

#define LDT_BASE(ldt) (((ldt)->base3124<<24) | ((ldt)->base2316<<16) | (ldt)->base1500)

#ifdef DEBUG
# if 0 /* not used */
#  ifdef X64
    /* Intel docs confusingly say that app descriptors are 8 bytes while
     * system descriptors are 16; more likely all are 16, as linux kernel
     * docs seem to assume. */
#   error NYI
#  endif
static void
print_raw_ldt(raw_ldt_entry_t *ldt)
{
    LOG(GLOBAL, LOG_ALL, 1,
        "ldt @"PFX":\n", ldt);
    LOG(GLOBAL, LOG_ALL, 1,
        "\traw: "PFX" "PFX"\n", *((unsigned int *)ldt), *(1+(unsigned int *)ldt));
    LOG(GLOBAL, LOG_ALL, 1,
        "\tbase: 0x%x\n", LDT_BASE(ldt));
    LOG(GLOBAL, LOG_ALL, 1,
        "\tlimit: 0x%x\n", (ldt->limit1916<<16) | ldt->limit1500);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tlimit_in_pages: 0x%x\n", ldt->limit_in_pages);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tseg_32bit: 0x%x\n", ldt->seg_32bit);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tseg_present: 0x%x\n", ldt->seg_present);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tprivilege_level: 0x%x\n", ldt->privilege_level);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tnot_system: 0x%x\n", ldt->not_system);
    LOG(GLOBAL, LOG_ALL, 1,
        "\ttype: 0x%x == %s%s%s%s\n", ldt->type,
           (ldt->type & LDT_TYPE_CODE)  ? "code":"data",
           (ldt->type & LDT_TYPE_DOWN)  ? " top-down":"",
           (ldt->type & LDT_TYPE_WRITE) ? " W":" ",
           (ldt->type & LDT_TYPE_ACCESSED)  ? " accessed":"");
}

static void
print_all_ldt(void)
{
    int i, bytes;
    /* can't fit 64K on our stack */
    raw_ldt_entry_t *ldt = global_heap_alloc(sizeof(raw_ldt_entry_t) * LDT_ENTRIES
                                           HEAPACCT(ACCT_OTHER));
    /* make sure our struct size jives w/ ldt.h */
    ASSERT(sizeof(raw_ldt_entry_t) == LDT_ENTRY_SIZE);
    memset(ldt, 0, sizeof(*ldt));
    bytes = modify_ldt_syscall(0, (void *)ldt, sizeof(raw_ldt_entry_t) * LDT_ENTRIES);
    LOG(GLOBAL, LOG_ALL, 3, "read %d bytes, should == %d * %d\n",
        bytes, sizeof(raw_ldt_entry_t), LDT_ENTRIES);
    ASSERT(bytes == 0 /* no ldt entries */ ||
           bytes == sizeof(raw_ldt_entry_t) * LDT_ENTRIES);
    for (i = 0; i < bytes/sizeof(raw_ldt_entry_t); i++) {
        if (((ldt[i].base3124<<24) | (ldt[i].base2316<<16) | ldt[i].base1500) != 0) {
            LOG(GLOBAL, LOG_ALL, 1, "ldt at index %d:\n", i);
            print_raw_ldt(&ldt[i]);
        }
    }
    global_heap_free(ldt, sizeof(raw_ldt_entry_t) * LDT_ENTRIES HEAPACCT(ACCT_OTHER));
}
# endif /* #if 0 */
#endif /* DEBUG */

#define LDT_ENTRIES_TO_CHECK 128

/* returns -1 if all indices are in use */
static int
find_unused_ldt_index()
{
    int i, bytes;
    /* N.B.: we don't have 64K of stack for the full LDT_ENTRIES
     * array, and I don't want to allocate a big array on the heap
     * when it's very doubtful any more than a handful of these
     * descriptors are actually in use
     */
    raw_ldt_entry_t ldt[LDT_ENTRIES_TO_CHECK];
    ASSERT(LDT_ENTRIES_TO_CHECK < LDT_ENTRIES);
    /* make sure our struct size jives w/ ldt.h */
    ASSERT(sizeof(raw_ldt_entry_t) == LDT_ENTRY_SIZE);
    memset(ldt, 0, sizeof(*ldt));
    bytes = modify_ldt_syscall(0, (void *)ldt, sizeof(ldt));
    if (bytes == 0) {
        /* no indices are taken yet */
        return 0;
    }
    ASSERT(bytes == sizeof(ldt));
    for (i = 0; i < bytes/sizeof(raw_ldt_entry_t); i++) {
        if (((ldt[i].base3124<<24) | (ldt[i].base2316<<16) | ldt[i].base1500) == 0) {
            return i;
        }
    }
    return -1;
}

static void
initialize_ldt_struct(our_modify_ldt_t *ldt, void *base, size_t size, uint index)
{
    ASSERT(ldt != NULL);
    ldt->entry_number = index;
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)base)));
    ldt->base_addr = (int)(ptr_int_t) base;
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(size)));
    ldt->limit = size;
    ldt->seg_32bit = IF_X64_ELSE(0, 1);
    ldt->contents = MODIFY_LDT_CONTENTS_DATA;
    ldt->read_exec_only = 0;
    ldt->limit_in_pages = (size == GDT_NO_SIZE_LIMIT) ? 1 : 0;
    ldt->seg_not_present = 0;
    /* While linux kernel doesn't care if we set this, vmkernel requires it */
    ldt->useable = 1; /* becomes custom AVL bit */
}

static void
clear_ldt_struct(our_modify_ldt_t *ldt, uint index)
{
    /* set fields to match LDT_empty() macro from linux kernel */
    memset(ldt, 0, sizeof(*ldt));
    ldt->seg_not_present = 1;
    ldt->read_exec_only = 1;
    ldt->entry_number = index;
}

static void
create_ldt_entry(void *base, size_t size, uint index)
{
    our_modify_ldt_t array;
    int ret;
    initialize_ldt_struct(&array, base, size, index);
    ret = modify_ldt_syscall(1, (void *)&array, sizeof(array));
    ASSERT(ret >= 0);
}

static void
clear_ldt_entry(uint index)
{
    our_modify_ldt_t array;
    int ret;
    clear_ldt_struct(&array, index);
    ret = modify_ldt_syscall(1, (void *)&array, sizeof(array));
    ASSERT(ret >= 0);
}
#endif /* HAVE_TLS */

/* segment selector format:
 * 15..............3      2          1..0
 *        index      0=GDT,1=LDT  Requested Privilege Level
 */
#define USER_PRIVILEGE  3
#define LDT_NOT_GDT     1
#define GDT_NOT_LDT     0
#define SELECTOR_IS_LDT  0x4
#define LDT_SELECTOR(idx) ((idx) << 3 | ((LDT_NOT_GDT) << 2) | (USER_PRIVILEGE))
#define GDT_SELECTOR(idx) ((idx) << 3 | ((GDT_NOT_LDT) << 2) | (USER_PRIVILEGE))
#define SELECTOR_INDEX(sel) ((sel) >> 3)

#define WRITE_DR_SEG(val) \
    ASSERT(sizeof(val) == sizeof(reg_t));                           \
    asm volatile("mov %0,%%"ASM_XAX"; mov %%"ASM_XAX", %"ASM_SEG";" \
                 : : "m" ((val)) : ASM_XAX);

#define WRITE_LIB_SEG(val) \
    ASSERT(sizeof(val) == sizeof(reg_t));                               \
    asm volatile("mov %0,%%"ASM_XAX"; mov %%"ASM_XAX", %"LIB_ASM_SEG";" \
                 : : "m" ((val)) : ASM_XAX);

static uint
read_selector(reg_id_t seg)
{
    uint sel;
    if (seg == SEG_FS) {
        asm volatile("movl %%fs, %0" : "=r"(sel));
    } else if (seg == SEG_GS) {
        asm volatile("movl %%gs, %0" : "=r"(sel));
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
    /* Pre-P6 family leaves upper 2 bytes undefined, so we clear them.  We don't
     * clear and then use movw because that takes an extra clock cycle, and gcc
     * can optimize this "and" into "test %?x, %?x" for calls from
     * is_segment_register_initialized().
     */
    sel &= 0xffff;
    return sel;
}

/* i#107: handle segment reg usage conflicts */
typedef struct _os_seg_info_t {
    int   tls_type;
    void *dr_fs_base;
    void *dr_gs_base;
    our_modify_ldt_t app_thread_areas[GDT_NUM_TLS_SLOTS];
} os_seg_info_t;

/* layout of our TLS */
typedef struct _os_local_state_t {
    /* put state first to ensure that it is cache-line-aligned */
    /* On Linux, we always use the extended structure. */
    local_state_extended_t state;
    /* linear address of tls page */
    struct _os_local_state_t *self;
    /* store what type of TLS this is so we can clean up properly */
    tls_type_t tls_type;
    /* For pre-SYS_set_thread_area kernels (pre-2.5.32, pre-NPTL), each
     * thread needs its own ldt entry */
    int ldt_index;
    /* tid needed to ensure children are set up properly */
    thread_id_t tid;
    /* i#107 application's gs/fs value and pointed-at base */
    ushort app_gs;      /* for mangling seg update/query */
    ushort app_fs;      /* for mangling seg update/query */
    void  *app_gs_base; /* for mangling segmented memory ref */
    void  *app_fs_base; /* for mangling segmented memory ref */
    union {
        /* i#107: We use space in os_tls to store thread area information
         * thread init. It will not conflict with the client_tls usage,
         * so we put them into a union for saving space. 
         */
        os_seg_info_t os_seg_info;
        void *client_tls[MAX_NUM_CLIENT_TLS];
    };
} os_local_state_t;

#define TLS_LOCAL_STATE_OFFSET (offsetof(os_local_state_t, state))

/* offset from top of page */
#define TLS_OS_LOCAL_STATE     0x00

#define TLS_SELF_OFFSET        (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, self))
#define TLS_THREAD_ID_OFFSET   (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, tid))
#define TLS_DCONTEXT_OFFSET    (TLS_OS_LOCAL_STATE + TLS_DCONTEXT_SLOT)

/* they should be used with os_tls_offset, so do not need add TLS_OS_LOCAL_STATE here */
#define TLS_APP_GS_BASE_OFFSET (offsetof(os_local_state_t, app_gs_base))
#define TLS_APP_FS_BASE_OFFSET (offsetof(os_local_state_t, app_fs_base))
#define TLS_APP_GS_OFFSET (offsetof(os_local_state_t, app_gs))
#define TLS_APP_FS_OFFSET (offsetof(os_local_state_t, app_fs))

/* N.B.: imm and idx are ushorts!
 * We use %c[0-9] to get gcc to emit an integer constant without a leading $ for
 * the segment offset.  See the documentation here:
 * http://gcc.gnu.org/onlinedocs/gccint/Output-Template.html#Output-Template
 * Also, var needs to match the pointer size, or else we'll get stack corruption.
 * XXX: This is marked volatile prevent gcc from speculating this code before
 * checks for is_segment_register_initialized(), but if we could find a more
 * precise constraint, then the compiler would be able to optimize better.  See
 * glibc comments on THREAD_SELF.
 */
#define WRITE_TLS_SLOT_IMM(imm, var)                                  \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                            \
    ASSERT(sizeof(var) == sizeof(void*));                             \
    asm volatile("mov %0, %"ASM_SEG":%c1" : : "r"(var), "i"(imm));

#define READ_TLS_SLOT_IMM(imm, var)                  \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());           \
    ASSERT(sizeof(var) == sizeof(void*));            \
    asm volatile("mov %"ASM_SEG":%c1, %0" : "=r"(var) : "i"(imm));

/* FIXME: need dedicated-storage var for _TLS_SLOT macros, can't use expr */
#define WRITE_TLS_SLOT(idx, var)                            \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                  \
    ASSERT(sizeof(var) == sizeof(void*));                   \
    ASSERT(sizeof(idx) == 2);                               \
    asm("mov %0, %%"ASM_XAX : : "m"((var)) : ASM_XAX);      \
    asm("movzw"IF_X64_ELSE("q","l")" %0, %%"ASM_XDX : : "m"((idx)) : ASM_XDX); \
    asm("mov %%"ASM_XAX", %"ASM_SEG":(%%"ASM_XDX")" : : : ASM_XAX, ASM_XDX);

#define READ_TLS_SLOT(idx, var)                                    \
    ASSERT(sizeof(var) == sizeof(void*));                          \
    ASSERT(sizeof(idx) == 2);                                      \
    asm("movzw"IF_X64_ELSE("q","l")" %0, %%"ASM_XAX : : "m"((idx)) : ASM_XAX); \
    asm("mov %"ASM_SEG":(%%"ASM_XAX"), %%"ASM_XAX : : : ASM_XAX);  \
    asm("mov %%"ASM_XAX", %0" : "=m"((var)) : : ASM_XAX);

/* FIXME: assumes that fs/gs is not already in use by app */
static bool
is_segment_register_initialized(void)
{
    if (read_selector(SEG_TLS) != 0)
        return true;
#ifdef X64
    if (tls_using_msr) {
        /* When the MSR is used, the selector in the register remains 0.
         * We can't clear the MSR early in a new thread and then look for
         * a zero base here b/c if kernel decides to use GDT that zeroing
         * will set the selector, unless we want to assume we know when
         * the kernel uses the GDT.
         * Instead we make a syscall to get the tid.  This should be ok
         * perf-wise b/c the common case is the non-zero above.
         */
        byte *base;
        int res = dynamorio_syscall(SYS_arch_prctl, 2,
                                    (SEG_TLS == SEG_FS ? ARCH_GET_FS : ARCH_GET_GS),
                                    &base);
        ASSERT(tls_type == TLS_TYPE_ARCH_PRCTL);
        if (res >= 0 && base != NULL) {
            os_local_state_t *os_tls = (os_local_state_t *) base;
            return (os_tls->tid == get_sys_thread_id());
        }
    }
#endif
    return false;
}

/* converts a local_state_t offset to a segment offset */
ushort
os_tls_offset(ushort tls_offs)
{
    /* no ushort truncation issues b/c TLS_LOCAL_STATE_OFFSET is 0 */
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    return (TLS_LOCAL_STATE_OFFSET + tls_offs);
}

/* XXX: Will return NULL if called before os_thread_init(), which sets
 * ostd->dr_fs/gs_base.
 */
void *
os_get_dr_seg_base(dcontext_t *dcontext, reg_id_t seg)
{
    os_thread_data_t *ostd;

    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(seg == SEG_FS || seg == SEG_GS);
    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return NULL;
    ostd = (os_thread_data_t *)dcontext->os_field;
    if (seg == SEG_FS)
        return ostd->dr_fs_base;
    else
        return ostd->dr_gs_base;
    return NULL;
}

static os_local_state_t *
get_os_tls(void)
{
    os_local_state_t *os_tls;
    ASSERT(is_segment_register_initialized());
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
    local_state = (byte*)dcontext->local_state;
    if (local_state == NULL)
        return NULL;
    return (os_local_state_t *)(local_state - offsetof(os_local_state_t, state));
}

void *
os_get_app_seg_base(dcontext_t *dcontext, reg_id_t seg)
{
    os_local_state_t *os_tls;
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(seg == SEG_FS || seg == SEG_GS);
    if (dcontext == NULL)
        dcontext = get_thread_private_dcontext();
    if (dcontext == NULL) {
        /* No dcontext means we haven't initialized TLS, so we haven't replaced
         * the app's segments.  get_segment_base is expensive, but this should
         * be rare.  Re-examine if it pops up in a profile.
         */
        return get_segment_base(seg);
    }
    os_tls = get_os_tls_from_dc(dcontext);
    if (seg == SEG_FS)
        return os_tls->app_fs_base;
    else 
        return os_tls->app_gs_base;
    ASSERT_NOT_REACHED();
    return NULL;
}

ushort
os_get_app_seg_base_offset(reg_id_t seg)
{
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    if (seg == SEG_FS)
        return TLS_APP_FS_BASE_OFFSET;
    else if (seg == SEG_GS)
        return TLS_APP_GS_BASE_OFFSET;
    ASSERT_NOT_REACHED();
    return 0;
}

ushort 
os_get_app_seg_offset(reg_id_t seg)
{
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    if (seg == SEG_FS)
        return TLS_APP_FS_OFFSET;
    else if (seg == SEG_GS)
        return TLS_APP_GS_OFFSET;
    ASSERT_NOT_REACHED();
    return 0;
}

void *
get_tls(ushort tls_offs)
{
    void *val;
    READ_TLS_SLOT(tls_offs, val);
    return val;
}

void
set_tls(ushort tls_offs, void *value)
{
    WRITE_TLS_SLOT(tls_offs, value);
}


#ifdef X86
/* Returns POINTER_MAX on failure.
 * Assumes that cs, ss, ds, and es are flat.
 * Should we export this to clients?  For now they can get
 * this information via opnd_compute_address().
 */
byte *
get_segment_base(uint seg)
{
    if (seg == SEG_CS || seg == SEG_SS || seg == SEG_DS || seg == SEG_ES)
        return NULL;
    else {
# ifdef HAVE_TLS
        uint selector = read_selector(seg);
        uint index = SELECTOR_INDEX(selector);
        LOG(THREAD_GET, LOG_THREADS, 4, "%s selector %x index %d ldt %d\n",
            __func__, selector, index, TEST(SELECTOR_IS_LDT, selector));

        if (TEST(SELECTOR_IS_LDT, selector)) {
            LOG(THREAD_GET, LOG_THREADS, 4, "selector is LDT\n");
            /* we have to read the entire ldt from 0 to the index */
            size_t sz = sizeof(raw_ldt_entry_t) * (index + 1);
            raw_ldt_entry_t *ldt = global_heap_alloc(sz HEAPACCT(ACCT_OTHER));
            int bytes;
            byte *base;
            memset(ldt, 0, sizeof(*ldt));
            bytes = modify_ldt_syscall(0, (void *)ldt, sz);
            base = (byte *)(ptr_uint_t) LDT_BASE(&ldt[index]);
            global_heap_free(ldt, sz HEAPACCT(ACCT_OTHER));
            if (bytes == sz) {
                LOG(THREAD_GET, LOG_THREADS, 4,
                    "modify_ldt %d => %x\n", index, base);
                return base;
            }
        } else {
            our_modify_ldt_t desc;
            int res;
#  ifdef X64
            byte *base;
            res = dynamorio_syscall(SYS_arch_prctl, 2,
                                    (seg == SEG_FS ? ARCH_GET_FS : ARCH_GET_GS), &base);
            if (res >= 0) {
                LOG(THREAD_GET, LOG_THREADS, 4,
                    "arch_prctl %s => "PFX"\n", reg_names[seg], base);
                return base;
            }
            /* else fall back on get_thread_area */
#  endif /* X64 */
            if (selector == 0)
                return NULL;
            DOCHECKINT(1, {
                uint max_idx =
                    IF_VMX86_ELSE(tls_gdt_index,
                                  (kernel_is_64bit() ? GDT_64BIT : GDT_32BIT));
                ASSERT_CURIOSITY(index <= max_idx && index >= (max_idx - 2));
            });
            initialize_ldt_struct(&desc, NULL, 0, index);
            res = dynamorio_syscall(SYS_get_thread_area, 1, &desc);
            if (res >= 0) {
                LOG(THREAD_GET, LOG_THREADS, 4,
                    "get_thread_area %d => %x\n", index, desc.base_addr);
                return (byte *)(ptr_uint_t) desc.base_addr;
            }
        }
# endif /* HAVE_TLS */
    }
    return (byte *) POINTER_MAX;
}

/* i#572: handle opnd_compute_address to return the application 
 * segment base value.
 */
byte *
get_app_segment_base(uint seg)
{
    if (seg == SEG_CS || seg == SEG_SS || seg == SEG_DS || seg == SEG_ES)
        return NULL;
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
        return get_tls(os_get_app_seg_base_offset(seg));
    }
    return get_segment_base(seg);
}
#endif

local_state_extended_t *
get_local_state_extended()
{
    os_local_state_t *os_tls;
    ASSERT(is_segment_register_initialized());
    READ_TLS_SLOT_IMM(TLS_SELF_OFFSET, os_tls);
    return &(os_tls->state);
}

local_state_t *
get_local_state()
{
#ifdef HAVE_TLS
    return (local_state_t *) get_local_state_extended();
#else
    return NULL;
#endif
}

/* i#107: handle segment register usage conflicts between app and dr:
 * os_handle_mov_seg updates the app's tls selector maintained by DR.
 * It is called before entering code cache in dispatch_enter_fcache.
 */
void
os_handle_mov_seg(dcontext_t *dcontext, byte *pc)
{
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
        sel = (ushort)reg_get_value_priv(opnd_get_reg(opnd), 
                                         get_mcontext(dcontext));
    } else {
        void *ptr;
        ptr = (ushort *)opnd_compute_address_priv(opnd, get_mcontext(dcontext));
        ASSERT(ptr != NULL);
        if (!safe_read(ptr, sizeof(sel), &sel)) {
            /* FIXME: if invalid address, should deliver a signal to user. */
            ASSERT_NOT_IMPLEMENTED(false);
        }
    }
    /* calculate the entry_number */
    desc_idx = SELECTOR_INDEX(sel) - gdt_entry_tls_min;
    if (seg == SEG_GS) {
        os_tls->app_gs = sel;
        os_tls->app_gs_base = (void *)(ptr_uint_t) desc[desc_idx].base_addr;
    } else {
        os_tls->app_fs = sel;
        os_tls->app_fs_base = (void *)(ptr_uint_t) desc[desc_idx].base_addr;
    }
    instr_free(dcontext, &instr);
    LOG(THREAD_GET, LOG_THREADS, 2,
        "thread %d segment change %s to selector 0x%x => app fs: "PFX", gs: "PFX"\n",
        get_thread_id(), reg_names[seg], sel, os_tls->app_fs_base, os_tls->app_gs_base);
}

/* Queries the set of available GDT slots, and initializes:
 * - tls_gdt_index
 * - gdt_entry_tls_min on ia32
 * - lib_tls_gdt_index if using private loader
 * GDT slots are initialized with a base and limit of zero.  The caller is
 * responsible for setting them to a real base.
 */
static void
choose_gdt_slots(os_local_state_t *os_tls)
{
    static bool tls_global_init = false;
    our_modify_ldt_t desc;
    int i;
    int avail_index[GDT_NUM_TLS_SLOTS];
    our_modify_ldt_t clear_desc;
    int res;

    /* using local static b/c dynamo_initialized is not set for a client thread
     * when created in client's dr_init routine
     */
    /* FIXME: Could be racy if we have multiple threads initializing during
     * startup.
     */
    if (tls_global_init)
        return;
    tls_global_init = true;

    /* We don't want to break the assumptions of pthreads or wine,
     * so we try to take the last slot.  We don't want to hardcode
     * the index b/c the kernel will let us clobber entries so we want
     * to only pass in -1.
     */
    ASSERT(!dynamo_initialized);
    ASSERT(tls_gdt_index == -1);
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++)
        avail_index[i] = -1;
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        /* We use a base and limit of 0 for testing what's available. */
        initialize_ldt_struct(&desc, NULL, 0, -1);
        res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
        LOG(GLOBAL, LOG_THREADS, 4,
            "%s: set_thread_area -1 => %d res, %d index\n",
            __FUNCTION__, res, desc.entry_number);
        if (res >= 0) {
            /* We assume monotonic increases */
            avail_index[i] = desc.entry_number;
            ASSERT(avail_index[i] > tls_gdt_index);
            tls_gdt_index = desc.entry_number;
        } else
            break;
    }

#ifndef X64
    /* In x86-64's ia32 emulation,
     * set_thread_area(6 <= entry_number && entry_number <= 8) fails
     * with EINVAL (22) because x86-64 only accepts GDT indices 12 to 14
     * for TLS entries.
     */
    if (tls_gdt_index > (gdt_entry_tls_min + GDT_NUM_TLS_SLOTS))
        gdt_entry_tls_min = GDT_ENTRY_TLS_MIN_64;  /* The kernel is x64. */
#endif

    /* Now give up the earlier slots */
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        if (avail_index[i] > -1 &&
            avail_index[i] != tls_gdt_index) {
            LOG(GLOBAL, LOG_THREADS, 4,
                "clearing set_thread_area index %d\n", avail_index[i]);
            clear_ldt_struct(&clear_desc, avail_index[i]);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &clear_desc);
            ASSERT(res >= 0);
        }
    }

# ifndef VMX86_SERVER
    ASSERT_CURIOSITY(tls_gdt_index ==
                     (kernel_is_64bit() ? GDT_64BIT : GDT_32BIT));
# endif

# ifdef CLIENT_INTERFACE
    if (INTERNAL_OPTION(private_loader) && tls_gdt_index != -1) {
        /* Use the app's selector with our own TLS base for libraries.  app_fs
         * and app_gs are initialized by the caller in os_tls_app_seg_init().
         */
        int index = SELECTOR_INDEX(IF_X64_ELSE(os_tls->app_fs,
                                               os_tls->app_gs));
        if (index == 0) {
            /* An index of zero means the app has no TLS (yet), and happens
             * during early injection.  We use -1 to grab a new entry.  When the
             * app asks for its first table entry with set_thread_area, we give
             * it this one and emulate its usage of the segment.
             */
            ASSERT_CURIOSITY(DYNAMO_OPTION(early_inject) && "app has "
                             "no TLS, but we used non-early injection");
            initialize_ldt_struct(&desc, NULL, 0, -1);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 4,
                "%s: set_thread_area -1 => %d res, %d index\n",
                __FUNCTION__, res, desc.entry_number);
            ASSERT(res >= 0);
            if (res >= 0) {
                return_stolen_lib_tls_gdt = true;
                index = desc.entry_number;
            }
        }
        lib_tls_gdt_index = index;
    }
# endif
}

/* initialization for mangle_app_seg, must be called before
 * DR setup its own segment.
 */
static void
os_tls_app_seg_init(os_local_state_t *os_tls, void *segment)
{
    int i, index;
    our_modify_ldt_t *desc;
    app_pc app_fs_base, app_gs_base;

    os_tls->app_fs = read_selector(SEG_FS);
    os_tls->app_gs = read_selector(SEG_GS);
    app_fs_base = get_segment_base(SEG_FS);
    app_gs_base = get_segment_base(SEG_GS);
    /* If we're a non-initial thread, fs/gs will be set to the parent's value */
    if (!is_dynamo_address(app_gs_base))
        os_tls->app_gs_base = app_gs_base;
    else
        os_tls->app_gs_base = NULL;
    if (!is_dynamo_address(app_fs_base))
        os_tls->app_fs_base = app_fs_base;
    else
        os_tls->app_fs_base = NULL;

    /* get all TLS thread area value */
    /* XXX: is get_thread_area supported in 64-bit kernel?
     * It has syscall number 211.
     * It works for a 32-bit application running in a 64-bit kernel.
     * It returns error value -38 for a 64-bit app in a 64-bit kernel.
     */
    desc = &os_tls->os_seg_info.app_thread_areas[0];
#ifndef X64
    /* Initialize gdt_entry_tls_min on ia32.  On x64, the initial value is
     * correct.
     */
    choose_gdt_slots(os_tls);
#endif
    index = gdt_entry_tls_min;
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        int res;
        initialize_ldt_struct(&desc[i], NULL, 0, i + index);
        res = dynamorio_syscall(SYS_get_thread_area, 1, &desc[i]);
        if (res < 0)
            clear_ldt_struct(&desc[i], i + index);
    }

    os_tls->os_seg_info.dr_fs_base = IF_X64_ELSE(NULL, segment);
    os_tls->os_seg_info.dr_gs_base = IF_X64_ELSE(segment, NULL);
    /* now allocate the tls segment for client libraries */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
#ifdef X64
        os_tls->os_seg_info.dr_fs_base = privload_tls_init(os_tls->app_fs_base);
#else
        os_tls->os_seg_info.dr_gs_base = privload_tls_init(os_tls->app_gs_base);
#endif
    }

    LOG(THREAD_GET, LOG_THREADS, 1, "thread %d app fs: "PFX", gs: "PFX"\n",
        get_thread_id(), os_tls->app_fs_base, os_tls->app_gs_base);
    LOG(THREAD_GET, LOG_THREADS, 1, "thread %d DR fs: "PFX", gs: "PFX"\n",
        get_thread_id(), os_tls->os_seg_info.dr_fs_base, os_tls->os_seg_info.dr_gs_base);
}

void
os_tls_init(void)
{
#ifdef HAVE_TLS
    /* We create a 1-page segment with an LDT entry for each thread and load its
     * selector into fs/gs.
     * FIXME PR 205276: this whole scheme currently does not check if app is using
     * segments need to watch modify_ldt syscall
     */
    /* FIXME: heap_mmap marks as exec, we just want RW */
    byte *segment = heap_mmap(PAGE_SIZE);
    os_local_state_t *os_tls = (os_local_state_t *) segment;
    int index = -1;
    uint selector;
    int res;
# ifdef X64
    byte *cur_gs;
# endif

    LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init for thread "IDFMT"\n", get_thread_id());

    /* MUST zero out dcontext slot so uninit access gets NULL */
    memset(segment, 0, PAGE_SIZE);
    /* store key data in the tls itself */
    os_tls->self = os_tls;
    os_tls->tid = get_thread_id();
    os_tls->tls_type = TLS_TYPE_NONE;
    /* We save DR's TLS segment base here so that os_get_dr_seg_base() will work
     * even when -no_mangle_app_seg is set.  If -mangle_app_seg is set, this
     * will be overwritten in os_tls_app_seg_init().
     */
    os_tls->os_seg_info.IF_X64_ELSE(dr_gs_base, dr_fs_base) = segment;
    ASSERT(proc_is_cache_aligned(os_tls->self + TLS_LOCAL_STATE_OFFSET));
    /* Verify that local_state_extended_t should indeed be used. */
    ASSERT(DYNAMO_OPTION(ibl_table_in_tls));

    /* get application's GS/FS segment base before being replaced by DR. */
    if (INTERNAL_OPTION(mangle_app_seg))
        os_tls_app_seg_init(os_tls, segment);
    /* We have four different ways to obtain TLS, each with its own limitations:
     *
     * 1) Piggyback on the threading system (like we do on Windows): here that would
     *    be pthreads, which uses a segment since at least RH9, and uses gdt-based
     *    segments for NPTL.  The advantage is we won't run out of ldt or gdt entries
     *    (except when the app itself would).  The disadvantage is we're stealing
     *    application slots and we rely on user mode interfaces.
     *
     * 2) Steal an ldt entry via SYS_modify_ldt.  This suffers from the 8K ldt entry
     *    limit and requires that we update manually on a new thread.  For 64-bit
     *    we're limited here to a 32-bit base.  (Strangely, the kernel's
     *    include/asm-x86_64/ldt.h implies that the base is ignored: but it doesn't
     *    seem to be.)
     * 
     * 3) Steal a gdt entry via SYS_set_thread_area.  There is a 3rd unused entry
     *    (after pthreads and wine) we could use.  The kernel swaps for us, and with
     *    CLONE_TLS the kernel will set up the entry for a new thread for us.  Xref
     *    PR 192231 and PR 285898.  This system call is disabled on 64-bit 2.6
     *    kernels (though the man page for arch_prctl implies it isn't for 2.5
     *    kernels?!?)
     *
     * 4) Use SYS_arch_prctl.  This is only implemented on 64-bit kernels, and can
     *    only be used to set the gdt entries that fs and gs select for.  Faster to
     *    use <4GB base (obtain with mmap MAP_32BIT) since can use gdt; else have to
     *    use wrmsr.  The man pages say "ARCH_SET_GS is disabled in some kernels".
     */

# ifdef X64
    /* First choice is gdt, which means arch_prctl.  Since this may fail
     * on some kernels, we require -heap_in_lower_4GB so we can fall back
     * on modify_ldt.
     */
    res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_GET_GS, &cur_gs);
    if (res >= 0) {
        LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: cur gs base is "PFX"\n", cur_gs);
        /* If we're a non-initial thread, gs will be set to the parent thread's value */
        if (cur_gs == NULL || is_dynamo_address(cur_gs) || 
            /* By resolving i#107, we can handle gs conflicts between app and dr. */
            INTERNAL_OPTION(mangle_app_seg)) {
            res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, segment);
            if (res >= 0) {
                os_tls->tls_type = TLS_TYPE_ARCH_PRCTL;
                LOG(GLOBAL, LOG_THREADS, 1,
                    "os_tls_init: arch_prctl successful for base "PFX"\n", segment);
                /* Kernel should have written %gs for us if using GDT */
                if (!dynamo_initialized && read_selector(SEG_TLS) == 0)
                    tls_using_msr = true;
                if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
                    res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_FS, 
                                            os_tls->os_seg_info.dr_fs_base);
                    /* Assuming set fs must be successful if set gs succeeded. */
                    ASSERT(res >= 0);
                }
            } else {
                /* we've found a kernel where ARCH_SET_GS is disabled */
                ASSERT_CURIOSITY(false && "arch_prctl failed on set but not get");
                LOG(GLOBAL, LOG_THREADS, 1,
                    "os_tls_init: arch_prctl failed: error %d\n", res);
            }
        } else {
            /* FIXME PR 205276: we don't currently handle it: fall back on ldt, but
             * we'll have the same conflict w/ the selector...
             */
            ASSERT_BUG_NUM(205276, cur_gs == NULL);
        }
    }
# endif

    if (os_tls->tls_type == TLS_TYPE_NONE) {
        /* Second choice is set_thread_area */
        /* PR 285898: if we added CLONE_SETTLS to all clone calls (and emulated vfork
         * with clone) we could avoid having to set tls up for each thread (as well
         * as solve race PR 207903), at least for kernel 2.5.32+.  For now we stick
         * w/ manual setup.
         */
        our_modify_ldt_t desc;

        /* Pick which GDT slots we'll use for DR TLS and for library TLS if
         * using the private loader.
         */
        choose_gdt_slots(os_tls);

        if (tls_gdt_index > -1) {
            /* Now that we know which GDT slot to use, install the per-thread base
             * into it.
             */
            /* Base here must be 32-bit */
            IF_X64(ASSERT(DYNAMO_OPTION(heap_in_lower_4GB) &&
                          segment <= (byte*)UINT_MAX));
            initialize_ldt_struct(&desc, segment, PAGE_SIZE, tls_gdt_index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 3,
                "%s: set_thread_area %d => %d res, %d index\n",
                __FUNCTION__, tls_gdt_index, res, desc.entry_number);
            ASSERT(res < 0 || desc.entry_number == tls_gdt_index);
        } else {
            res = -1;  /* fall back on LDT */
        }

        if (res >= 0) {
            LOG(GLOBAL, LOG_THREADS, 1,
                "os_tls_init: set_thread_area successful for base "PFX" @index %d\n",
                segment, tls_gdt_index);
            os_tls->tls_type = TLS_TYPE_GDT;
            index = tls_gdt_index;
            selector = GDT_SELECTOR(index);
            WRITE_DR_SEG(selector); /* macro needs lvalue! */
        } else {
            IF_VMX86(ASSERT_NOT_REACHED()); /* since no modify_ldt */
            LOG(GLOBAL, LOG_THREADS, 1,
                "os_tls_init: set_thread_area failed: error %d\n", res);
        }

# ifdef CLIENT_INTERFACE
        /* Install the library TLS base. */
        if (INTERNAL_OPTION(private_loader) && res >= 0) {
            app_pc base = IF_X64_ELSE(os_tls->os_seg_info.dr_fs_base,
                                      os_tls->os_seg_info.dr_gs_base);
            /* lib_tls_gdt_index is picked in choose_gdt_slots. */
            ASSERT(lib_tls_gdt_index >= gdt_entry_tls_min);
            initialize_ldt_struct(&desc, base, GDT_NO_SIZE_LIMIT,
                                  lib_tls_gdt_index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 3,
                "%s: set_thread_area %d => %d res, %d index\n",
                __FUNCTION__, lib_tls_gdt_index, res, desc.entry_number);
            if (res >= 0) {
                /* i558 update lib seg reg to enforce the segment changes */
                selector = GDT_SELECTOR(lib_tls_gdt_index);
                LOG(GLOBAL, LOG_THREADS, 2, "%s: setting %s to selector 0x%x\n",
                    __FUNCTION__, reg_names[LIB_SEG_TLS], selector);
                WRITE_LIB_SEG(selector);
            }
        }
# endif
    }

    if (os_tls->tls_type == TLS_TYPE_NONE) {
        /* Third choice: modify_ldt, which should be available on kernel 2.3.99+ */
        /* Base here must be 32-bit */
        IF_X64(ASSERT(DYNAMO_OPTION(heap_in_lower_4GB) && segment <= (byte*)UINT_MAX));
        /* we have the thread_initexit_lock so no race here */
        index = find_unused_ldt_index();
        selector = LDT_SELECTOR(index);
        ASSERT(index != -1);
        create_ldt_entry((void *)segment, PAGE_SIZE, index);
        os_tls->tls_type = TLS_TYPE_LDT;
        WRITE_DR_SEG(selector); /* macro needs lvalue! */
        LOG(GLOBAL, LOG_THREADS, 1,
            "os_tls_init: modify_ldt successful for base "PFX" w/ index %d\n",
            segment, index);
    }
    os_tls->ldt_index = index;
    ASSERT(os_tls->tls_type != TLS_TYPE_NONE);
    /* FIXME: this should be a SYSLOG fatal error?  Should fall back on !HAVE_TLS?
     * Should have create_ldt_entry() return failure instead of asserting, then.
     */
#else
    tls_table = (tls_slot_t *)
        global_heap_alloc(MAX_THREADS*sizeof(tls_slot_t) HEAPACCT(ACCT_OTHER));
    memset(tls_table, 0, MAX_THREADS*sizeof(tls_slot_t));
#endif
    /* store type in global var for convenience: should be same for all threads */
    tls_type = os_tls->tls_type;
    ASSERT(is_segment_register_initialized());
}

/* Frees local_state.  If the calling thread is exiting (i.e.,
 * !other_thread) then also frees kernel resources for the calling
 * thread; if other_thread then that may not be possible.
 */
void
os_tls_exit(local_state_t *local_state, bool other_thread)
{
#ifdef HAVE_TLS
    int res;
    static const ptr_uint_t zero = 0;
    /* We can't read from fs: as we can be called from other threads */
    /* ASSUMPTION: local_state_t is laid out at same start as local_state_extended_t */
    os_local_state_t *os_tls = (os_local_state_t *)
        (((byte*)local_state) - offsetof(os_local_state_t, state));
    tls_type_t tls_type = os_tls->tls_type;
    int index = os_tls->ldt_index;

    /* If the MSR is in use, writing to the reg faults.  We rely on it being 0
     * to indicate that.
     */
    if (!other_thread && read_selector(SEG_TLS) != 0) {
        WRITE_DR_SEG(zero); /* macro needs lvalue! */
    }
    heap_munmap(os_tls->self, PAGE_SIZE);

    /* For another thread we can't really make these syscalls so we have to
     * leave it un-cleaned-up.  That's fine if the other thread is exiting:
     * but if we have a detach feature (i#95) we'll have to get the other
     * thread to run this code.
     */
    if (!other_thread) {
        if (tls_type == TLS_TYPE_LDT)
            clear_ldt_entry(index);
        else if (tls_type == TLS_TYPE_GDT) {
            our_modify_ldt_t desc;
            clear_ldt_struct(&desc, index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            ASSERT(res >= 0);        
        }
# ifdef X64
        else if (tls_type == TLS_TYPE_ARCH_PRCTL) {
            res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, NULL);
            ASSERT(res >= 0);
            /* syscall re-sets gs register so re-clear it */
            if (read_selector(SEG_TLS) != 0) {
                WRITE_DR_SEG(zero); /* macro needs lvalue! */
            }
        }
# endif
    }
#else
    global_heap_free(tls_table, MAX_THREADS*sizeof(tls_slot_t) HEAPACCT(ACCT_OTHER));
    DELETE_LOCK(tls_lock);
#endif
}

static int
os_tls_get_gdt_index(dcontext_t *dcontext)
{
    os_local_state_t *os_tls = (os_local_state_t *)
        (((byte*)dcontext->local_state) - offsetof(os_local_state_t, state));
    if (os_tls->tls_type == TLS_TYPE_GDT)
        return os_tls->ldt_index;
    else
        return -1;
}

void
os_tls_pre_init(int gdt_index)
{
    /* Only set to above 0 for tls_type == TLS_TYPE_GDT */
    if (gdt_index > 0) {
        /* PR 458917: clear gdt slot to avoid leak across exec */ 
        our_modify_ldt_t desc;
        int res;
        static const ptr_uint_t zero = 0;
        /* Be sure to clear the selector before anything that might
         * call get_thread_private_dcontext()
         */
        WRITE_DR_SEG(zero); /* macro needs lvalue! */
        clear_ldt_struct(&desc, gdt_index);
        res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
        ASSERT(res >= 0);        
    }
}

#ifdef CLIENT_INTERFACE
/* Allocates num_slots tls slots aligned with alignment align */
bool
os_tls_calloc(OUT uint *offset, uint num_slots, uint alignment)
{
    bool res = false;
    uint i, count = 0;
    int start = -1;
    uint offs = offsetof(os_local_state_t, client_tls);
    if (num_slots > MAX_NUM_CLIENT_TLS)
        return false;
    mutex_lock(&client_tls_lock);
    for (i = 0; i < MAX_NUM_CLIENT_TLS; i++) {
        if (!client_tls_allocated[i] &&
            /* ALIGNED doesn't work for 0 */
            (alignment == 0 || ALIGNED(offs + i*sizeof(void*), alignment))) {
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
        *offset = offs + start*sizeof(void*);
        res = true;
    }
    mutex_unlock(&client_tls_lock);
    return res;
}

bool
os_tls_cfree(uint offset, uint num_slots)
{
    uint i;
    uint offs = (offset - offsetof(os_local_state_t, client_tls))/sizeof(void*);
    bool ok = true;
    mutex_lock(&client_tls_lock);
    for (i = 0; i < num_slots; i++) {
        if (!client_tls_allocated[i + offs])
            ok = false;
        client_tls_allocated[i + offs] = false;
    }
    mutex_unlock(&client_tls_lock);
    return ok;
}
#endif

void
os_thread_init(dcontext_t *dcontext)
{
    os_local_state_t *os_tls = get_os_tls();
    os_thread_data_t *ostd = (os_thread_data_t *)
        heap_alloc(dcontext, sizeof(os_thread_data_t) HEAPACCT(ACCT_OTHER));
    dcontext->os_field = (void *) ostd;
    /* make sure stack fields, etc. are 0 now so they can be initialized on demand
     * (don't have app esp register handy here to init now)
     */
    memset(ostd, 0, sizeof(*ostd));

#ifdef RETURN_AFTER_CALL
    if (!dynamo_initialized) {
        /* Find the bottom of the stack of the initial (native) entry */
        ostd->stack_bottom_pc = find_stack_bottom();
        LOG(THREAD, LOG_ALL, 1, "Stack bottom pc = "PFX"\n", ostd->stack_bottom_pc);
    } else {
        /* We only need the stack bottom for the initial thread */
        ostd->stack_bottom_pc = NULL;
    }
#endif

    ASSIGN_INIT_LOCK_FREE(ostd->suspend_lock, suspend_lock);

    signal_thread_init(dcontext);

    /* i#107, initialize thread area information,
     * the value was first get in os_tls_init and stored in os_tls
     */
    ostd->dr_gs_base = os_tls->os_seg_info.dr_gs_base;
    ostd->dr_fs_base = os_tls->os_seg_info.dr_fs_base;
    if (INTERNAL_OPTION(mangle_app_seg)) {
        ostd->app_thread_areas = 
            heap_alloc(dcontext, sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS
                       HEAPACCT(ACCT_OTHER));
        memcpy(ostd->app_thread_areas,
               os_tls->os_seg_info.app_thread_areas,
               sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS);
    }

    LOG(THREAD, LOG_THREADS, 1, "cur gs base is "PFX"\n", get_segment_base(SEG_GS));
    LOG(THREAD, LOG_THREADS, 1, "cur fs base is "PFX"\n", get_segment_base(SEG_FS));
}

void
os_thread_exit(dcontext_t *dcontext, bool other_thread)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;

    /* i#237/PR 498284: if we had a vfork child call execve we need to clean up
     * the env vars.
     */
    if (dcontext->thread_record->execve)
        handle_execve_post(dcontext);

    DELETE_LOCK(ostd->suspend_lock);

    signal_thread_exit(dcontext, other_thread);

    /* for non-debug we do fast exit path and don't free local heap */
    DODEBUG({
        if (INTERNAL_OPTION(mangle_app_seg)) {
            heap_free(dcontext, ostd->app_thread_areas, 
                      sizeof(our_modify_ldt_t) * GDT_NUM_TLS_SLOTS 
                      HEAPACCT(ACCT_OTHER));
#ifdef CLIENT_INTERFACE
            if (INTERNAL_OPTION(private_loader)) {
                privload_tls_exit(IF_X64_ELSE(ostd->dr_fs_base, 
                                              ostd->dr_gs_base));
            }
#endif
        }
        heap_free(dcontext, ostd, sizeof(os_thread_data_t) HEAPACCT(ACCT_OTHER));
    });
}

/* Happens in the parent prior to fork. */
static void
os_fork_pre(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;

    /* Otherwise a thread might wait for us. */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(ostd->fork_threads == NULL && ostd->fork_num_threads == 0);

    /* i#239: Synch with all other threads to ensure that they are holding no
     * locks across the fork.
     * FIXME i#26: Suspend signals received before initializing siginfo are
     * squelched, so we won't be able to suspend threads that are initializing.
     */
    LOG(GLOBAL, 2, LOG_SYSCALLS|LOG_THREADS,
        "fork: synching with other threads to prevent deadlock in child\n");
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER,
                                &ostd->fork_threads,
                                &ostd->fork_num_threads,
                                THREAD_SYNCH_VALID_MCONTEXT,
                                /* If we fail to suspend a thread, there is a
                                 * risk of deadlock in the child, so it's worth
                                 * retrying on failure.
                                 */
                                THREAD_SYNCH_SUSPEND_FAILURE_RETRY)) {
        /* If we failed to synch with all threads, we live with the possiblity
         * of deadlock and continue as normal.
         */
        LOG(GLOBAL, 1, LOG_SYSCALLS|LOG_THREADS,
            "fork: synch failed, possible deadlock in child\n");
        ASSERT_CURIOSITY(false);
    }

    /* We go back to the code cache to execute the syscall, so we can't hold
     * locks.  If the synch succeeded, no one else is running, so it should be
     * safe to release these locks.  However, if there are any rogue threads,
     * then releasing these locks will allow them to synch and create threads.
     * Such threads could be running due to synch failure or presence of
     * non-suspendable client threads.  We keep our data in ostd to prevent some
     * conflicts, but there are some unhandled corner cases.
     */
    mutex_unlock(&thread_initexit_lock);
    mutex_unlock(&all_threads_synch_lock);
}

/* Happens after the fork in both the parent and child. */
static void
os_fork_post(dcontext_t *dcontext, bool parent)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
    /* Re-acquire the locks we released before the fork. */
    mutex_lock(&all_threads_synch_lock);
    mutex_lock(&thread_initexit_lock);
    /* Resume the other threads that we suspended. */
    if (parent) {
        LOG(GLOBAL, 2, LOG_SYSCALLS|LOG_THREADS,
            "fork: resuming other threads after fork\n");
    }
    end_synch_with_all_threads(ostd->fork_threads, ostd->fork_num_threads,
                               parent/*resume in parent, not in child*/);
    ostd->fork_threads = NULL;  /* Freed by end_synch_with_all_threads. */
    ostd->fork_num_threads = 0;
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
    mutex_fork_reset(&all_threads_synch_lock);
    mutex_fork_reset(&thread_initexit_lock);

    os_fork_post(dcontext, false/*!parent*/);

    /* re-populate cached data that contains pid */
    pid_cached = get_process_id();
    get_application_pid_helper(true);
    get_application_name_helper(true, true /* not important */);

    /* close all copies of parent files */
    TABLE_RWLOCK(fd_table, write, lock);
    iter = 0;
    do {
         iter = generic_hash_iterate_next(GLOBAL_DCONTEXT, fd_table, iter,
                                          &fd, (void **)&flags);
         if (iter < 0)
             break;
         if (TEST(OS_OPEN_CLOSE_ON_FORK, flags)) {
             close_syscall((file_t)fd);
             iter = generic_hash_iterate_remove(GLOBAL_DCONTEXT, fd_table,
                                                iter, fd);
         }
    } while (true);
    TABLE_RWLOCK(fd_table, write, unlock);
}

/* We only bother swapping the library segment if we're using the private
 * loader.
 */
bool
os_should_swap_state(void)
{
    /* -private_loader currently implies -mangle_app_seg, but let's be safe. */
    return (INTERNAL_OPTION(mangle_app_seg) &&
            IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false));
}

bool
os_using_app_state(dcontext_t *dcontext)
{
    /* FIXME: This could be optimized to avoid the syscall by keeping state in
     * the dcontext.
     */
    if (INTERNAL_OPTION(mangle_app_seg)) {
        return (get_segment_base(LIB_SEG_TLS) ==
                os_get_app_seg_base(dcontext, LIB_SEG_TLS));
    }
    /* We're always in the app state if we're not mangling. */
    return true;
}

/* Similar to PEB swapping on Windows, this call will switch between DR's
 * private lib segment base and the app's segment base.
 * i#107: If the app wants to use SEG_TLS, we should also switch that back at
 * this boundary, but there are many places where we simply assume it is always
 * installed.
 */
void
os_swap_context(dcontext_t *dcontext, bool to_app)
{
    if (os_should_swap_state())
        os_switch_seg_to_context(dcontext, LIB_SEG_TLS, to_app);
}

void
os_thread_under_dynamo(dcontext_t *dcontext)
{
    os_swap_context(dcontext, false/*to dr*/);
    start_itimer(dcontext);
}

void
os_thread_not_under_dynamo(dcontext_t *dcontext)
{
    stop_itimer(dcontext);
    os_swap_context(dcontext, true/*to app*/);
}

static pid_t
get_process_group_id()
{
    return dynamorio_syscall(SYS_getpgid, 0);
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER: around most of file, to exclude preload */
process_id_t
get_process_id()
{
    return dynamorio_syscall(SYS_getpid, 0);
}
#ifndef NOT_DYNAMORIO_CORE_PROPER /* around most of file, to exclude preload */

process_id_t
get_parent_id(void)
{
    return dynamorio_syscall(SYS_getppid, 0);
}

thread_id_t 
get_sys_thread_id(void)
{
    if (kernel_thread_groups) {
        return dynamorio_syscall(SYS_gettid, 0);
    } else {
        return dynamorio_syscall(SYS_getpid, 0);
    }
}

thread_id_t 
get_thread_id(void)
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
    ptr_int_t tid; /* can't use thread_id_t since it's 32-bits */
    if (!is_segment_register_initialized())
        return INVALID_THREAD_ID;
    READ_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, tid);
    /* it reads 8-bytes into the memory, which includes app_gs and app_fs.
     * 0x000000007127357b <get_tls_thread_id+37>:      mov    %gs:(%rax),%rax
     * 0x000000007127357f <get_tls_thread_id+41>:      mov    %rax,-0x8(%rbp)
     * so we remove the TRUNCATE check and trucate it on return.
     */
    return (thread_id_t) tid;
}

/* returns the thread-private dcontext pointer for the calling thread */
dcontext_t*
get_thread_private_dcontext(void)
{
#ifdef HAVE_TLS
    dcontext_t *dcontext;
    /* We have to check this b/c this is called from __errno_location prior
     * to os_tls_init, as well as after os_tls_exit, and early in a new
     * thread's initialization (see comments below on that).
     */
    if (!is_segment_register_initialized())
        return (IF_CLIENT_INTERFACE(standalone_library ? GLOBAL_DCONTEXT :) NULL);
    /* We used to check tid and return NULL to distinguish parent from child, but
     * that was affecting performance (xref PR 207366: but I'm leaving the assert in
     * for now so debug build will still incur it).  So we fixed the cases that
     * needed that:
     *
     * - dynamo_thread_init() calling is_thread_initialized() for a new thread 
     *   created via clone or the start/stop interface: so we have 
     *   is_thread_initialized() pay the get_thread_id() cost.
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
    DOCHECK(CHKLVL_DEFAULT+1, {
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
    thread_id_t tid = get_thread_id();
    int i;
    if (tls_table != NULL) {
        for (i=0; i<MAX_THREADS; i++) {
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
    ASSERT(is_segment_register_initialized());
    WRITE_TLS_SLOT_IMM(TLS_DCONTEXT_OFFSET, dcontext);
#else
    thread_id_t tid = get_thread_id();
    int i;
    bool found = false;
    ASSERT(tls_table != NULL);
    mutex_lock(&tls_lock);
    for (i=0; i<MAX_THREADS; i++) {
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
            for (i=0; i<MAX_THREADS; i++) {
                if (tls_table[i].tid == 0) {
                    tls_table[i].tid = tid;
                    tls_table[i].dcontext = dcontext;
                    found = true;
                    break;
                }
            }
        }
    }
    mutex_unlock(&tls_lock);
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
    ptr_int_t new_tid = new; /* can't use thread_id_t since it's 32-bits */
    ASSERT(is_segment_register_initialized());
    DOCHECK(1, {
        ptr_int_t old_tid; /* can't use thread_id_t since it's 32-bits */
        READ_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, old_tid);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(old_tid)));
        ASSERT(old_tid == old);
    });
    WRITE_TLS_SLOT_IMM(TLS_THREAD_ID_OFFSET, new_tid);
#else
    int i;
    mutex_lock(&tls_lock);
    for (i=0; i<MAX_THREADS; i++) {
        if (tls_table[i].tid == old) {
            tls_table[i].tid = new;
            break;
        }
    }
    mutex_unlock(&tls_lock);
#endif
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* translate permission string to platform independent protection bits */
static inline uint
permstr_to_memprot(const char * const perm)
{
    uint mem_prot = 0;
    if (perm == NULL || *perm == '\0')
        return mem_prot;
    if (perm[2]=='x')
        mem_prot |= MEMPROT_EXEC;
    if (perm[1]=='w')
        mem_prot |= MEMPROT_WRITE;
    if (perm[0]=='r')
        mem_prot |= MEMPROT_READ;
    return mem_prot;
}

/* translate platform independent protection bits to native flags */
uint
memprot_to_osprot(uint prot)
{
    uint mmap_prot = 0;
    if (TEST(MEMPROT_EXEC, prot))
        mmap_prot |= PROT_EXEC;
    if (TEST(MEMPROT_READ, prot))
        mmap_prot |= PROT_READ;
    if (TEST(MEMPROT_WRITE, prot))
        mmap_prot |= PROT_WRITE;
    return mmap_prot;
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

bool
mmap_syscall_succeeded(byte *retval)
{
    ptr_int_t result = (ptr_int_t) retval;
    /* libc interprets up to -PAGE_SIZE as an error, and you never know if
     * some weird errno will be used by say vmkernel (xref PR 365331) 
     */
    bool fail = (result < 0 && result >= -PAGE_SIZE);
    ASSERT_CURIOSITY(!fail ||
                     IF_VMX86(result == -ENOENT ||)
                     IF_VMX86(result == -ENOSPC ||)
                     result == -EBADF   ||
                     result == -EACCES  ||
                     result == -EINVAL  ||
                     result == -ETXTBSY ||
                     result == -EAGAIN  ||
                     result == -ENOMEM  ||
                     result == -ENODEV  ||
                     result == -EFAULT);
    return !fail;
}

static inline byte *
mmap_syscall(byte *addr, size_t len, ulong prot, ulong flags, ulong fd, ulong pgoff)
{
    return (byte *) dynamorio_syscall(IF_X64_ELSE(SYS_mmap, SYS_mmap2), 6,
                                      addr, len, prot, flags, fd, pgoff);
}

static inline long
munmap_syscall(byte *addr, size_t len)
{
    return dynamorio_syscall(SYS_munmap, 2, addr, len);
}

#ifndef NOT_DYNAMORIO_CORE_PROPER
/* free memory allocated from os_raw_mem_alloc */
void
os_raw_mem_free(void *p, size_t size, heap_error_code_t *error_code)
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
    ASSERT(rc == 0);
}

/* try to alloc memory at preferred from os directly,
 * caller is required to handle thread synchronization and to update
 */
void *
os_raw_mem_alloc(void *preferred, size_t size, uint prot,
                 heap_error_code_t *error_code)
{
    byte *p;
    uint os_prot = memprot_to_osprot(prot);

    ASSERT(error_code != NULL);
    /* should only be used on aligned pieces */
    ASSERT(size > 0 && ALIGNED(size, PAGE_SIZE));

    p = mmap_syscall(preferred, size, os_prot,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (!mmap_syscall_succeeded(p)) {
        *error_code = -(heap_error_code_t)(ptr_int_t)p;
        LOG(GLOBAL, LOG_HEAP, 3,
            "os_raw_mem_alloc %d bytes failed"PFX"\n", size, p);
        return NULL;
    }
    if (preferred != NULL && p != preferred) {
        *error_code = HEAP_ERROR_NOT_AT_PREFERRED;
        os_raw_mem_free(p, size, error_code);
        LOG(GLOBAL, LOG_HEAP, 3,
            "os_raw_mem_alloc %d bytes failed"PFX"\n", size, p);
        return NULL;
    }
    LOG(GLOBAL, LOG_HEAP, 2, "os_raw_mem_alloc: "SZFMT" bytes @ "PFX"\n",
        size, p);
    return p;
}

/* caller is required to handle thread synchronization and to update dynamo vm areas */
void
os_heap_free(void *p, size_t size, heap_error_code_t *error_code)
{
    long rc;
    ASSERT(error_code != NULL);
    if (!dynamo_exited)
        LOG(GLOBAL, LOG_HEAP, 4, "os_heap_free: %d bytes @ "PFX"\n", size, p);
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
    ASSERT(!os_in_vmkernel_userworld() ||
           !executable || preferred == NULL ||
           ((byte *)preferred >= os_vmk_mmap_text_start() &&
            ((byte *)preferred)+size <= os_vmk_mmap_text_end()));
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

    /* FIXME: note that this memory is in fact still committed - see man mmap */
    /* FIXME: case 2347 on Linux or -vm_reserve should be set to false */
    /* FIXME: Need to actually get a mmap-ing with |MAP_NORESERVE */
    p = mmap_syscall(preferred, size, prot, MAP_PRIVATE|MAP_ANONYMOUS
                     IF_X64(| (DYNAMO_OPTION(heap_in_lower_4GB) ?
                               MAP_32BIT : 0)),
                     -1, 0);
    if (!mmap_syscall_succeeded(p)) {
        *error_code = -(heap_error_code_t)(ptr_int_t)p;
        LOG(GLOBAL, LOG_HEAP, 4,
            "os_heap_reserve %d bytes failed "PFX"\n", size, p);
        return NULL;
    } else if (preferred != NULL && p != preferred) {
        /* We didn't get the preferred address.  To harmonize with windows behavior and
         * give greater control we fail the reservation. */
        heap_error_code_t dummy;
        *error_code = HEAP_ERROR_NOT_AT_PREFERRED;
        os_heap_free(p, size, &dummy);
        ASSERT(dummy == HEAP_ERROR_SUCCESS);
        LOG(GLOBAL, LOG_HEAP, 4,
            "os_heap_reserve %d bytes at "PFX" not preferred "PFX"\n",
            size, preferred, p);
        return NULL;
    } else {
        *error_code = HEAP_ERROR_SUCCESS;
    }
    LOG(GLOBAL, LOG_HEAP, 2, "os_heap_reserve: %d bytes @ "PFX"\n", size, p);
#ifdef VMX86_SERVER
    /* PR 365331: ensure our memory is all in the mmap_text region */
    ASSERT(!os_in_vmkernel_userworld() || !executable ||
           ((byte *)p >= os_vmk_mmap_text_start() &&
            ((byte *)p) + size <= os_vmk_mmap_text_end()));
#endif
    return p;
}

void *
os_heap_reserve_in_region(void *start, void *end, size_t size,
                          heap_error_code_t *error_code, bool executable)
{
    byte *p = NULL;
    byte *try_start;

    LOG(GLOBAL, LOG_HEAP, 3,
        "os_heap_reserve_in_region: "SZFMT" bytes in "PFX"-"PFX"\n", size, start, end);

    /* if no restriction on location use regular os_heap_reserve() */
    if (start == (void *)PTR_UINT_0 && end == (void *)POINTER_MAX)
        return os_heap_reserve(NULL, size, error_code, executable);

    /* FIXME: be smarter and use a memory query instead of trying every single page 
     * FIXME: if result is not at preferred it may still be in range: so
     * make syscall ourselves and avoid this loop.
     */
    for (try_start = (byte *) start; try_start < (byte *)end - size;
         try_start += PAGE_SIZE) {
        p = os_heap_reserve(try_start, size, error_code, executable);
        if (*error_code == HEAP_ERROR_SUCCESS && p != NULL &&
            p >= (byte *)start && p + size <= (byte *)end)
            break;
    }
    if (p == NULL)
        *error_code = HEAP_ERROR_CANT_RESERVE_IN_REGION;
    else
        *error_code = HEAP_ERROR_SUCCESS;

    LOG(GLOBAL, LOG_HEAP, 2,
        "os_heap_reserve_in_region: reserved "SZFMT" bytes @ "PFX" in "PFX"-"PFX"\n",
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

    LOG(GLOBAL, LOG_HEAP, 2, "os_heap_commit: %d bytes @ "PFX"\n", size, p);
    return true;
}

/* caller is required to handle thread synchronization and to update dynamo vm areas */
void
os_heap_decommit(void *p, size_t size, heap_error_code_t *error_code)
{
    int rc;
    ASSERT(error_code != NULL);

    if (!dynamo_exited)
        LOG(GLOBAL, LOG_HEAP, 4, "os_heap_decommit: %d bytes @ "PFX"\n", size, p);

    *error_code = HEAP_ERROR_SUCCESS;
    /* FIXME: for now do nothing since os_heap_reserve has in fact committed the memory */
    rc = 0;
    /* TODO: 
           p = mmap_syscall(p, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
       we should either do a mremap() 
       or we can do a munmap() followed 'quickly' by a mmap() - 
       also see above the comment that os_heap_reserve() in fact is not so lightweight
    */
    ASSERT(rc == 0);    
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

/* Waits on the futex until woken if the kernel supports SYS_futex syscall
 * and the futex's value has not been changed from mustbe. Does not block
 * if the kernel doesn't support SYS_futex. Returns 0 if woken by another thread, 
 * and negative value for all other cases.
 */
ptr_int_t
futex_wait(volatile int *futex, int mustbe)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        /* XXX: Having debug timeout like win32 os_wait_event() would be useful */
        res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAIT, mustbe, NULL,
                                NULL, 0);
    } else {
        res = -1;
    }
    return res;
}

/* Wakes up at most one thread waiting on the futex if the kernel supports
 * SYS_futex syscall. Does nothing if the kernel doesn't support SYS_futex.
 */
ptr_int_t
futex_wake(volatile int *futex)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAKE, 1, NULL, NULL, 0);
    } else {
        res = -1;
    }
    return res;
}

/* Wakes up all the threads waiting on the futex if the kernel supports
 * SYS_futex syscall. Does nothing if the kernel doesn't support SYS_futex.
 */
ptr_int_t
futex_wake_all(volatile int *futex)
{
    ptr_int_t res;
    ASSERT(ALIGNED(futex, sizeof(int)));
    if (kernel_futex_support) {
        do {
            res = dynamorio_syscall(SYS_futex, 6, futex, FUTEX_WAKE, INT_MAX,
                                    NULL, NULL, 0);
        } while (res == INT_MAX);
        res = 0;
    } else {
        res = -1;
    }
    return res;
}

/* yield the current thread */
void
thread_yield()
{
    dynamorio_syscall(SYS_sched_yield, 0);
}

static bool
thread_signal(process_id_t pid, thread_id_t tid, int signum)
{
    /* FIXME: for non-NPTL use SYS_kill */
    /* Note that the pid is equivalent to the thread group id.
     * However, we can have threads sharing address space but not pid
     * (if created via CLONE_VM but not CLONE_THREAD), so make sure to
     * use the pid of the target thread, not our pid.
     */
    return (dynamorio_syscall(SYS_tgkill, 3, pid, tid, signum) == 0);
}

void
thread_sleep(uint64 milliseconds)
{
    struct timespec req;
    struct timespec remain;
    int count = 0;
    req.tv_sec = (milliseconds / 1000);
    /* docs say can go up to 1000000000, but doesn't work on FC9 */
    req.tv_nsec = (milliseconds % 1000) * 1000000;
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
}

bool
thread_suspend(thread_record_t *tr)
{
    os_thread_data_t *ostd = (os_thread_data_t *) tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    /* See synch comments in thread_resume: the mutex held there
     * prevents prematurely sending a re-suspend signal.
     */
    mutex_lock(&ostd->suspend_lock);
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
        ASSERT(ostd->suspended == 0);
        if (!thread_signal(tr->pid, tr->id, SUSPEND_SIGNAL)) {
            ostd->suspend_count--;
            mutex_unlock(&ostd->suspend_lock);
            return false;
        }
    }
    /* we can unlock before the wait loop b/c we're using a separate "resumed"
     * int and thread_resume holds the lock across its wait.  this way a resume
     * can proceed as soon as the suspended thread is suspended, before the
     * suspending thread gets scheduled again.
     */
    mutex_unlock(&ostd->suspend_lock);
    /* i#96/PR 295561: use futex(2) if available */
    while (ostd->suspended == 0) {
        /* Waits only if the suspended flag is not set as 1. Return value
         * doesn't matter because the flag will be re-checked. 
         */
        futex_wait(&ostd->suspended, 0);
        if (ostd->suspended == 0) {
            /* If it still has to wait, give up the cpu. */
            thread_yield();
        }
    }
    return true;
}

bool
thread_resume(thread_record_t *tr)
{
    os_thread_data_t *ostd = (os_thread_data_t *) tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    /* This mutex prevents sending a re-suspend signal before the target
     * reaches a safe post-resume point from a first suspend signal.
     * Given that race, we can't just use atomic_add_exchange_int +
     * atomic_dec_becomes_zero on suspend_count.
     */
    mutex_lock(&ostd->suspend_lock);
    ASSERT(ostd->suspend_count > 0);
    /* PR 479750: if do get here and target is not suspended then abort
     * to avoid possible deadlocks
     */
    if (ostd->suspend_count == 0) {
        mutex_unlock(&ostd->suspend_lock);
        return true; /* the thread is "resumed", so success status */
    }
    ostd->suspend_count--;
    if (ostd->suspend_count > 0) {
        mutex_unlock(&ostd->suspend_lock);
        return true; /* still suspended */
    }
    ostd->wakeup = 1;
    futex_wake(&ostd->wakeup);
    /* i#96/PR 295561: use futex(2) if available */
    while (ostd->resumed == 0) {
        /* Waits only if the resumed flag is not set as 1. Return value
         * doesn't matter because the flag will be re-checked. 
         */
        futex_wait(&ostd->resumed, 0);
        if (ostd->resumed == 0) {
            /* If it still has to wait, give up the cpu. */
            thread_yield();
        }
    }
    ostd->wakeup = 0;
    ostd->resumed = 0;
    mutex_unlock(&ostd->suspend_lock);
    return true;
}

bool
thread_terminate(thread_record_t *tr)
{
    /* PR 297902: for NPTL sending SIGKILL will take down the whole group:
     * so instead we send SIGUSR2 and have a flag set telling
     * target thread to execute SYS_exit
     */
    os_thread_data_t *ostd = (os_thread_data_t *) tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ostd->terminate = true;
    return thread_signal(tr->pid, tr->id, SUSPEND_SIGNAL);
}

bool
is_thread_terminated(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
    ASSERT(ostd != NULL);
    return (ostd->terminated == 1);
}

void
os_wait_thread_terminated(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
    ASSERT(ostd != NULL);
    while (ostd->terminated == 0) {
        /* Waits only if the terminated flag is not set as 1. Return value
         * doesn't matter because the flag will be re-checked. 
         */
        futex_wait(&ostd->terminated, 0);
        if (ostd->terminated == 0) {
            /* If it still has to wait, give up the cpu. */
            thread_yield();
        }
    }
}

bool
thread_get_mcontext(thread_record_t *tr, priv_mcontext_t *mc)
{
    /* PR 212090: only works when target is suspended by us, and
     * we then take the signal context
     */
    os_thread_data_t *ostd = (os_thread_data_t *) tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ASSERT(ostd->suspend_count > 0);
    if (ostd->suspend_count == 0)
        return false;
    ASSERT(ostd->suspended_sigcxt != NULL);
    sigcontext_to_mcontext(mc, ostd->suspended_sigcxt);
    return true;
}

bool
thread_set_mcontext(thread_record_t *tr, priv_mcontext_t *mc)
{
    /* PR 212090: only works when target is suspended by us, and
     * we then replace the signal context
     */
    os_thread_data_t *ostd = (os_thread_data_t *) tr->dcontext->os_field;
    ASSERT(ostd != NULL);
    ASSERT(ostd->suspend_count > 0);
    if (ostd->suspend_count == 0)
        return false;
    ASSERT(ostd->suspended_sigcxt != NULL);
    mcontext_to_sigcontext(ostd->suspended_sigcxt, mc);
    return true;
}

bool
is_thread_currently_native(thread_record_t *tr)
{
    return (!tr->under_dynamo_control);
}

#ifdef CLIENT_SIDELINE /* PR 222812: tied to sideline usage */
static void
client_thread_run(void)
{
    void (*func)(void *param);
    dcontext_t *dcontext;
    byte *xsp;
    GET_STACK_PTR(xsp);
    void *crec = get_clone_record((reg_t)xsp);
    IF_DEBUG(int rc = )
        dynamo_thread_init(get_clone_record_dstack(crec), NULL, true);
    ASSERT(rc != -1); /* this better be a new thread */
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    LOG(THREAD, LOG_ALL, 1, "\n***** CLIENT THREAD %d *****\n\n",
        get_thread_id());
    /* We stored the func and args in particular clone record fields */
    func = (void (*)(void *param)) signal_thread_inherit(dcontext, crec);
    void *arg = (void *) get_clone_record_app_xsp(crec);
    LOG(THREAD, LOG_ALL, 1, "func="PFX", arg="PFX"\n", func, arg);

    (*func)(arg);

    LOG(THREAD, LOG_ALL, 1, "\n***** CLIENT THREAD %d EXITING *****\n\n",
        get_thread_id());
    cleanup_and_terminate(dcontext, SYS_exit, 0, 0, false/*just thread*/);
}

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
    dcontext_t *dcontext = get_thread_private_dcontext();
    byte *xsp;
    /* We do not pass SIGCHLD since don't want signal to parent and don't support
     * waiting on child.
     * We do not pass CLONE_THREAD so that the new thread is in its own thread
     * group, allowing it to have private itimers and not receive any signals
     * sent to the app's thread groups.  It also makes the thread not show up in
     * the thread list for the app, making it more invisible.
     */
    uint flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
        IF_NOT_X64(| CLONE_SETTLS)
        /* CLONE_THREAD required.  Signals and itimers are private anyway. */
        IF_VMX86(| (os_in_vmkernel_userworld() ? CLONE_THREAD : 0));
    pre_second_thread();
    /* need to share signal handler table, prior to creating clone record */
    handle_clone(dcontext, flags);
    void *crec = create_clone_record(dcontext, (reg_t*)&xsp);
    /* make sure client_thread_run can get the func and arg, and that
     * signal_thread_inherit gets the right syscall info
     */
    set_clone_record_fields(crec, (reg_t) arg, (app_pc) func, SYS_clone, flags);
    /* i#501 switch to app's tls before creating client thread */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false))
        os_switch_lib_tls(dcontext, true/*to app*/);
#ifndef X64
    /* For the TCB we simply share the parent's.  On Linux we could just inherit
     * the same selector but not for VMX86_SERVER so we specify for both for
     * 32-bit.  Most of the fields are pthreads-specific and we assume the ones
     * that will be used (such as tcbhead_t.sysinfo @0x10) are read-only.
     */
    our_modify_ldt_t desc;
    /* if get_segment_base() returned size too we could use it */
    uint index = lib_tls_gdt_index;
    ASSERT(lib_tls_gdt_index != -1);
    initialize_ldt_struct(&desc, NULL, 0, index);
    int res = dynamorio_syscall(SYS_get_thread_area, 1, &desc);
    if (res != 0) {
        LOG(THREAD, LOG_ALL, 1,
            "%s: client thread tls get entry %d failed: %d\n",
            __FUNCTION__, index, res);
        return false;
    }
#endif
    LOG(THREAD, LOG_ALL, 1, "dr_create_client_thread xsp="PFX" dstack="PFX"\n",
        xsp, get_clone_record_dstack(crec));
    thread_id_t newpid = dynamorio_clone(flags, xsp, NULL, IF_X64_ELSE(NULL, &desc),
                                         NULL, client_thread_run);
    /* i#501 switch to app's tls before creating client thread */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false))
        os_switch_lib_tls(dcontext, false/*to dr*/);
    if (newpid < 0) {
        LOG(THREAD, LOG_ALL, 1, "client thread creation failed: %d\n", newpid);
        return false;
    } else if (newpid == 0) {
        /* dynamorio_clone() should have called client_thread_run directly */
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}
#endif /* CLIENT_SIDELINE PR 222812: tied to sideline usage */

int
get_num_processors(void)
{
    static uint num_cpu = 0;         /* cached value */
    if (!num_cpu) {
        /* We used to use get_nprocs_conf, but that's in libc, so now we just
         * look at the /sys filesystem ourselves, which is what glibc does.
         */
        uint local_num_cpus = 0;
        file_t cpu_dir = os_open_directory("/sys/devices/system/cpu",
                                           OS_OPEN_READ);
        dir_iterator_t iter;
        os_dir_iterator_start(&iter, cpu_dir);
        while (os_dir_iterator_next(&iter)) {
            int dummy_num;
            if (sscanf(iter.name, "cpu%d", &dummy_num) == 1)
                local_num_cpus++;
        }
        os_close(cpu_dir);
        num_cpu = local_num_cpus;
        ASSERT(num_cpu);
    }
    return num_cpu;
}

/* i#46: To support -no_private_loader, we have to call the dlfcn family of
 * routines in libdl.so.  When we do early injection, there is no loader to
 * resolve these imports, so they will crash.  Early injection is incompatible
 * with -no_private_loader, so this should never happen.
 */

#if defined(CLIENT_INTERFACE) || defined(HOT_PATCHING_INTERFACE)
shlib_handle_t 
load_shared_library(const char *name)
{
# ifdef STATIC_LIBRARY
    if (os_files_same(name, get_application_name())) {
        /* The private loader falls back to dlsym() and friends for modules it
         * does't recognize, so this works without disabling the private loader.
         */
        return dlopen(NULL, RTLD_LAZY);  /* Gets a handle to the exe. */
    }
# endif
    /* We call locate_and_load_private_library() to support searching for
     * a pathless name.
     */
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false))
        return (shlib_handle_t) locate_and_load_private_library(name);
    ASSERT(!DYNAMO_OPTION(early_inject));
    return dlopen(name, RTLD_LAZY);
}
#endif

#if defined(CLIENT_INTERFACE)
shlib_routine_ptr_t
lookup_library_routine(shlib_handle_t lib, const char *name)
{
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
        return (shlib_routine_ptr_t)
            get_private_library_address((app_pc)lib, name);
    }
    ASSERT(!DYNAMO_OPTION(early_inject));
    return dlsym(lib, name);
}

void
unload_shared_library(shlib_handle_t lib)
{
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
        unload_private_library(lib);
    } else {
        ASSERT(!DYNAMO_OPTION(early_inject));
        if (!DYNAMO_OPTION(avoid_dlclose)) {
            dlclose(lib);
        }
    }
}

void
shared_library_error(char *buf, int maxlen)
{
    const char *err;
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
        err = "error in private loader";
    } else {
        ASSERT(!DYNAMO_OPTION(early_inject));
        err = dlerror();
        if (err == NULL) {
            err = "dlerror returned NULL";
        }
    }
    strncpy(buf, err, maxlen-1);
    buf[maxlen-1] = '\0'; /* strncpy won't put on trailing null if maxes out */
}

/* addr is any pointer known to lie within the library.
 * for linux, one of addr or name is needed; for windows, neither is needed.
 */
bool
shared_library_bounds(IN shlib_handle_t lib, IN byte *addr,
                      IN const char *name,
                      OUT byte **start, OUT byte **end)
{
    ASSERT(start != NULL && end != NULL);
    /* PR 366195: dlopen() handle truly is opaque, so we have to use either
     * addr or name
     */
    ASSERT(addr != NULL || name != NULL);
    *start = addr;
    if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
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
    return (get_library_bounds(name, start, end, NULL, 0) > 0);
}
#endif /* defined(CLIENT_INTERFACE) */

#endif /* !NOT_DYNAMORIO_CORE_PROPER: around most of file, to exclude preload */

/* FIXME - not available in 2.0 or earlier kernels, not really an issue since no one
 * should be running anything that old. */
int
llseek_syscall(int fd, int64 offset, int origin, int64 *result)
{
#ifdef X64
    *result = dynamorio_syscall(SYS_lseek, 3, fd, offset, origin);
    return ((*result > 0) ? 0 : (int)*result);
#else
    return dynamorio_syscall(SYS__llseek, 5, fd, (uint)((offset >> 32) & 0xFFFFFFFF),
                             (uint)(offset & 0xFFFFFFFF), result, origin); 
#endif
}

bool
os_file_exists(const char *fname, bool is_dir)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dynamorio_syscall(SYSNUM_STAT, 2, fname, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    return (!is_dir || S_ISDIR(st.st_mode));
}

/* Returns true if two paths point to the same file.  Follows symlinks.
 */
bool
os_files_same(const char *path1, const char *path2)
{
    struct stat64 st1, st2;
    ptr_int_t res = dynamorio_syscall(SYSNUM_STAT, 2, path1, &st1);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    res = dynamorio_syscall(SYSNUM_STAT, 2, path2, &st2);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    return st1.st_ino == st2.st_ino;
}

bool
os_get_file_size(const char *file, uint64 *size)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dynamorio_syscall(SYSNUM_STAT, 2, file, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    ASSERT(size != NULL);
    *size = st.st_size;
    return true;
}

bool
os_get_file_size_by_handle(file_t fd, uint64 *size)
{
    /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
    struct stat64 st;
    ptr_int_t res = dynamorio_syscall(SYSNUM_FSTAT, 2, fd, &st);
    if (res != 0) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    ASSERT(size != NULL);
    *size = st.st_size;
    return true;
}

/* created directory will be owned by effective uid,
 * Note a symbolic link will never be followed.
 */
bool
os_create_dir(const char *fname, create_directory_flags_t create_dir_flags)
{
    bool require_new = TEST(CREATE_DIR_REQUIRE_NEW, create_dir_flags);
    int rc = dynamorio_syscall(SYS_mkdir, 2, fname, S_IRWXU|S_IRWXG);
    ASSERT(create_dir_flags == CREATE_DIR_REQUIRE_NEW || 
           create_dir_flags == CREATE_DIR_ALLOW_EXISTING);
    return (rc == 0 || (!require_new && rc == -EEXIST));
}

int
open_syscall(const char *file, int flags, int mode)
{
    ASSERT(file != NULL);
    return dynamorio_syscall(SYS_open, 3, file, flags, mode);
}

int
close_syscall(int fd)
{
    return dynamorio_syscall(SYS_close, 1, fd);
}

int
dup_syscall(int fd)
{
    return dynamorio_syscall(SYS_dup, 1, fd);
}

ssize_t
read_syscall(int fd, void *buf, size_t nbytes)
{
    return dynamorio_syscall(SYS_read, 3, fd, buf, nbytes);
}

ssize_t
write_syscall(int fd, const void *buf, size_t nbytes)
{
    return dynamorio_syscall(SYS_write, 3, fd, buf, nbytes);
}

#ifndef NOT_DYNAMORIO_CORE_PROPER
static int
fcntl_syscall(int fd, int cmd, long arg)
{
    return dynamorio_syscall(SYS_fcntl, 3, fd, cmd, arg);
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* not easily accessible in header files */
#ifdef X64
/* not needed */
# define O_LARGEFILE    0
#else
# define O_LARGEFILE    0100000
#endif

/* we assume that opening for writing wants to create file.
 * we also assume that nobody calling this is creating a persistent
 * file: for that, use os_open_protected() to avoid leaking on exec
 * and to separate from the app's files.
 */
file_t
os_open(const char *fname, int os_open_flags)
{
    int res;
    int flags = 0;
    if (TEST(OS_OPEN_ALLOW_LARGE, os_open_flags))
        flags |= O_LARGEFILE;
    if (!TEST(OS_OPEN_WRITE, os_open_flags)) 
        res = open_syscall(fname, flags|O_RDONLY, 0);
    else {    
        res = open_syscall(fname, flags|O_RDWR|O_CREAT|
                           (TEST(OS_OPEN_APPEND, os_open_flags) ? 
                            /* Currently we only support either appending
                             * or truncating, just like Windows and the client
                             * interface.  If we end up w/ a use case that wants
                             * neither it could open append and then seek; if we do
                             * add OS_TRUNCATE or sthg we'll need to add it to
                             * any current writers who don't set OS_OPEN_REQUIRE_NEW.
                             */
                            O_APPEND : O_TRUNC) |
                           (TEST(OS_OPEN_REQUIRE_NEW, os_open_flags) ? 
                            O_EXCL : 0), 
                           S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    }
    if (res < 0)
        return INVALID_FILE;

    return res;
}

file_t
os_open_directory(const char *fname, int os_open_flags)
{
    /* no special handling */
    return os_open(fname, os_open_flags);
}

void
os_close(file_t f)
{
    close_syscall(f);
}

#ifndef NOT_DYNAMORIO_CORE_PROPER
/* dups curfd to a private fd.
 * returns -1 if unsuccessful.
 */
static file_t
fd_priv_dup(file_t curfd)
{
    file_t newfd = -1;
    if (DYNAMO_OPTION(steal_fds) > 0) {
        /* RLIMIT_NOFILES is 1 greater than max and F_DUPFD starts at given value */
        /* XXX: if > linux 2.6.24, can use F_DUPFD_CLOEXEC to avoid later call:
         * so how do we tell if the flag is supported?  try calling once at init?
         */
        newfd = fcntl_syscall(curfd, F_DUPFD, app_rlimit_nofile.rlim_cur);
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
            newfd = fcntl_syscall(curfd, F_DUPFD, app_rlimit_nofile.rlim_cur/2);
        }
    }
    return newfd;
}

static bool
fd_mark_close_on_exec(file_t fd)
{
    /* we assume FD_CLOEXEC is the only flag and don't bother w/ F_GETFD */
    if (fcntl_syscall(fd, F_SETFD, FD_CLOEXEC) != 0) {
        SYSLOG_INTERNAL_WARNING("unable to mark file %d as close-on-exec", fd);
        return false;
    }
    return true;
}

static void
fd_table_add(file_t fd, uint flags)
{
    if (fd_table != NULL) {
        TABLE_RWLOCK(fd_table, write, lock);
        DODEBUG({
            /* i#1010: If the fd is already in the table, chances are it's a
             * stale logfile fd left behind by a vforked or cloned child that
             * called execve.  Avoid an assert if that happens.
             */
            bool present = generic_hash_remove(GLOBAL_DCONTEXT, fd_table,
                                               (ptr_uint_t)fd);
            ASSERT_CURIOSITY_ONCE(!present && "stale fd not cleaned up");
        });
        generic_hash_add(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd,
                         /* store the flags, w/ a set bit to ensure not 0 */
                         (void *)(ptr_uint_t)(flags|OS_OPEN_RESERVED));
        TABLE_RWLOCK(fd_table, write, unlock);
    } else {
#ifdef DEBUG
        static int num_pre_heap;
        num_pre_heap++;
        /* we add main_logfile in os_init() */
        ASSERT(num_pre_heap == 1 && "only main_logfile should come here");
#endif
    }
}

static bool
fd_is_dr_owned(file_t fd)
{
    ptr_uint_t flags;
    ASSERT(fd_table != NULL);
    TABLE_RWLOCK(fd_table, read, lock);
    flags = (ptr_uint_t) generic_hash_lookup(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)fd);
    TABLE_RWLOCK(fd_table, read, unlock);
    return (flags != 0);
}

static bool
fd_is_in_private_range(file_t fd)
{
    return (DYNAMO_OPTION(steal_fds) > 0 &&
            app_rlimit_nofile.rlim_cur > 0 &&
            fd >= app_rlimit_nofile.rlim_cur);
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
    ASSERT(fd_table != NULL || dynamo_exited);
    if (fd_table != NULL) {
        TABLE_RWLOCK(fd_table, write, lock);
        generic_hash_remove(GLOBAL_DCONTEXT, fd_table, (ptr_uint_t)f);
        TABLE_RWLOCK(fd_table, write, unlock);
    }
    os_close(f);
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

#ifndef NOT_DYNAMORIO_CORE_PROPER /* so drinject can use drdecode's copy */
ssize_t
os_write(file_t f, const void *buf, size_t count)
{
    return write_syscall(f, buf, count);
}
#endif /* !NOT_DYNAMORIO_CORE_PROPER */

ssize_t 
os_read(file_t f, void *buf, size_t count)
{
    return read_syscall(f, buf, count);
}

void
os_flush(file_t f)
{
    /* we're not using FILE*, so there is no buffering */
}

/* seek the current file position to offset bytes from origin, return true if successful */
bool
os_seek(file_t f, int64 offset, int origin)
{
    int64 result;
    int ret = 0;

    ret = llseek_syscall(f, offset, origin, &result);

    return (ret == 0);
}

/* return the current file position, -1 on failure */
int64
os_tell(file_t f)
{
    int64 result = -1;
    int ret = 0;

    ret = llseek_syscall(f, 0, SEEK_CUR, &result);

    if (ret != 0)
        return -1;

    return result;
}

bool
os_delete_file(const char *name)
{
    return (dynamorio_syscall(SYS_unlink, 1, name) == 0);
}

bool
os_rename_file(const char *orig_name, const char *new_name, bool replace)
{
    ptr_int_t res;
    if (!replace) {
        /* SYS_rename replaces so we must test beforehand => could have race */
        /* _LARGEFILE64_SOURCE should make libc struct match kernel (see top of file) */
        struct stat64 st;
        ptr_int_t res = dynamorio_syscall(SYSNUM_STAT, 2, new_name, &st);
        if (res == 0)
            return false;
        else if (res != -ENOENT) {
            LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s stat failed: "PIFX"\n", __func__, res);
            return false;
        }
    }
    res = dynamorio_syscall(SYS_rename, 2, orig_name, new_name);
    if (res != 0)
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s \"%s\" to \"%s\" failed: "PIFX"\n",
            __func__, orig_name, new_name, res);
    return (res == 0);
}

bool
os_delete_mapped_file(const char *filename)
{
    return os_delete_file(filename);
}

byte *
os_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot,
            bool copy_on_write, bool image, bool fixed)
{
    int flags;
#ifndef X64
    uint pg_offs;
    ASSERT_TRUNCATE(pg_offs, uint, offs / PAGE_SIZE);
    pg_offs = (uint) (offs / PAGE_SIZE);
#endif
#ifdef VMX86_SERVER
    flags = MAP_PRIVATE; /* MAP_SHARED not supported yet */
#else
    flags = copy_on_write ? MAP_PRIVATE : MAP_SHARED;
#endif
#ifdef X64
    /* allocate memory from reachable range for image: or anything (pcache
     * in particular)
     */
    if (!fixed)
        flags |= MAP_32BIT;
#endif
    /* Allows memory request instead of mapping a file, 
     * so we can request memory from a particular address with fixed argument */
    if (f == -1)
        flags |= MAP_ANONYMOUS;
    if (fixed)
        flags |= MAP_FIXED;
    byte *map = mmap_syscall(addr, *size, memprot_to_osprot(prot),
                             flags, f,
                             /* mmap in X64 uses offset instead of page offset */
                             IF_X64_ELSE(offs, pg_offs));
    if (!mmap_syscall_succeeded(map)) {
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n",
            __func__, map);
        map = NULL;
    }
    return map;
}

bool
os_unmap_file(byte *map, size_t size)
{
    long res = munmap_syscall(map, size);
    return (res == 0);
}

/* around most of file, to exclude preload */
#if !defined(NOT_DYNAMORIO_CORE_PROPER) || defined(STANDALONE_UNIT_TEST)

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
        LOG(THREAD_GET, LOG_SYSCALLS, 2, "%s failed: "PIFX"\n", __func__, res);
        return false;
    }
    LOG(GLOBAL, LOG_STATS, 3,
        "os_get_disk_free_space: avail="SZFMT", free="SZFMT", bsize="SZFMT"\n",
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
    dynamorio_syscall(SYS_exit_group, 1, status);
    /* would assert that result is -ENOSYS but assert likely calls us => infinite loop */
    exit_thread_syscall(status);
    ASSERT_NOT_REACHED();
}

void
exit_thread_syscall(long status)
{
    dynamorio_syscall(SYS_exit, 1, status);
}

/* FIXME: this one will not be easily internationalizable 
   yet it is easier to have a syslog based Unix implementation with real strings.
 */

void 
os_syslog(syslog_event_type_t priority, uint message_id, 
          uint substitutions_num, va_list args) {
    int native_priority;
    switch (priority) {
    case SYSLOG_INFORMATION: native_priority = LOG_INFO; break;
    case SYSLOG_WARNING:     native_priority = LOG_WARNING; break;
    case SYSLOG_CRITICAL:    native_priority = LOG_CRIT; break;
    case SYSLOG_ERROR:       native_priority = LOG_ERR; break;
    default:
        ASSERT_NOT_REACHED();
    }
    /* can amount to passing a format string (careful here) to vsyslog */

    /* Never let user controlled data in the format string! */
    ASSERT_NOT_IMPLEMENTED(false);
}

static bool
all_memory_areas_initialized(void)
{
    return (all_memory_areas != NULL && !vmvector_empty(all_memory_areas) &&
            /* not really set until vm_areas_init() */
            dynamo_initialized);
}

#if defined(DEBUG) && defined(INTERNAL)
static void
print_all_memory_areas(file_t outf)
{
    vmvector_iterator_t vmvi;
    if (all_memory_areas == NULL || vmvector_empty(all_memory_areas))
        return;

    vmvector_iterator_start(all_memory_areas, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        app_pc start, end;
        void *nxt = vmvector_iterator_next(&vmvi, &start, &end);
        allmem_info_t *info = (allmem_info_t *) nxt;
        print_file(outf, PFX"-"PFX" prot=%s type=%s\n", start, end,
                   memprot_string(info->prot),
                   (info->type == DR_MEMTYPE_FREE ? "free" :
                    (info->type == DR_MEMTYPE_IMAGE ? "image" : "data")));
    }
    vmvector_iterator_stop(&vmvi);
}
#endif

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
    if (all_memory_areas_initialized())
        res = is_readable_without_exception_internal(base, size, false/*use allmem*/);
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
     * We do set up an early signal handler in os_init(),
     * but there is still be a window prior to that with no handler.
     */
    if (!fault_handling_initialized) {
        return safe_read_via_query(base, size, out_buf, bytes_read);
    } else {
        return safe_read_fast(base, size, out_buf, bytes_read);
    }
}

/* FIXME - fold this together with safe_read_ex() (is a lot of places to update) */
bool
safe_read(const void *base, size_t size, void *out_buf)
{
    return safe_read_ex(base, size, out_buf, NULL);
}

bool
safe_write_ex(void *base, size_t size, const void *in_buf, size_t *bytes_written)
{
    uint prot;
    byte *region_base;
    size_t region_size;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool res = false;
    if (bytes_written != NULL)
        *bytes_written = 0;

    if (dcontext != NULL) {
        TRY_EXCEPT(dcontext, {
            /* We abort on the 1st fault, just like safe_read */
            memcpy(base, in_buf, size);
            res = true;
         } , { /* EXCEPT */
            /* nothing: res is already false */
         });
    } else {
        /* this is subject to races, but should only happen at init/attach when
         * there should only be one live thread.
         */
        /* on x86 must be readable to be writable so start with that */
        if (is_readable_without_exception(base, size) &&
            get_memory_info_from_os(base, &region_base, &region_size, &prot) &&
            TEST(MEMPROT_WRITE, prot)) {
            size_t bytes_checked = region_size - ((byte *)base - region_base);
            while (bytes_checked < size) {
                if (!get_memory_info_from_os(region_base + region_size, &region_base,
                                             &region_size, &prot) ||
                    !TEST(MEMPROT_WRITE, prot))
                    return false;
                bytes_checked += region_size;
            }
        } else {
            return false;
        }
        /* ok, checks passed do the copy, FIXME - because of races this isn't safe! */
        memcpy(base, in_buf, size);
        res = true;
    }

    if (res) {
        if (bytes_written != NULL)
            *bytes_written = size;
    }
    return res;
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
    byte *check_pc = (byte *) ALIGN_BACKWARD(pc, PAGE_SIZE);
    if (size > ((byte *)POINTER_MAX - pc))
        size = (byte *)POINTER_MAX - pc;
    do {
        bool rc = query_os ?
            get_memory_info_from_os(check_pc, NULL, NULL, &prot) :
            get_memory_info(check_pc, NULL, NULL, &prot);
        if (!rc || !TESTANY(MEMPROT_READ|MEMPROT_EXEC, prot))
            return false;
        check_pc += PAGE_SIZE;
    } while (check_pc != 0/*overflow*/ && check_pc < pc+size);
    return true;
}

bool
is_readable_without_exception(const byte *pc, size_t size)
{
    /* case 9745 / i#853: We've had problems with all_memory_areas not being
     * accurate in the past.  Parsing proc maps is too slow for some apps, so we
     * use a runtime option.
     */
    bool query_os = !DYNAMO_OPTION(use_all_memory_areas);
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
is_user_address(byte *pc)
{
    /* FIXME: NYI */
    /* note returning true will always skip the case 9022 logic on Linux */
    return true;
}

#endif /* !NOT_DYNAMORIO_CORE_PROPER */

/* change protections on memory region starting at pc of length length 
 * this does not update the all memory area info 
 */
bool 
os_set_protection(byte *pc, size_t length, uint prot/*MEMPROT_*/)
{
    app_pc start_page = (app_pc) PAGE_START(pc);
    uint num_bytes = ALIGN_FORWARD(length + (pc - start_page), PAGE_SIZE);
    long res = 0;
    uint flags = memprot_to_osprot(prot);
#ifdef IA32_ON_IA64
    LOG(THREAD_GET, LOG_VMAREAS, 1, "protection change not supported on IA64\n"); 
    LOG(THREAD_GET, LOG_VMAREAS, 1, " attempted change_prot("PFX", "PIFX", %s) => "
        "mprotect("PFX", "PIFX")==%d pages\n",
        pc, length, memprot_string(prot), start_page, num_bytes,
        num_bytes / PAGE_SIZE);
#else
    DOSTATS({
        /* once on each side of prot, to get on right side of writability */
        if (!TEST(PROT_WRITE, flags)) {
            STATS_INC(protection_change_calls);
            STATS_ADD(protection_change_pages, num_bytes / PAGE_SIZE);
        }
    });
    res = mprotect_syscall((void *) start_page, num_bytes, flags);
    if (res != 0) 
        return false;
    LOG(THREAD_GET, LOG_VMAREAS, 3, "change_prot("PFX", "PIFX", %s) => "
        "mprotect("PFX", "PIFX", %d)==%d pages\n",
        pc, length, memprot_string(prot), start_page, num_bytes, flags,
        num_bytes / PAGE_SIZE);
#endif
    DOSTATS({
        /* once on each side of prot, to get on right side of writability */
        if (TEST(PROT_WRITE, flags)) {
            STATS_INC(protection_change_calls);
            STATS_ADD(protection_change_pages, num_bytes / PAGE_SIZE);
        }
    });
    return true;
}

#ifndef NOT_DYNAMORIO_CORE_PROPER

/* change protections on memory region starting at pc of length length */
bool
set_protection(byte *pc, size_t length, uint prot/*MEMPROT_*/)
{
    app_pc start_page = (app_pc) PAGE_START(pc);
    uint num_bytes = ALIGN_FORWARD(length + (pc - start_page), PAGE_SIZE);

    if (os_set_protection(pc, length, prot) == false)
        return false;
    all_memory_areas_lock();
    ASSERT(vmvector_overlap(all_memory_areas, start_page, start_page + num_bytes) ||
           /* we could synch up: instead we relax the assert if DR areas not in allmem */
           are_dynamo_vm_areas_stale());
    LOG(GLOBAL, LOG_VMAREAS, 3, "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
        start_page, start_page + num_bytes, prot);
    update_all_memory_areas(start_page, start_page + num_bytes,
                            prot, -1/*type unchanged*/);
    all_memory_areas_unlock();
    return true;
}

/* change protections on memory region starting at pc of length length */
bool
change_protection(byte *pc, size_t length, bool writable)
{
    uint flags = (writable) ? (MEMPROT_READ|MEMPROT_WRITE) : (MEMPROT_READ);
    return set_protection(pc, length, flags);
}

/* make pc's page writable */
bool
make_writable(byte *pc, size_t size)
{
    long res;
    app_pc start_page = (app_pc) PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC|PROT_READ|PROT_WRITE;
    /* if can get current protection then keep old read/exec flags.
     * this is crucial on modern linux kernels which refuse to mark stack +x.
     */
    if (!is_in_dynamo_dll(pc)/*avoid allmem assert*/ &&
#ifdef STATIC_LIBRARY
        /* FIXME i#975: is_in_dynamo_dll() is always false for STATIC_LIBRARY,
         * but we can't call get_memory_info() until allmem is initialized.  Our
         * uses before then are for patching x86.asm, which is OK.
         */
        all_memory_areas_initialized() &&
#endif
        get_memory_info(pc, NULL, NULL, &prot))
        prot |= PROT_WRITE;

    ASSERT(start_page == pc && ALIGN_FORWARD(size, PAGE_SIZE) == size);
#ifdef IA32_ON_IA64
    LOG(THREAD_GET, LOG_VMAREAS, 1, "protection change not supported on IA64\n"); 
    LOG(THREAD_GET, LOG_VMAREAS, 3,
        "attempted make_writable: pc "PFX" -> "PFX"-"PFX"\n",
        pc, start_page, start_page + prot_size);
#else
    res = mprotect_syscall((void *) start_page, prot_size, prot);
    LOG(THREAD_GET, LOG_VMAREAS, 3, "make_writable: pc "PFX" -> "PFX"-"PFX" %d\n",
        pc, start_page, start_page + prot_size, res);
    ASSERT(res == 0);
    if (res != 0) 
        return false;
#endif
    STATS_INC(protection_change_calls);
    STATS_ADD(protection_change_pages, size / PAGE_SIZE);

    if (all_memory_areas_initialized()) {
        /* update all_memory_areas list with the protection change */
        all_memory_areas_lock();
        ASSERT(vmvector_overlap(all_memory_areas, start_page, start_page + prot_size) ||
               /* we could synch up: instead, relax assert if DR areas not in allmem */
               are_dynamo_vm_areas_stale());
        LOG(GLOBAL, LOG_VMAREAS, 3, "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
            start_page, start_page + prot_size, osprot_to_memprot(prot));
        update_all_memory_areas(start_page, start_page + prot_size,
                                osprot_to_memprot(prot), -1/*type unchanged*/);
        all_memory_areas_unlock();
    }
    return true;
}

/* like make_writable but adds COW */
bool make_copy_on_writable(byte *pc, size_t size)
{
    /* FIXME: for current usage this should be fine */
    return make_writable(pc, size);
}

/* make pc's page unwritable */
void
make_unwritable(byte *pc, size_t size)
{
    long res;
    app_pc start_page = (app_pc) PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC|PROT_READ;
    /* if can get current protection then keep old read/exec flags.
     * this is crucial on modern linux kernels which refuse to mark stack +x.
     */
    if (!is_in_dynamo_dll(pc)/*avoid allmem assert*/ &&
#ifdef STATIC_LIBRARY
        /* FIXME i#975: is_in_dynamo_dll() is always false for STATIC_LIBRARY,
         * but we can't call get_memory_info() until allmem is initialized.  Our
         * uses before then are for patching x86.asm, which is OK.
         */
        all_memory_areas_initialized() &&
#endif
        get_memory_info(pc, NULL, NULL, &prot))
        prot &= ~PROT_WRITE;

    ASSERT(start_page == pc && ALIGN_FORWARD(size, PAGE_SIZE) == size);
    /* inc stats before making unwritable, in case messing w/ data segment */
    STATS_INC(protection_change_calls);
    STATS_ADD(protection_change_pages, size / PAGE_SIZE);
#ifdef IA32_ON_IA64
    LOG(THREAD_GET, LOG_VMAREAS, 1, "protection change not supported on IA64\n"); 
    LOG(THREAD_GET, LOG_VMAREAS, 3,
        "attempted make_writable: pc "PFX" -> "PFX"-"PFX"\n",
        pc, start_page, start_page + prot_size);
#else
    res = mprotect_syscall((void *) start_page, prot_size, prot);
    LOG(THREAD_GET, LOG_VMAREAS, 3, "make_unwritable: pc "PFX" -> "PFX"-"PFX"\n",
        pc, start_page, start_page + prot_size);
    ASSERT(res == 0);

    /* update all_memory_areas list with the protection change */
    if (all_memory_areas_initialized()) {
        all_memory_areas_lock();
        ASSERT(vmvector_overlap(all_memory_areas, start_page, start_page + prot_size) ||
               /* we could synch up: instead, relax assert if DR areas not in allmem */
               are_dynamo_vm_areas_stale());
        LOG(GLOBAL, LOG_VMAREAS, 3, "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
            start_page, start_page + prot_size, osprot_to_memprot(prot));
        update_all_memory_areas(start_page, start_page + prot_size,
                                osprot_to_memprot(prot), -1/*type unchanged*/);
        all_memory_areas_unlock();
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

bool
ignorable_system_call(int num)
{
    switch (num) {
#if defined(SYS_exit_group)
    case SYS_exit_group:
#endif
    case SYS_exit:
    case SYS_brk:
    case SYS_mmap:
#ifndef X64
    case SYS_mmap2:
#endif
    case SYS_munmap:
    case SYS_mremap:
    case SYS_mprotect:
    case SYS_execve:
    case SYS_clone:
    case SYS_fork:
    case SYS_vfork:
    case SYS_kill:
#if defined(SYS_tkill)
    case SYS_tkill:
#endif
#if defined(SYS_tgkill)
    case SYS_tgkill:
#endif
#ifndef X64
    case SYS_signal:
    case SYS_sigaction:
    case SYS_sigsuspend:
    case SYS_sigpending:
    case SYS_sigreturn:
    case SYS_sigprocmask:
#endif
    case SYS_rt_sigreturn:
    case SYS_rt_sigaction:
    case SYS_rt_sigprocmask:
    case SYS_rt_sigpending:
    case SYS_rt_sigtimedwait:
    case SYS_rt_sigqueueinfo:
    case SYS_rt_sigsuspend:
    case SYS_sigaltstack:
#ifndef X64
    case SYS_sgetmask:
    case SYS_ssetmask:
#endif
    case SYS_setitimer:
    case SYS_getitimer:
    case SYS_close:
    case SYS_dup2:
    case SYS_dup3:
    case SYS_fcntl:
    case SYS_getrlimit:
    case SYS_setrlimit:
    /* i#107: syscall might change/query app's seg memory 
     * need stop app from clobbering our GDT slot.
     */
#ifdef X64
    case SYS_arch_prctl:
#endif
    case SYS_set_thread_area:
    case SYS_get_thread_area:
    /* FIXME: we might add SYS_modify_ldt later. */
        return false;
    default:
#ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(num))
            return vmkuw_ignorable_system_call(num);
#endif
        return true;
    }
}

typedef struct {
        unsigned long addr;
        unsigned long len;
        unsigned long prot;
        unsigned long flags;
        unsigned long fd;
        unsigned long offset;
} mmap_arg_struct_t;

#endif /* !NOT_DYNAMORIO_CORE_PROPER: around most of file, to exclude preload */

const reg_id_t syscall_regparms[MAX_SYSCALL_ARGS] = {
#ifdef X64
    DR_REG_RDI,
    DR_REG_RSI,
    DR_REG_RDX,
    DR_REG_R10,  /* RCX goes here in normal x64 calling contention. */
    DR_REG_R8,
    DR_REG_R9
#else
    DR_REG_EBX,
    DR_REG_ECX,
    DR_REG_EDX,
    DR_REG_ESI,
    DR_REG_EDI,
    DR_REG_EBP
#endif
};

#ifndef NOT_DYNAMORIO_CORE_PROPER

static inline reg_t *
sys_param_addr(dcontext_t *dcontext, int num)
{
    /* we force-inline get_mcontext() and so don't take it as a param */
    priv_mcontext_t *mc = get_mcontext(dcontext);
#ifdef X64
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
    case 0: return &mc->xbx;
    case 1: return &mc->xcx;
    case 2: return &mc->xdx;
    case 3: return &mc->xsi;
    case 4: return &mc->xdi;
    /* FIXME: do a safe_read: but what about performance?
     * See the #if 0 below, as well. */
    case 5: return (dcontext->sys_was_int ? &mc->xbp : ((reg_t*)mc->xsp));
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

/* since always coming from dispatch now, only need to set mcontext */
#define SET_RETURN_VAL(dc, val)  get_mcontext(dc)->xax = (val);

#ifdef CLIENT_INTERFACE
DR_API
reg_t
dr_syscall_get_param(void *drcontext, int param_num)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall,
                  "dr_syscall_get_param() can only be called from pre-syscall event");
    return sys_param(dcontext, param_num);
}

DR_API
void
dr_syscall_set_param(void *drcontext, int param_num, reg_t new_value)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                  dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_param() can only be called from a syscall event");
    *sys_param_addr(dcontext, param_num) = new_value;
}

DR_API
reg_t
dr_syscall_get_result(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_get_param() can only be called from post-syscall event");
    return get_mcontext(dcontext)->xax;
}

DR_API
void
dr_syscall_set_result(void *drcontext, reg_t value)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                  dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_result() can only be called from a syscall event");
    SET_RETURN_VAL(dcontext, value);
}

DR_API
void
dr_syscall_set_sysnum(void *drcontext, int new_num)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    priv_mcontext_t *mc = get_mcontext(dcontext);
    CLIENT_ASSERT(dcontext->client_data->in_pre_syscall ||
                  dcontext->client_data->in_post_syscall,
                  "dr_syscall_set_sysnum() can only be called from a syscall event");
    mc->xax = new_num;
}

DR_API
void
dr_syscall_invoke_another(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(dcontext->client_data->in_post_syscall,
                  "dr_syscall_invoke_another() can only be called from post-syscall event");
    LOG(THREAD, LOG_SYSCALLS, 2, "invoking additional syscall on client request\n");
    dcontext->client_data->invoke_another_syscall = true;
    if (get_syscall_method() == SYSCALL_METHOD_SYSENTER) {
        priv_mcontext_t *mc = get_mcontext(dcontext);
        /* restore xbp to xsp */
        mc->xbp = mc->xsp;
    }
    /* for x64 we don't need to copy xcx into r10 b/c we use r10 as our param */
}
#endif /* CLIENT_INTERFACE */

static inline bool
is_clone_thread_syscall_helper(ptr_uint_t sysnum, ptr_uint_t flags)
{
    return (sysnum == SYS_vfork ||
            (sysnum == SYS_clone && TEST(CLONE_VM, flags)));
}


bool
is_clone_thread_syscall(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    return is_clone_thread_syscall_helper(mc->xax, sys_param(dcontext, 0));
}

bool
was_clone_thread_syscall(dcontext_t *dcontext)
{
    return is_clone_thread_syscall_helper(dcontext->sys_num,
                                          /* flags in param0 */
                                          dcontext->sys_param0);
}

static inline bool
is_sigreturn_syscall_helper(int sysnum)
{
    return (IF_NOT_X64(sysnum == SYS_sigreturn ||) sysnum == SYS_rt_sigreturn);
}

bool
is_sigreturn_syscall(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    return is_sigreturn_syscall_helper(mc->xax);
}

bool
was_sigreturn_syscall(dcontext_t *dcontext)
{
    return is_sigreturn_syscall_helper(dcontext->sys_num);
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
        LOG(GLOBAL, LOG_TOP|LOG_SYSCALLS, 1,
            "thread %d sending itself a SIGABRT\n", get_thread_id());
        KSTOP(num_exits_dir_syscall);
        /* FIXME: need to check whether app has a handler for SIGABRT! */
        /* FIXME PR 211180/6723: this will do SYS_exit rather than the SIGABRT.
         * Should do set_default_signal_action(SIGABRT) (and set a flag so
         * no races w/ another thread re-installing?) and then SYS_kill.
         */
        cleanup_and_terminate(dcontext, SYS_exit, -1, 0,
                              (is_last_app_thread() && !dynamo_exited));
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
};
static const char * const env_to_propagate[] = {
    /* these must line up with the enum */
    DYNAMORIO_VAR_RUNUNDER,
    DYNAMORIO_VAR_OPTIONS,
    /* un-named */
    DYNAMORIO_VAR_CONFIGDIR,
};
#define NUM_ENV_TO_PROPAGATE (sizeof(env_to_propagate)/sizeof(env_to_propagate[0]))

static void
handle_execve(dcontext_t *dcontext)
{
    /* in /usr/src/linux/arch/i386/kernel/process.c:
     *  asmlinkage int sys_execve(struct pt_regs regs) { ...
     *  error = do_execve(filename, (char **) regs.xcx, (char **) regs.xdx, &regs);
     * in fs/exec.c:
     *  int do_execve(char * filename, char ** argv, char ** envp, struct pt_regs * regs)
     */
    /* We need to make sure we get injected into the new image:
     * we simply make sure LD_PRELOAD contains us, and that our directory
     * is on LD_LIBRARY_PATH (seems not to work to put absolute paths in
     * LD_PRELOAD).
     * FIXME: this doesn't work for setuid programs
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
    char *fname = (char *)  sys_param(dcontext, 0);
    char **envp = (char **) sys_param(dcontext, 2);
    int i, j, preload = -1, ldpath = -1;
    int prop_found[NUM_ENV_TO_PROPAGATE];
    int prop_idx[NUM_ENV_TO_PROPAGATE];
    bool preload_us = false, ldpath_us = false;
    bool x64 = IF_X64_ELSE(true, false);
    file_t file;
    char *inject_library_path;
    DEBUG_DECLARE(char **argv = (char **) sys_param(dcontext, 1);)

    LOG(GLOBAL, LOG_ALL, 1, "\n---------------------------------------------------------------------------\n");
    LOG(THREAD, LOG_ALL, 1, "\n---------------------------------------------------------------------------\n");
    DODEBUG({
        SYSLOG_INTERNAL_INFO("-- execve %s --", fname);
        LOG(THREAD, LOG_SYSCALLS, 1, "syscall: execve %s\n", fname);
        LOG(GLOBAL, LOG_TOP|LOG_SYSCALLS, 1, "execve %s\n", fname);
        if (stats->loglevel >= 3) {
            if (argv == NULL) {
                LOG(THREAD, LOG_SYSCALLS, 3, "\targs are NULL\n");
            } else {
                for (i = 0; argv[i] != NULL; i++) {
                    LOG(THREAD, LOG_SYSCALLS, 2, "\targ %d: len=%d\n",
                        i, strlen(argv[i]));
                    LOG(THREAD, LOG_SYSCALLS, 3, "\targ %d: %s\n",
                        i, argv[i]);
                }
            }
        }
    });

    /* Issue 20: handle cross-architecture execve */
    /* Xref alternate solution i#145: use dual paths on
     * LD_LIBRARY_PATH to solve cross-arch execve
     */
    file = os_open(fname, OS_OPEN_READ);
    if (file != INVALID_FILE) {
        x64 = file_is_elf64(file);
        os_close(file);
    }
    inject_library_path = IF_X64_ELSE(x64, !x64) ? dynamorio_library_path :
        dynamorio_alt_arch_path;

    for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
        prop_found[j] = -1;
        prop_idx[j] = -1;
    }

    if (envp == NULL) {
        LOG(THREAD, LOG_SYSCALLS, 3, "\tenv is NULL\n");
        i = 0;
    } else {
        for (i = 0; envp[i] != NULL; i++) {
            /* execve env vars should never be set here */
            ASSERT(strstr(envp[i], DYNAMORIO_VAR_EXECVE) != envp[i]);
            for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
                if (strstr(envp[i], env_to_propagate[j]) == envp[i]) {
                    prop_found[j] = i;
                    break;
                }
            }
            if (strstr(envp[i], "LD_LIBRARY_PATH=") == envp[i]) {
                ldpath = i;
                if (strstr(envp[i], inject_library_path) != NULL)
                    ldpath_us = true;
            }
            if (strstr(envp[i], "LD_PRELOAD=") == envp[i]) {
                preload = i;
                if (strstr(envp[i], DYNAMORIO_PRELOAD_NAME) != NULL &&
                    strstr(envp[i], DYNAMORIO_LIBRARY_NAME) != NULL) {
                    preload_us = true;
                }
            }
            LOG(THREAD, LOG_SYSCALLS, 3, "\tenv %d: %s\n", i, envp[i]);
        }
    }

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
     * set a flag and clean up in dispatch, but that seems overkill.  (If vfork
     * didn't suspend the parent we'd need to touch a marker file or something
     * to know the execve was finished.)
     */
    mark_thread_execve(dcontext->thread_record, true);

#ifdef STATIC_LIBRARY
    /* no way we can inject, we just lose control */
    SYSLOG_INTERNAL_WARNING("WARNING: static DynamoRIO library, losing control on execve");
    return;
#endif

    /* We want to add new env vars, so we create a new envp
     * array.  We have to deallocate them and restore the old
     * envp if execve fails; if execve succeeds, the address
     * space is reset so we don't need to do anything.  
     */
    int num_old = i;
    uint sz;
    char *var, *old;
    int idx_preload = preload, idx_ldpath = ldpath;
    int num_new;
    char **new_envp;
    uint logdir_length;

    num_new = 
        2 + /* execve indicator var plus final NULL */
        ((preload<0) ? 1 : 0) +
        ((ldpath<0) ? 1 : 0);
    if (DYNAMO_OPTION(follow_children)) {
        num_new += (get_log_dir(PROCESS_DIR, NULL, NULL) ? 1 : 0) /* logdir */;
        for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
            if (prop_found[j] < 0)
                num_new++;
        }
    }
    new_envp = heap_alloc(dcontext, sizeof(char*)*(num_old+num_new)
                          HEAPACCT(ACCT_OTHER));
    memcpy(new_envp, envp, sizeof(char*)*num_old);

    *sys_param_addr(dcontext, 2) = (reg_t) new_envp;  /* OUT */
    /* store for reset in case execve fails, and for cleanup if
     * this is a vfork thread
     */
    dcontext->sys_param0 = (reg_t) envp;
    dcontext->sys_param1 = (reg_t) new_envp;
    /* if no var to overwrite, need new var at end */
    if (preload < 0)
        idx_preload = i++;
    if (ldpath < 0)
        idx_ldpath = i++;
    if (DYNAMO_OPTION(follow_children)) {
        for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
            prop_idx[j] = prop_found[j];
            if (prop_idx[j] < 0)
                prop_idx[j] = i++;
        }
    }

    if (!preload_us) {
        LOG(THREAD, LOG_SYSCALLS, 1,
            "WARNING: execve env does NOT preload DynamoRIO, forcing it!\n");
        if (preload >= 0) {
            sz = strlen(envp[preload]) + strlen(DYNAMORIO_PRELOAD_NAME)+
                strlen(DYNAMORIO_LIBRARY_NAME) + 3;
            var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
            old = envp[preload] + strlen("LD_PRELOAD=");
            snprintf(var, sz, "LD_PRELOAD=%s %s %s",
                     DYNAMORIO_PRELOAD_NAME, DYNAMORIO_LIBRARY_NAME, old);
        } else {
            sz = strlen("LD_PRELOAD=") + strlen(DYNAMORIO_PRELOAD_NAME) + 
                strlen(DYNAMORIO_LIBRARY_NAME) + 2;
            var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "LD_PRELOAD=%s %s",
                     DYNAMORIO_PRELOAD_NAME, DYNAMORIO_LIBRARY_NAME);
        }
        *(var+sz-1) = '\0'; /* null terminate */
        new_envp[idx_preload] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n",
            idx_preload, new_envp[idx_preload]);
    }

    if (!ldpath_us) {
        if (ldpath >= 0) {
            sz = strlen(envp[ldpath]) + strlen(inject_library_path) + 2;
            var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
            old = envp[ldpath] + strlen("LD_LIBRARY_PATH=");
            snprintf(var, sz, "LD_LIBRARY_PATH=%s:%s", inject_library_path, old);
        } else {
            sz = strlen("LD_LIBRARY_PATH=") + strlen(inject_library_path) + 1;
            var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "LD_LIBRARY_PATH=%s", inject_library_path);
        }
        *(var+sz-1) = '\0'; /* null terminate */
        new_envp[idx_ldpath] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n",
            idx_ldpath, new_envp[idx_ldpath]);
    }

    if (DYNAMO_OPTION(follow_children)) {
        for (j = 0; j < NUM_ENV_TO_PROPAGATE; j++) {
            const char *val = "";
            bool set_env_var = (prop_found[j] < 0);
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
                val = option_string;
                /* i#1097: don't use options from the app's envp; use ours. */
                set_env_var = true;
                break;
            default:
                val = getenv(env_to_propagate[j]);
                if (val == NULL)
                    val = "";
                break;
            }
            if (set_env_var) {
                sz = strlen(env_to_propagate[j]) + strlen(val) + 2 /* '=' + null */;
                var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
                snprintf(var, sz, "%s=%s", env_to_propagate[j], val);
                *(var+sz-1) = '\0'; /* null terminate */
                new_envp[prop_idx[j]] = var;
                LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n",
                    prop_idx[j], new_envp[prop_idx[j]]);
            }
        }
        if (get_log_dir(PROCESS_DIR, NULL, &logdir_length)) {
            sz = strlen(DYNAMORIO_VAR_EXECVE_LOGDIR) + 1 + logdir_length;
            var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
            snprintf(var, sz, "%s=", DYNAMORIO_VAR_EXECVE_LOGDIR);
            get_log_dir(PROCESS_DIR, var+strlen(var), &logdir_length);
            *(var+sz-1) = '\0'; /* null terminate */
            new_envp[i++] = var;
            LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", i-1, new_envp[i-1]);
        }
    } else if (prop_idx[ENV_PROP_RUNUNDER] >= 0) {
        /* disable auto-following of this execve, yet still allow preload
         * on other side to inject if config file exists.
         * kind of hacky mangle here:
         */
        ASSERT(new_envp[prop_idx[ENV_PROP_RUNUNDER]][0] == 'D');
        new_envp[prop_idx[ENV_PROP_RUNUNDER]][0] = 'X';
    }

    sz = strlen(DYNAMORIO_VAR_EXECVE) + 4;
    /* we always pass this var to indicate "post-execve" */
    var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
    /* PR 458917: we overload this to also pass our gdt index */
    ASSERT(os_tls_get_gdt_index(dcontext) < 100 &&
           os_tls_get_gdt_index(dcontext) >= -1); /* only 2 chars allocated */
    snprintf(var, sz, "%s=%02d", DYNAMORIO_VAR_EXECVE, os_tls_get_gdt_index(dcontext));
    *(var+sz-1) = '\0'; /* null terminate */
    new_envp[i++] = var;
    LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", i-1, new_envp[i-1]);
    /* must end with NULL */
    new_envp[i] = NULL;

    /* we need to clean up the .1config file here.  if the execve fails,
     * we'll just live w/o dynamic option re-read.
     */
    config_exit();
}

static void
handle_execve_post(dcontext_t *dcontext)
{
    /* if we get here it means execve failed (doesn't return on success),
     * or we did an execve from a vfork and its memory changes are visible
     * in the parent process.
     * we have to restore env to how it was and free the allocated heap.
     */
    char **old_envp = (char **) dcontext->sys_param0;
    char **new_envp = (char **) dcontext->sys_param1;
#ifdef STATIC_LIBRARY
    /* nothing to clean up */
    return;
#endif
    if (new_envp != NULL) {
        int i;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tcleaning up our env vars\n");
        /* we replaced existing ones and/or added new ones.
         * we can't compare to old_envp b/c it may have changed by now.
         */
        for (i=0; new_envp[i] != NULL; i++) {
            if (is_dynamo_address((byte *)new_envp[i])) {
                heap_free(dcontext, new_envp[i],
                          sizeof(char)*(strlen(new_envp[i])+1)
                          HEAPACCT(ACCT_OTHER));
            }
        }
        i++; /* need to de-allocate final null slot too */
        heap_free(dcontext, new_envp, sizeof(char*)*i HEAPACCT(ACCT_OTHER));
        /* restore prev envp if we're post-syscall */
        if (!dcontext->thread_record->execve)
            *sys_param_addr(dcontext, 2) = (reg_t) old_envp;
    }
}

/* i#237/PR 498284: to avoid accumulation of thread state we clean up a vfork
 * child who invoked execve here so we have at most one outstanding thread.  we
 * also clean up at process exit and before thread creation.  we could do this
 * in dispatch but too rare to be worth a flag check there.
 */
static void
cleanup_after_vfork_execve(dcontext_t *dcontext)
{
    thread_record_t **threads;
    int num_threads, i;
    if (num_execve_threads == 0)
        return;

    mutex_lock(&thread_initexit_lock);
    get_list_of_threads_ex(&threads, &num_threads, true/*include execve*/);
    for (i=0; i<num_threads; i++) {
        if (threads[i]->execve) {
            LOG(THREAD, LOG_SYSCALLS, 2, "cleaning up earlier vfork thread %d\n",
                threads[i]->id);
            dynamo_other_thread_exit(threads[i]);
        }
    }
    mutex_unlock(&thread_initexit_lock);
    global_heap_free(threads, num_threads*sizeof(thread_record_t*)
                     HEAPACCT(ACCT_THREAD_MGT));
}

/* returns whether to execute syscall */
static bool
handle_close_pre(dcontext_t *dcontext)
{
    /* in fs/open.c: asmlinkage long sys_close(unsigned int fd) */
    uint fd = (uint) sys_param(dcontext, 0);
    LOG(THREAD, LOG_SYSCALLS, 3, "syscall: close fd %d\n", fd);

    /* prevent app from closing our files */
    if (fd_is_dr_owned(fd)) {
        SYSLOG_INTERNAL_WARNING_ONCE("app trying to close DR file(s)");
        LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
            "WARNING: app trying to close DR file %d!  Not allowing it.\n", fd);
        SET_RETURN_VAL(dcontext, -EBADF);
        DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
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
        LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
            "WARNING: app is closing stdout=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n", fd, our_stdout);
        if (privmod_stdout != NULL &&
            IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            /* update the privately loaded libc's stdout _fileno. */
            (*privmod_stdout)->_fileno = our_stdout;
        }
    }
    if (DYNAMO_OPTION(dup_stderr_on_close) && fd == STDERR) {
        our_stderr = fd_priv_dup(fd);
        if (our_stderr < 0) /* no private fd available */
            our_stderr = dup_syscall(fd);
        if (our_stderr >= 0)
            fd_mark_close_on_exec(our_stderr);
        fd_table_add(our_stderr, 0);
        LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
            "WARNING: app is closing stderr=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n", fd, our_stderr);
        if (privmod_stderr != NULL && 
            IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            /* update the privately loaded libc's stderr _fileno. */
            (*privmod_stderr)->_fileno = our_stderr;
        }
    }
    if (DYNAMO_OPTION(dup_stdin_on_close) && fd == STDIN) {
        our_stdin = fd_priv_dup(fd);
        if (our_stdin < 0) /* no private fd available */
            our_stdin = dup_syscall(fd);
        if (our_stdin >= 0)
            fd_mark_close_on_exec(our_stdin);
        fd_table_add(our_stdin, 0);
        LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
            "WARNING: app is closing stdin=%d - duplicating descriptor for "
            "DynamoRIO usage got %d.\n", fd, our_stdin);
        if (privmod_stdin != NULL &&
            IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            /* update the privately loaded libc's stdout _fileno. */
            (*privmod_stdin)->_fileno = our_stdin;
        }
    }
    return true;
}

/***************************************************************************/

/* Used to obtain the pc of the syscall instr itself when the dcontext dc
 * is currently in a syscall handler.
 * Alternatively for sysenter we could set app_sysenter_instr_addr for Linux.
 */
#define SYSCALL_PC(dc) \
 ((get_syscall_method() == SYSCALL_METHOD_INT ||     \
   get_syscall_method() == SYSCALL_METHOD_SYSCALL) ? \
  (ASSERT(SYSCALL_LENGTH == INT_LENGTH),             \
   POST_SYSCALL_PC(dc) - INT_LENGTH) :               \
  (vsyscall_syscall_end_pc - SYSENTER_LENGTH))

static void
handle_exit(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool exit_process = false;

    if (mc->xax == SYS_exit_group) {
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
        mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num_threads);
        for (i=0; i<num_threads; i++) {
            if (threads[i]->pid != mypid && !IS_CLIENT_THREAD(threads[i]->dcontext)) {
                exit_process = false;
                break;
            }
        }
        if (!exit_process) {
            /* We need to clean up the other threads in our group here. */
            thread_id_t myid = get_thread_id();
            priv_mcontext_t mcontext;
            DEBUG_DECLARE(thread_synch_result_t synch_res;)
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "SYS_exit_group %d not final group: %d cleaning up just "
                "threads in group\n", get_process_id(), get_thread_id());
            /* Set where we are to handle reciprocal syncs */
            copy_mcontext(mc, &mcontext);
            mc->pc = SYSCALL_PC(dcontext);
            for (i=0; i<num_threads; i++) {
                if (threads[i]->id != myid && threads[i]->pid == mypid) {
                    /* See comments in dynamo_process_exit_cleanup(): we terminate
                     * to make cleanup easier, but may want to switch to shifting
                     * the target thread to a stack-free loop.
                     */
                    DEBUG_DECLARE(synch_res =)
                        synch_with_thread(threads[i]->id, true/*block*/,
                                          true/*have initexit lock*/,
                                          THREAD_SYNCH_VALID_MCONTEXT, 
                                          THREAD_SYNCH_TERMINATED_AND_CLEANED,
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
        mutex_unlock(&thread_initexit_lock);
        global_heap_free(threads, num_threads*sizeof(thread_record_t*)
                         HEAPACCT(ACCT_THREAD_MGT));
    }

    if (is_last_app_thread() && !dynamo_exited) {
        LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
            "SYS_exit%s(%d) in final thread %d of %d => exiting DynamoRIO\n",
            (mc->xax == SYS_exit_group) ? "_group" : "", mc->xax,
            get_thread_id(), get_process_id());
        /* we want to clean up even if not automatic startup! */
        automatic_startup = true;
        exit_process = true;
    } else {
        LOG(THREAD, LOG_TOP|LOG_THREADS|LOG_SYSCALLS, 1,
            "SYS_exit%s(%d) in thread %d of %d => cleaning up %s\n",
            (mc->xax == SYS_exit_group) ? "_group" : "",
            mc->xax, get_thread_id(), get_process_id(),
            exit_process ? "process" : "thread");
    }
    KSTOP(num_exits_dir_syscall);

    cleanup_and_terminate(dcontext, mc->xax, sys_param(dcontext, 0),
                          sys_param(dcontext, 1), exit_process);
}

bool
os_set_app_thread_area(dcontext_t *dcontext, our_modify_ldt_t *user_desc)
{
    int i;
    os_thread_data_t *ostd = dcontext->os_field;
    our_modify_ldt_t *desc = (our_modify_ldt_t *)ostd->app_thread_areas;

    if (user_desc->seg_not_present == 1) {
        /* find a empty one to update */
        for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
            if (desc[i].seg_not_present == 1)
                break;
        }
        if (i < GDT_NUM_TLS_SLOTS) {
            user_desc->entry_number = GDT_SELECTOR(i + gdt_entry_tls_min);
            memcpy(&desc[i], user_desc, sizeof(*user_desc));
        } else
            return false;
    } else {
        /* If we used early injection, this might be ld.so trying to set up TLS.  We
         * direct the app to use the GDT entry we already set up for our private
         * libraries, but only the first time it requests TLS.
         */
        if (user_desc->entry_number == -1 && return_stolen_lib_tls_gdt) {
            mutex_lock(&set_thread_area_lock);
            if (return_stolen_lib_tls_gdt) {
                uint selector = read_selector(LIB_SEG_TLS);
                uint index = SELECTOR_INDEX(selector);
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
                return_stolen_lib_tls_gdt = false;
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
                user_desc->entry_number = index;
                LOG(GLOBAL, LOG_THREADS, 2, "%s: directing app to use "
                    "selector 0x%x for first call to set_thread_area\n",
                    __FUNCTION__, selector);
            }
            mutex_unlock(&set_thread_area_lock);
        }

        /* update the specific one */
        i = user_desc->entry_number - gdt_entry_tls_min;
        if (i < 0 || i >= GDT_NUM_TLS_SLOTS)
            return false;
        LOG(GLOBAL, LOG_THREADS, 2,
            "%s: change selector 0x%x base from "PFX" to "PFX"\n",
            __FUNCTION__, GDT_SELECTOR(user_desc->entry_number),
            desc[i].base_addr, user_desc->base_addr);
        memcpy(&desc[i], user_desc, sizeof(*user_desc));
    }
    /* if not conflict with dr's tls, perform the syscall */
    if (IF_CLIENT_INTERFACE_ELSE(!INTERNAL_OPTION(private_loader), true) &&
        GDT_SELECTOR(user_desc->entry_number) != read_selector(SEG_TLS) &&
        GDT_SELECTOR(user_desc->entry_number) != read_selector(LIB_SEG_TLS))
        return false;
    return true;
}

bool
os_get_app_thread_area(dcontext_t *dcontext, our_modify_ldt_t *user_desc)
{
    os_thread_data_t *ostd = (os_thread_data_t *)dcontext->os_field;
    our_modify_ldt_t *desc = (our_modify_ldt_t *)ostd->app_thread_areas;
    int i = user_desc->entry_number - gdt_entry_tls_min;
    if (i < 0 || i >= GDT_NUM_TLS_SLOTS)
        return false;
    if (desc[i].seg_not_present == 1)
        return false;
    return true;
}

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

static bool
os_switch_seg_to_context(dcontext_t *dcontext, reg_id_t seg, bool to_app)
{
    int res = -1;
    app_pc base;
    os_local_state_t *os_tls = get_os_tls_from_dc(dcontext);

    /* we can only update the executing thread's segment (i#920) */
    ASSERT_MESSAGE(CHKLVL_ASSERTS+1/*expensive*/, "can only act on executing thread",
                   dcontext == get_thread_private_dcontext());
    ASSERT(seg == SEG_FS || seg == SEG_GS);
    if (to_app) {
        base = os_get_app_seg_base(dcontext, seg);
    } else {
        base = os_get_dr_seg_base(dcontext, seg);
    }
    switch (os_tls->tls_type) {
# ifdef X64
    case TLS_TYPE_ARCH_PRCTL: {
        int prctl_code = (seg == SEG_FS ? ARCH_SET_FS : ARCH_SET_GS);
        res = dynamorio_syscall(SYS_arch_prctl, 2, prctl_code, base);
        ASSERT(res >= 0);
        LOG(GLOBAL, LOG_THREADS, 2,
            "%s %s: arch_prctl successful for thread %d base "PFX"\n",
            __FUNCTION__, to_app ? "to app" : "to DR", get_thread_id(), base);
        if (seg == SEG_TLS && base == NULL) {
            /* Set the selector to 0 so we don't think TLS is available. */
            /* FIXME i#107: Still assumes app isn't using SEG_TLS. */
            reg_t zero = 0;
            WRITE_DR_SEG(zero);
        }
        break;
    }
# endif
    case TLS_TYPE_GDT: {
        our_modify_ldt_t desc;
        uint index;
        uint selector;
        if (to_app) {
            selector = (seg == SEG_FS ? os_tls->app_fs : os_tls->app_gs);
            index = SELECTOR_INDEX(selector);
        } else {
            index = (seg == LIB_SEG_TLS ? lib_tls_gdt_index : tls_gdt_index);
            ASSERT(index != -1 && "TLS indices not initialized");
            selector = GDT_SELECTOR(index);
        }
        if (selector != 0) {
            if (to_app) {
                our_modify_ldt_t *areas = 
                    ((os_thread_data_t *)dcontext->os_field)->app_thread_areas;
                ASSERT((index >= gdt_entry_tls_min) && 
                       ((index - gdt_entry_tls_min) <= GDT_NUM_TLS_SLOTS));
                desc = areas[index - gdt_entry_tls_min];
            } else {
                initialize_ldt_struct(&desc, base, GDT_NO_SIZE_LIMIT, index);
            }
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            ASSERT(res >= 0);
        } else {
            /* For a selector of zero, we just reset the segment to zero.  We
             * don't need to call set_thread_area.
             */
            res = 0;  /* Indicate success. */
        }
        /* i558 update lib seg reg to enforce the segment changes */
        LOG(THREAD, LOG_LOADER, 2, "%s: switching to %s, setting %s to 0x%x\n",
            __FUNCTION__, (to_app ? "app" : "dr"), reg_names[seg], selector);
        WRITE_LIB_SEG(selector);
        LOG(THREAD, LOG_LOADER, 2,
            "%s %s: set_thread_area successful for thread %d base "PFX"\n",
            __FUNCTION__, to_app ? "to app" : "to DR", get_thread_id(), base);
        break;
    }
    default:
        /* not support ldt yet. */
        ASSERT_NOT_IMPLEMENTED(false);
        return false;
    }
    ASSERT(BOOLS_MATCH(to_app, os_using_app_state(dcontext)));
    /* FIXME: We do not support using ldt yet. */
    return (res >= 0);
}


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
/* Returns false if system call should NOT be executed
 * Returns true if system call should go ahead
 */
/* FIXME: split out specific handlers into separate routines
 */
bool
pre_system_call(dcontext_t *dcontext)
{
    priv_mcontext_t *mc = get_mcontext(dcontext);
    bool execute_syscall = true;
    where_am_i_t old_whereami = dcontext->whereami;
    dcontext->whereami = WHERE_SYSCALL_HANDLER;
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
    dcontext->sys_num = mc->xax;

    RSTATS_INC(pre_syscall);
    DOSTATS({
        if (ignorable_system_call(mc->xax))
            STATS_INC(pre_syscall_ignorable);
    });
    LOG(THREAD, LOG_SYSCALLS, 2, "system call %d\n", mc->xax);

    /* PR 313715: If we fail to hook the vsyscall page (xref PR 212570, PR 288330)
     * we fall back on int, but we have to tweak syscall param #5 (ebp)
     * Once we have PR 288330 we can remove this.
     */
    if (should_syscall_method_be_sysenter() && !dcontext->sys_was_int) {
        dcontext->sys_xbp = mc->xbp;
        /* not using SAFE_READ due to performance concerns (we do this for
         * every single system call on systems where we can't hook vsyscall!)
         */
        TRY_EXCEPT(dcontext, /* try */ {
            mc->xbp = *(reg_t*)mc->xsp;
        }, /* except */ {
            ASSERT_NOT_REACHED();
            mc->xbp = 0;
        });
    }

    switch (mc->xax) {

    case SYS_exit_group:
# ifdef VMX86_SERVER
        if (os_in_vmkernel_32bit()) {
            /* on esx 3.5 => ENOSYS, so wait for SYS_exit */
            LOG(THREAD, LOG_SYSCALLS, 2, "on esx35 => ignoring exitgroup\n");
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            break;
        }
# endif
        /* fall-through */
    case SYS_exit: {
        handle_exit(dcontext);
        break;
    }

    /****************************************************************************/
    /* MEMORY REGIONS */

#ifndef X64
    case SYS_mmap: {
        /* in /usr/src/linux/arch/i386/kernel/sys_i386.c:
           asmlinkage int old_mmap(struct mmap_arg_struct_t *arg)
         */
        mmap_arg_struct_t *arg = (mmap_arg_struct_t *) sys_param(dcontext, 0);
        DOLOG(2, LOG_SYSCALLS, {
            mmap_arg_struct_t arg_buf;
            if (safe_read(arg, sizeof(mmap_arg_struct_t), &arg_buf)) {
                void *addr = (void *) arg->addr;
                size_t len = (size_t) arg->len;
                uint prot = (uint) arg->prot;
                LOG(THREAD, LOG_SYSCALLS, 2,
                    "syscall: mmap addr="PFX" size="PIFX" prot=0x%x"
                    " flags="PIFX" offset="PIFX" fd=%d\n",
                    addr, len, prot, arg->flags, arg->offset, arg->fd);
            }
        });
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t) arg;
        break;
    }
#endif
    case IF_X64_ELSE(SYS_mmap,SYS_mmap2): {
        /* in /usr/src/linux/arch/i386/kernel/sys_i386.c:
           asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
             unsigned long prot, unsigned long flags,
             unsigned long fd, unsigned long pgoff)
         */
        void *addr = (void *) sys_param(dcontext, 0);
        size_t len = (size_t) sys_param(dcontext, 1);
        uint prot = (uint) sys_param(dcontext, 2);
        uint flags = (uint) sys_param(dcontext, 3);
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: mmap2 addr="PFX" size="PIFX" prot=0x%x"
            " flags="PIFX" offset="PIFX" fd=%d\n",
            addr, len, prot, flags,
            sys_param(dcontext, 5), sys_param(dcontext, 4));
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t) addr;
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
        app_pc addr = (void *) sys_param(dcontext, 0);
        size_t len = (size_t) sys_param(dcontext, 1);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: munmap addr="PFX" size="PFX"\n",
            addr, len);
        RSTATS_INC(num_app_munmaps);
        /* FIXME addr is supposed to be on a page boundary so we
         * could detect that condition here and set
         * expect_last_syscall_to_fail.
         */
        /* save params in case an undo is needed in post_system_call */
        dcontext->sys_param0 = (reg_t) addr;
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
            ASSERT_CURIOSITY((app_pc)ALIGN_FORWARD(addr+len, PAGE_SIZE) == ma->end);
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
        remove_from_all_memory_areas(addr, addr + len);
        break;
    }
    case SYS_mremap: {
        /* in /usr/src/linux/mm/mmap.c:
           asmlinkage unsigned long sys_mremap(unsigned long addr,
             unsigned long old_len, unsigned long new_len,
             unsigned long flags, unsigned long new_addr)
        */
        dr_mem_info_t info;
        app_pc addr = (void *) sys_param(dcontext, 0);
        size_t old_len = (size_t) sys_param(dcontext, 1);
        size_t new_len = (size_t) sys_param(dcontext, 2);
        DEBUG_DECLARE(bool ok;)
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: mremap addr="PFX" size="PFX"\n",
            addr, old_len);
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t) addr;
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
    case SYS_mprotect: {
        /* in /usr/src/linux/mm/mprotect.c:
           asmlinkage long sys_mprotect(unsigned long start, uint len,
           unsigned long prot)
        */
        uint res;
        DEBUG_DECLARE(app_pc end;)
        app_pc addr  = (void *) sys_param(dcontext, 0);
        size_t len  = (size_t) sys_param(dcontext, 1);
        uint prot = (uint) sys_param(dcontext, 2);
        uint new_memprot;
        /* save params in case an undo is needed in post_system_call */
        dcontext->sys_param0 = (reg_t) addr;
        dcontext->sys_param1 = len;
        dcontext->sys_param2 = prot;
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: mprotect addr="PFX" size="PFX" prot=%s\n",
            addr, len, memprot_string(osprot_to_memprot(prot)));

        /* PR 413109 - fail mprotect if start region is unknown; seen in hostd.
         * FIXME: get_memory_info_from_os() should be used instead of 
         *      vmvector_lookup_data() to catch mprotect failure cases on shared
         *      memory allocated by another process.  However, till PROC_MAPS 
         *      are implemented on visor, get_memory_info_from_os() can't 
         *      distinguish between inaccessible and unallocated, so it doesn't 
         *      work.  Once PROC_MAPS is available on visor use 
         *      get_memory_info_from_os() and resolve case.
         *
         * FIXME: Failing mprotect if addr isn't allocated doesn't help if there
         *      are unallocated pages in the middle of the the mprotect region. 
         *      As it will be expensive to do page wise check for each mprotect 
         *      syscall just to guard against a corner case, it might be better 
         *      to let the system call fail and recover in post_system_call(). 
         *      See PR 410921.
         */
        if (!vmvector_lookup_data(all_memory_areas, addr, NULL, 
                                  IF_DEBUG_ELSE(&end, NULL), NULL)) {
            LOG(THREAD, LOG_SYSCALLS, 2,
                "\t"PFX" isn't mapped; aborting mprotect\n", addr);
            execute_syscall = false;
            SET_RETURN_VAL(dcontext, -ENOMEM);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            break;
        } else {
            /* If mprotect region spans beyond the end of the vmarea then it 
             * spans 2 or more vmareas with dissimilar protection (xref 
             * PR 410921) or has unallocated regions in between (PR 413109).
             */
            DOCHECK(1, dcontext->mprot_multi_areas = (addr + len) > end ? true : false;);
        }

        res = app_memory_protection_change(dcontext, addr, len, 
                                           osprot_to_memprot(prot),
                                           &new_memprot, NULL);
        if (res != DO_APP_MEM_PROT_CHANGE) {
            if (res == FAIL_APP_MEM_PROT_CHANGE) {
                ASSERT_NOT_IMPLEMENTED(false); /* return code? */
            } else {
                ASSERT_NOT_IMPLEMENTED(res != SUBSET_APP_MEM_PROT_CHANGE);
                ASSERT_NOT_REACHED();
            }
            execute_syscall = false;
        }
        else {
            /* FIXME Store state for undo if the syscall fails. */
            /* update all_memory_areas list */
            all_memory_areas_lock();
            ASSERT(vmvector_overlap(all_memory_areas, addr, addr + len) ||
                   /* we could synch: instead, relax assert if DR areas not in allmem */
                   are_dynamo_vm_areas_stale());
            LOG(GLOBAL, LOG_VMAREAS|LOG_SYSCALLS, 3,
                "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
                addr, addr+len, osprot_to_memprot(prot));
            update_all_memory_areas(addr, addr+len, osprot_to_memprot(prot),
                                    -1/*type unchanged*/);
            all_memory_areas_unlock();
        }
        break;
    }
    case SYS_brk: {
        /* i#91/PR 396352: need to watch SYS_brk to maintain all_memory_areas.
         * We store the old break in the param1 slot.
         */
        DODEBUG(dcontext->sys_param0 = (reg_t) sys_param(dcontext, 0););
        dcontext->sys_param1 = dynamorio_syscall(SYS_brk, 1, 0);
        break;
    }
    case SYS_uselib: {
        /* Used to get the kernel to load a share library (legacy system call).
         * Was primarily used when statically linking to dynamically loaded shared
         * libraries that were loaded at known locations.  Shouldn't be used by
         * applications using the dynamic loader (ld) which is currently the only
         * way we can inject so we don't expect to see this.  PR 307621. */
        ASSERT_NOT_IMPLEMENTED(false);
        break;
    }

    /****************************************************************************/
    /* SPAWNING */

    case SYS_clone: {
        /* in /usr/src/linux/arch/i386/kernel/process.c 
         * 32-bit params: flags, newsp, ptid, tls, ctid
         * 64-bit params: should be the same yet tls (for ARCH_SET_FS) is in r8?!?
         *   I don't see how sys_clone gets its special args: shouldn't it
         *   just get pt_regs as a "special system call"?
         *   sys_clone(unsigned long clone_flags, unsigned long newsp,
         *     void __user *parent_tid, void __user *child_tid, struct pt_regs *regs)
         */
        uint flags = (uint) sys_param(dcontext, 0);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: clone with flags = "PFX"\n", flags);
        LOG(THREAD, LOG_SYSCALLS, 2, "args: "PFX", "PFX", "PFX", "PFX", "PFX"\n",
            sys_param(dcontext, 0), sys_param(dcontext, 1), sys_param(dcontext, 2),
            sys_param(dcontext, 3), sys_param(dcontext, 4));
        handle_clone(dcontext, flags);
        if ((flags & CLONE_VM) == 0) {
            LOG(THREAD, LOG_SYSCALLS, 1, "\tWARNING: CLONE_VM not set!\n");
        }
        /* save for post_system_call */
        dcontext->sys_param0 = (reg_t) flags;

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
        if (is_clone_thread_syscall(dcontext))
            create_clone_record(dcontext, sys_param_addr(dcontext, 1) /*newsp*/);
        else  /* This is really a fork. */
            os_fork_pre(dcontext);
        /* We switch the lib tls segment back to app's segment.
         * Please refer to comment on os_switch_lib_tls.
         */
        if (TEST(CLONE_VM, flags) /* not creating process */ &&
            IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            os_switch_lib_tls(dcontext, true/*to app*/);
        }
        break;
    }

    case SYS_vfork: {
        /* treat as if sys_clone with flags just as sys_vfork does */
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        uint flags = CLONE_VFORK | CLONE_VM | SIGCHLD;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork\n");
        handle_clone(dcontext, flags);
        cleanup_after_vfork_execve(dcontext);

        /* save for post_system_call, treated as if SYS_clone */
        dcontext->sys_param0 = (reg_t) flags;

        /* vfork has the same needs as clone.  Pass info via a clone_record_t
         * structure to child.  See SYS_clone for info about i#149/PR 403015.
         */
        if (is_clone_thread_syscall(dcontext)) {
            dcontext->sys_param1 = mc->xsp; /* for restoring in parent */
            create_clone_record(dcontext, (reg_t *)&mc->xsp /*child uses parent sp*/);
        }
        /* We switch the lib tls segment back to app's segment.
         * Please refer to comment on os_switch_lib_tls.
         */
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            os_switch_lib_tls(dcontext, true/*to app*/);
        }
        break;
    }

    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork\n");
        os_fork_pre(dcontext);
        break;
    }

    case SYS_execve: {
        handle_execve(dcontext);
        break;
    }

    /****************************************************************************/
    /* SIGNALS */
 
    case SYS_rt_sigaction: {   /* 174 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigaction(int sig, const struct sigaction *act, 
             struct sigaction *oact, size_t sigsetsize)
         */
        int sig  = (int) sys_param(dcontext, 0);
        const kernel_sigaction_t *act = (const kernel_sigaction_t *) sys_param(dcontext, 1);
        kernel_sigaction_t *oact = (kernel_sigaction_t *) sys_param(dcontext, 2);
        size_t sigsetsize = (size_t) sys_param(dcontext, 3);
        /* post_syscall does some work as well */
        dcontext->sys_param0 = (reg_t) sig;
        dcontext->sys_param1 = (reg_t) act;
        dcontext->sys_param2 = (reg_t) oact;
        dcontext->sys_param3 = (reg_t) sigsetsize;
        execute_syscall = handle_sigaction(dcontext, sig, act, oact, sigsetsize);
        if (!execute_syscall) {
            SET_RETURN_VAL(dcontext, 0);
        }
        break;
    }
#ifndef X64
    case SYS_sigreturn: {      /* 119 */
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
    case SYS_rt_sigreturn: {   /* 173 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int sys_rt_sigreturn(unsigned long __unused)
         */
        execute_syscall = handle_sigreturn(dcontext, true);
        /* see comment for SYS_sigreturn on return val */
        break;
    }
    case SYS_sigaltstack: {    /* 186 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int
           sys_sigaltstack(const stack_t *uss, stack_t *uoss)
        */
        const stack_t *uss = (const stack_t *) sys_param(dcontext, 0);
        stack_t *uoss = (stack_t *) sys_param(dcontext, 1);
        execute_syscall =
            handle_sigaltstack(dcontext, uss, uoss);
        if (!execute_syscall) {
            SET_RETURN_VAL(dcontext, 0);
        }
        break;
    }
    case SYS_rt_sigprocmask: { /* 175 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset, 
             size_t sigsetsize)
         */
        /* we also need access to the params in post_system_call */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
        dcontext->sys_param3 = sys_param(dcontext, 3);
        handle_sigprocmask(dcontext, (int) sys_param(dcontext, 0),
                           (kernel_sigset_t *) sys_param(dcontext, 1),
                           (kernel_sigset_t *) sys_param(dcontext, 2),
                           (size_t) sys_param(dcontext, 3));
        break;
    }
    case SYS_rt_sigsuspend: { /* 179 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage int
           sys_rt_sigsuspend(sigset_t *unewset, size_t sigsetsize)
         */
        handle_sigsuspend(dcontext, (kernel_sigset_t *) sys_param(dcontext, 0),
                          (size_t) sys_param(dcontext, 1));
        break;
    }
    case SYS_kill: {           /* 37 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_kill(int pid, int sig)
         */
        pid_t pid = (pid_t) sys_param(dcontext, 0);
        uint sig = (uint) sys_param(dcontext, 1);
        /* We check whether targeting this process or this process group */
        if (pid == get_process_id() || pid == 0 || pid == -get_process_group_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#if defined(SYS_tkill)
    case SYS_tkill: {          /* 238 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_tkill(int pid, int sig)
         */
        pid_t tid = (pid_t) sys_param(dcontext, 0);
        uint sig = (uint) sys_param(dcontext, 1);
        if (tid == get_thread_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#endif
#if defined(SYS_tgkill)
    case SYS_tgkill: {          /* 270 */
        /* in /usr/src/linux/kernel/signal.c:
         * asmlinkage long sys_tgkill(int tgid, int pid, int sig)
         */
        pid_t tgid = (pid_t) sys_param(dcontext, 0);
        pid_t tid = (pid_t) sys_param(dcontext, 1);
        uint sig = (uint) sys_param(dcontext, 2);
        /* some kernels support -1 values:
         +   tgkill(-1, tid, sig)  == tkill(tid, sig)
         *   tgkill(tgid, -1, sig) == kill(tgid, sig)
         * the 2nd was proposed but is not in 2.6.20 so I'm ignoring it, since
         * I don't want to kill the thread when the signal is never sent!
         * FIXME: the 1st is in my tkill manpage, but not my 2.6.20 kernel sources!
         */
        if ((tgid == -1 || tgid == get_process_id()) &&
            tid == get_thread_id()) {
            handle_self_signal(dcontext, sig);
        }
        break;
    }
#endif
    case SYS_setitimer:       /* 104 */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        dcontext->sys_param2 = sys_param(dcontext, 2);
        handle_pre_setitimer(dcontext, (int) sys_param(dcontext, 0),
                             (const struct itimerval *) sys_param(dcontext, 1),
                             (struct itimerval *) sys_param(dcontext, 2));
        break;
    case SYS_getitimer:      /* 105 */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        break;

#if 0
# ifndef X64
    case SYS_signal: {         /* 48 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage unsigned long
           sys_signal(int sig, __sighandler_t handler)
         */
        break;
    }
    case SYS_sigaction: {      /* 67 */
        /* in /usr/src/linux/arch/i386/kernel/signal.c:
           asmlinkage int 
           sys_sigaction(int sig, const struct old_sigaction *act,
                         struct old_sigaction *oact)
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
# endif
#else
    /* until we've implemented them, keep down here to get warning: */
# ifndef X64
    case SYS_signal:
    case SYS_sigaction:
    case SYS_sigsuspend:
    case SYS_sigprocmask:
# endif
#endif

#ifndef X64
    case SYS_sigpending:      /* 73 */
    case SYS_sgetmask:        /* 68 */
    case SYS_ssetmask:        /* 69 */
#endif
    case SYS_rt_sigpending:   /* 176 */
    case SYS_rt_sigtimedwait: /* 177 */
    case SYS_rt_sigqueueinfo: { /* 178 */
        /* FIXME: handle all of these syscalls! */
        LOG(THREAD, LOG_ASYNCH|LOG_SYSCALLS, 1,
            "WARNING: unhandled signal system call %d\n", mc->xax);
        break;
    }

    /****************************************************************************/
    /* FILES */
    /* prevent app from closing our files or opening a new file in our fd space.
     * it's not worth monitoring all syscalls that take in fds from affecting ours.
     */
 
    case SYS_close: {
        execute_syscall = handle_close_pre(dcontext);
        break;
    }

    case SYS_dup2:
    case SYS_dup3: {
        file_t newfd = (file_t) sys_param(dcontext, 1);
        if (fd_is_dr_owned(newfd) || fd_is_in_private_range(newfd)) {
            SYSLOG_INTERNAL_WARNING_ONCE("app trying to dup-close DR file(s)");
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "WARNING: app trying to dup2/dup3 to %d.  Disallowing.\n", newfd);
            SET_RETURN_VAL(dcontext, -EBADF);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }

    case SYS_fcntl: {
        int cmd = (int) sys_param(dcontext, 1);
        long arg = (long) sys_param(dcontext, 2);
        /* we only check for asking for min in private space: not min below
         * but actual will be above (see notes in os_file_init())
         */
        if ((cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) && fd_is_in_private_range(arg)) {
            SYSLOG_INTERNAL_WARNING_ONCE("app trying to open private fd(s)");
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "WARNING: app trying to dup to >= %d.  Disallowing.\n", arg);
            SET_RETURN_VAL(dcontext, -EINVAL);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }

    case SYS_getrlimit: {
        /* save for post */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        break;
    }

    case SYS_setrlimit: {
        int resource = (int) sys_param(dcontext, 0);
        if (resource == RLIMIT_NOFILE && DYNAMO_OPTION(steal_fds) > 0) {
            /* don't let app change limits as that would mess up our fd space */
            SET_RETURN_VAL(dcontext, -EPERM);
            DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
            execute_syscall = false;
        }
        break;
    }

    /* i#107 syscalls that might change/query app's segment */
# ifdef X64
    case SYS_arch_prctl: {
        /* we handle arch_prctl in post_syscall */
        dcontext->sys_param0 = sys_param(dcontext, 0);
        dcontext->sys_param1 = sys_param(dcontext, 1);
        break;
    }
# endif
    case SYS_set_thread_area: {
        our_modify_ldt_t desc;
        if (INTERNAL_OPTION(mangle_app_seg) && 
            safe_read((void *)sys_param(dcontext, 0), 
                      sizeof(desc), &desc)) {
            if (os_set_app_thread_area(dcontext, &desc) &&
                safe_write_ex((void *)sys_param(dcontext, 0), 
                              sizeof(desc), &desc, NULL)) {
                /* check if the range is unlimited */
                ASSERT_CURIOSITY(desc.limit == 0xfffff);
                execute_syscall = false;
                SET_RETURN_VAL(dcontext, 0);
            }
        }
        break;
    }
    case SYS_get_thread_area: {
        our_modify_ldt_t desc;
        if (INTERNAL_OPTION(mangle_app_seg) && 
            safe_read((const void *)sys_param(dcontext, 0), 
                      sizeof(desc), &desc)) {
            if (os_get_app_thread_area(dcontext, &desc) &&
                safe_write_ex((void *)sys_param(dcontext, 0), 
                              sizeof(desc), &desc, NULL)) {
                execute_syscall = false;
                SET_RETURN_VAL(dcontext, 0);
            }
        }
        break;
    }

#ifdef DEBUG
    case SYS_open: {
        dcontext->sys_param0 = sys_param(dcontext, 0);
        break;
    }
#endif

    default: {
#ifdef VMX86_SERVER
        if (is_vmkuw_sysnum(mc->xax)) {
            execute_syscall = vmkuw_pre_system_call(dcontext);
            break;
        }
#endif
    }

    } /* end switch */

    dcontext->whereami = old_whereami;
    return execute_syscall;
}

/* vmvector callbacks */
static void
allmem_info_free(void *data)
{
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, data, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
}

static void *
allmem_info_dup(void *data)
{
    allmem_info_t *src = (allmem_info_t *) data;
    allmem_info_t *dst = 
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
    ASSERT(src != NULL);
    *dst = *src;
    return dst;
}

static bool
allmem_should_merge(bool adjacent, void *data1, void *data2)
{
    allmem_info_t *i1 = (allmem_info_t *) data1;
    allmem_info_t *i2 = (allmem_info_t *) data2;
    /* we do want to merge identical regions, whether overlapping or
     * adjacent, to avoid continual splitting due to mprotect
     * fragmenting our list
     */
    return (i1->prot == i2->prot &&
            i1->type == i2->type &&
            i1->shareable == i2->shareable);
}

static void *
allmem_info_merge(void *dst_data, void *src_data)
{
    DOCHECK(1, {
      allmem_info_t *src = (allmem_info_t *) src_data;
      allmem_info_t *dst = (allmem_info_t *) dst_data;
      ASSERT(src->prot == dst->prot &&
             src->type == dst->type &&
             src->shareable == dst->shareable);
    });
    allmem_info_free(src_data);
    return dst_data;
}

static void
sync_all_memory_areas(void)
{
    /* The all_memory_areas list has the same circular dependence issues
     * as the dynamo_areas list.  For allocs outside of vmheap we can
     * be out of sync.
     */
    if (are_dynamo_vm_areas_stale()) {
        /* Trigger a sync */
        dynamo_vm_area_overlap((app_pc)NULL, (app_pc)1);
    }
}

/* caller should call sync_all_memory_areas first */
static void
add_all_memory_area(app_pc start, app_pc end, uint prot, int type, bool shareable)
{
    allmem_info_t *info;
    ASSERT(ALIGNED(start, PAGE_SIZE));
    ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
    LOG(GLOBAL, LOG_VMAREAS|LOG_SYSCALLS, 3,
        "update_all_memory_areas: adding: "PFX"-"PFX" prot=%d type=%d share=%d\n",
        start, end, prot, type, shareable);
    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
    info->prot = prot;
    ASSERT(type >= 0);
    info->type = type;
    info->shareable = shareable;
    vmvector_add(all_memory_areas, start, end, (void *)info);
}

void
update_all_memory_areas(app_pc start, app_pc end_in, uint prot, int type)
{
    allmem_info_t *info;
    app_pc end = (app_pc) ALIGN_FORWARD(end_in, PAGE_SIZE);
    bool removed;
    ASSERT(ALIGNED(start, PAGE_SIZE));
    /* all_memory_areas lock is held higher up the call chain to avoid rank
     * order violation with heap_unit_lock */
    ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
    sync_all_memory_areas();
    LOG(GLOBAL, LOG_VMAREAS, 4,
        "update_all_memory_areas "PFX"-"PFX" %d %d\n",
        start, end_in, prot, type);
    DOLOG(5, LOG_VMAREAS, print_all_memory_areas(GLOBAL););

    if (type == -1) {
        /* to preserve existing types we must iterate b/c we cannot
         * merge images into data
         */
        app_pc pc, sub_start, sub_end, next_add = start;
        pc = start;
        while (pc < end && pc >= start/*overflow*/ &&
               vmvector_lookup_data(all_memory_areas, pc, &sub_start, &sub_end,
                                    (void **) &info)) {
            if (info->type == DR_MEMTYPE_IMAGE) {
                bool shareable = false;
                app_pc overlap_end;
                dr_mem_type_t info_type = info->type;
                /* process prior to image */
                if (next_add < sub_start) {
                    vmvector_remove(all_memory_areas, next_add, pc);
                    add_all_memory_area(next_add, pc, prot, DR_MEMTYPE_DATA, false);
                }
                next_add = sub_end;
                /* change image prot */
                overlap_end = (sub_end > end) ? end : sub_end;
                if (sub_start == pc && sub_end == overlap_end) {
                    /* XXX: we should read maps to fully handle COW but for
                     * now we do some simple checks to prevent merging
                     * private with shareable regions
                     */
                    /* We assume a writable transition is accompanied by an actual
                     * write => COW => no longer shareable (i#669)
                     */
                    shareable = info->shareable;
                    if (TEST(MEMPROT_WRITE, prot) != TEST(MEMPROT_WRITE, info->prot))
                        shareable = false;
                    /* re-add so we can merge w/ adjacent non-shareable */
                } else {
                    /* assume we're here b/c was written and now marked +rx or sthg
                     * so no sharing
                     */
                    shareable = false;
                }
                vmvector_remove(all_memory_areas, pc, overlap_end);
                add_all_memory_area(pc, overlap_end, prot, info_type, shareable);
            }
            pc = sub_end;
        }
        /* process after last image */
        if (next_add < end) {
            vmvector_remove(all_memory_areas, next_add, end);
            add_all_memory_area(next_add, end, prot, DR_MEMTYPE_DATA, false);
        }
    } else {
        if (vmvector_overlap(all_memory_areas, start, end)) {
            LOG(THREAD_GET, LOG_VMAREAS|LOG_SYSCALLS, 4,
                "update_all_memory_areas: overlap found, removing and adding: "
                PFX"-"PFX" prot=%d\n", start, end, prot);
            /* New region to be added overlaps with one or more existing
             * regions.  Split already existing region(s) accordingly and add
             * the new region */
            removed = vmvector_remove(all_memory_areas, start, end);
            ASSERT(removed);
        }
        add_all_memory_area(start, end, prot, type, type == DR_MEMTYPE_IMAGE);
    }
    LOG(GLOBAL, LOG_VMAREAS, 5,
        "update_all_memory_areas "PFX"-"PFX" %d %d: post:\n",
        start, end_in, prot, type);
    DOLOG(5, LOG_VMAREAS, print_all_memory_areas(GLOBAL););
}

bool
remove_from_all_memory_areas(app_pc start, app_pc end)
{
    bool ok;
    DEBUG_DECLARE(dcontext_t *dcontext = get_thread_private_dcontext());
    ok = vmvector_remove(all_memory_areas, start, end);
    ASSERT(ok);
    LOG(THREAD, LOG_VMAREAS|LOG_SYSCALLS, 3,
        "remove_from_all_memory_areas: removed: "PFX"-"PFX"\n", start, end);
    return ok;
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
        LOG(GLOBAL, LOG_VMAREAS, 2, "%s mmap overlapping module area : \n"
            "\tmap : base="PFX" base+size="PFX" inode="UINT64_FORMAT_STRING"\n"
            "\tmod : start="PFX" end="PFX" inode="UINT64_FORMAT_STRING"\n",
            at_map ? "new" : "existing", base, base+size, inode,
            ma->start, ma->end, ma->names.inode);
        ASSERT_CURIOSITY(base >= ma->start);
        if (at_map) {
            ASSERT_CURIOSITY(base+size <= ma->end);
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
            ASSERT_CURIOSITY(inode == 0 /*see above comment*/|| base+size <= ma->end);
        }
        ASSERT_CURIOSITY(ma->names.inode == inode || inode == 0 /* for .bss */);
        DOCHECK(1, {
            if (readable && is_elf_so_header(base, size)) {
                /* Case 8879: For really small modules, to save disk space, the same
                 * disk page could hold both RO and .data, occupying just 1 page of
                 * disk space, e.g. /usr/lib/httpd/modules/mod_auth_anon.so.  When
                 * such a module is mapped in, the os maps the same disk page twice,
                 * one readonly and one copy-on-write (see pg.  96, Sec 4.4 from
                 * Linkers and Loaders by John R.  Levine).  This makes the data
                 * section also satisfy the elf_header check above.  So, if the new
                 * mmap overlaps an elf_area and it is also a header, then make sure
                 * the previous page (correcting for alignment) is also a elf_header.
                 * Note, if it is a header of a different module, then we'll not have
                 * an overlap, so we will not hit this case.
                 */
                ASSERT_CURIOSITY(ma->start + ma->os_data.alignment == base);
            }
        });
    }
    os_get_module_info_unlock();
    return ma != NULL;
}

/* All processing for mmap and mmap2. */
static void
process_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot,
             uint flags _IF_DEBUG(const char *map_type))
{
    bool image = false;
    uint memprot = osprot_to_memprot(prot);
    app_pc area_start;
    app_pc area_end;
    allmem_info_t *info;
#ifdef CLIENT_INTERFACE
    bool inform_client = false;
#endif

    LOG(THREAD, LOG_SYSCALLS, 4, "process_mmap("PFX","PFX",%s,%s)\n",
        base, size, memprot_string(memprot), map_type);
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
    /* FIXME - get inode for check */
    if (TEST(MAP_ANONYMOUS, flags)) {
        /* not an ELF mmap */
    } else if (mmap_check_for_module_overlap(base, size,
                                             TEST(MEMPROT_READ, memprot), 0, true)) {
        /* FIXME - how can we distinguish between the loader mapping the segments
         * over the initial map from someone just mapping over part of a module? If
         * is the latter case need to adjust the view size or remove from module list. */
        image = true;
        DODEBUG({ map_type = "ELF SO"; });
    } else if (TEST(MEMPROT_READ, memprot) &&
               /* i#727: We can still get SIGBUS on mmap'ed files that can't be
                * read, so pass size=0 to use a safe_read.
                */
               is_elf_so_header(base, 0)) {
        maps_iter_t iter;
        bool found_map = false;;
        uint64 inode = 0;
        const char *filename = "";
        LOG(THREAD, LOG_SYSCALLS|LOG_VMAREAS, 2, "dlopen "PFX"-"PFX"%s\n",
            base, base+size, TEST(MEMPROT_EXEC, memprot) ? " +x": "");
        image = true;
        DODEBUG({ map_type = "ELF SO"; });
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
        maps_iterator_start(&iter, true /* plan to alloc a module_area_t */);
        while (maps_iterator_next(&iter)) {
            if (iter.vm_start == base) {
                ASSERT_CURIOSITY(iter.inode != 0);
                ASSERT_CURIOSITY(iter.offset == 0); /* first map shouldn't have offset */
                /* XREF 307599 on rounding module end to the next PAGE boundary */
                ASSERT_CURIOSITY(iter.vm_end - iter.vm_start ==
                                 ALIGN_FORWARD(size, PAGE_SIZE));
                inode = iter.inode;
                filename = dr_strdup(iter.comment HEAPACCT(ACCT_OTHER));
                found_map = true;
                break;
            }
        }
        maps_iterator_stop(&iter);
#ifdef HAVE_PROC_MAPS
        ASSERT_CURIOSITY(found_map); /* barring weird races we should find this map */
#else /* HAVE_PROC_MAPS */
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
         * Once PR 235433 is implemented in visor then fix maps_iterator*() to
         * use vsi to find out page protection info, file name & inode.
         */
#endif /* HAVE_PROC_MAPS */
        /* XREF 307599 on rounding module end to the next PAGE boundary */
        module_list_add(base, ALIGN_FORWARD(size, PAGE_SIZE), true, filename, inode);
#ifdef CLIENT_INTERFACE
        inform_client = true;
#endif
        if (found_map)
            dr_strfree(filename HEAPACCT(ACCT_OTHER));
    }

    all_memory_areas_lock();
    sync_all_memory_areas();
    if (vmvector_lookup_data(all_memory_areas, base, &area_start, &area_end,
                             (void **) &info)) {
        uint new_memprot;
        LOG(THREAD, LOG_SYSCALLS, 4, "\tprocess overlap w/"PFX"-"PFX" prot=%d\n",
            area_start, area_end, info->prot);
        /* can't hold lock across call to app_memory_protection_change */
        all_memory_areas_unlock();
        if (info->prot != memprot) {
            DEBUG_DECLARE(uint res =)
                app_memory_protection_change(dcontext, base, size, memprot, 
                                             &new_memprot, NULL);
            ASSERT_NOT_IMPLEMENTED(res != PRETEND_APP_MEM_PROT_CHANGE &&
                                   res != SUBSET_APP_MEM_PROT_CHANGE);
                   
        }
        all_memory_areas_lock();
    }
    update_all_memory_areas(base, base + size, memprot,
                            image ? DR_MEMTYPE_IMAGE : DR_MEMTYPE_DATA);
    all_memory_areas_unlock();

    /* app_memory_allocation() expects to not see an overlap -- exec areas
     * doesn't expect one. We have yet to see a +x mmap into a previously
     * mapped +x region so this may not be an issue.
     */
    LOG(THREAD, LOG_SYSCALLS, 4, "\t try app_mem_alloc\n");
    if (app_memory_allocation(dcontext, base, size, memprot, image _IF_DEBUG(map_type)))
        STATS_INC(num_app_code_modules);
    LOG(THREAD, LOG_SYSCALLS, 4, "\t app_mem_alloc -- DONE\n");

#ifdef CLIENT_INTERFACE
    /* invoke the client event only after DR's state is consistent */
    if (inform_client && dynamo_initialized)
        instrument_module_load_trigger(base);
#endif
}

#ifdef X64
void
os_set_dr_seg(dcontext_t *dcontext, reg_id_t seg)
{
    int res;
    os_thread_data_t *ostd = dcontext->os_field;
    res = dynamorio_syscall(SYS_arch_prctl, 2, 
                            seg == SEG_GS ? ARCH_SET_GS : ARCH_SET_FS,
                            seg == SEG_GS ? 
                            ostd->dr_gs_base : ostd->dr_fs_base);
    ASSERT(res >= 0);
}
#endif

#ifdef X64
static void
handle_post_arch_prctl(dcontext_t *dcontext, int code, reg_t base)
{
    /* XXX: we can move it to pre_system_call to avoid system call. */
    /* i#107 syscalls that might change/query app's segment */
    os_local_state_t *os_tls = get_os_tls();
    switch (code) {
    case ARCH_SET_FS: {
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            os_thread_data_t *ostd;
            our_modify_ldt_t *desc;
            /* update new value set by app */
            os_tls->app_fs = read_selector(SEG_FS);
            os_tls->app_fs_base = (void *) base;
            /* update the app_thread_areas */
            ostd = (os_thread_data_t *)dcontext->os_field;
            desc = (our_modify_ldt_t *)ostd->app_thread_areas;
            desc[FS_TLS].entry_number = GDT_ENTRY_TLS_MIN_64 + FS_TLS;
            dynamorio_syscall(SYS_get_thread_area, 1, &desc[FS_TLS]);
            /* set it back to the value we are actually using. */
            os_set_dr_seg(dcontext, SEG_FS);
        }
        break;
    }
    case ARCH_GET_FS: {
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false))
            safe_write_ex((void *)base, sizeof(void *), &os_tls->app_fs_base, NULL);
        break;
    }
    case ARCH_SET_GS: {
        os_thread_data_t *ostd;
        our_modify_ldt_t *desc;
        /* update new value set by app */
        os_tls->app_gs = read_selector(SEG_GS);
        os_tls->app_gs_base = (void *) base;
        /* update the app_thread_areas */
        ostd = (os_thread_data_t *)dcontext->os_field;
        desc = ostd->app_thread_areas;
        desc[GS_TLS].entry_number = GDT_ENTRY_TLS_MIN_64 + GS_TLS;
        dynamorio_syscall(SYS_get_thread_area, 1, &desc[GS_TLS]);
        /* set the value back to the value we actually using */
        os_set_dr_seg(dcontext, SEG_GS);
        break;
    }
    case ARCH_GET_GS: {
        safe_write_ex((void*)base, sizeof(void *), &os_tls->app_gs_base, NULL);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    } /* switch (dcontext->sys_param0) */
    LOG(THREAD_GET, LOG_THREADS, 2,
        "thread %d segment change => app fs: "PFX", gs: "PFX"\n",
        get_thread_id(), os_tls->app_fs_base, os_tls->app_gs_base);
}
#endif /* X64 */

/* Returns false if system call should NOT be executed
 * Returns true if system call should go ahead
 */
/* FIXME: split out specific handlers into separate routines
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
    ptr_int_t result = (ptr_int_t) mc->xax; /* signed */
    bool success = (result >= 0);
    app_pc base;
    size_t size;
    uint prot;
    where_am_i_t old_whereami;
    DEBUG_DECLARE(bool ok;)

    RSTATS_INC(post_syscall);

    old_whereami = dcontext->whereami;
    dcontext->whereami = WHERE_SYSCALL_HANDLER;

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


    /* handle fork, try to do it early before too much logging occurs */
    if (sysnum == SYS_fork ||
        (sysnum == SYS_clone && !TEST(CLONE_VM, dcontext->sys_param0))) {
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
            /* FIXME - xref PR 246902 - dispatch runs a lot of code before
             * getting to post_system_call() is any of that going to be messed up
             * by waiting till here to fixup the child logfolder/file and tid?
             */
            dynamorio_fork_init(dcontext);

            LOG(THREAD, LOG_SYSCALLS, 1,
                "after fork-like syscall: parent is %d, child is %d\n", parent, child);
        } else {
            /* we're the parent */
            os_fork_post(dcontext, true/*parent*/);
        }
    }


    LOG(THREAD, LOG_SYSCALLS, 2,
        "post syscall: sysnum="PFX", result="PFX" (%d)\n",
        sysnum, mc->xax, (int)mc->xax);

    switch (sysnum) {

    /****************************************************************************/
    /* MEMORY REGIONS */

#ifdef DEBUG
    case SYS_open: {
        if (success) {
            /* useful for figuring out what module was loaded that then triggers
             * module.c elf curiosities
             */
            LOG(THREAD, LOG_SYSCALLS, 2, "SYS_open %s => %d\n",
                dcontext->sys_param0, (int)result);
        }
        break;
    }
#endif

#ifndef X64
    case SYS_mmap2:
#endif
    case SYS_mmap: {
        uint flags;
        DEBUG_DECLARE(const char *map_type;)
        RSTATS_INC(num_app_mmaps);
        base = (app_pc) mc->xax; /* For mmap, it's NOT arg->addr! */
        /* mmap isn't simply a user-space wrapper for mmap2. It's called
         * directly when dynamically loading an SO, i.e., dlopen(). */
        success = mmap_syscall_succeeded((app_pc)result);
        /* The syscall either failed OR the retcode is less than the
         * largest uint value of any errno and the addr returned is
         * page-aligned.
         */
        ASSERT_CURIOSITY(!success ||
                         ((app_pc)result < (app_pc)(ptr_int_t)-0x1000 &&
                          ALIGNED(base, PAGE_SIZE)));
        if (!success)
            goto exit_post_system_call;
#ifndef X64
        if (sysnum == SYS_mmap) {
            /* The syscall succeeded so the read of 'arg' should be
             * safe. */
            mmap_arg_struct_t *arg = (mmap_arg_struct_t *) dcontext->sys_param0;
            size = (size_t) arg->len;
            prot = (uint) arg->prot;
            flags = (uint) arg->flags;
            DEBUG_DECLARE(map_type = "mmap";)
        }
        else {
#endif
            size = (size_t) dcontext->sys_param1;
            prot = (uint) dcontext->sys_param2;
            flags = (uint) dcontext->sys_param3;
            DEBUG_DECLARE(map_type = IF_X64_ELSE("mmap2","mmap");)
#ifndef X64
        }
#endif
        process_mmap(dcontext, base, size, prot, flags _IF_DEBUG(map_type));
        break;
    }
    case SYS_munmap: {
        app_pc addr = (app_pc) dcontext->sys_param0;
        size_t len = (size_t) dcontext->sys_param1;
        /* We assumed in pre_system_call() that the unmap would succeed
         * and flushed fragments and removed the region from exec areas.
         * If the unmap failed, we re-add the region to exec areas.
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
        if (!success) {
            dr_mem_info_t info;
            /* must go to os to get real memory since we already removed */
            DEBUG_DECLARE(ok =)
                query_memory_ex_from_os(addr, &info);
            ASSERT(ok);
            app_memory_allocation(dcontext, addr, len, info.prot,
                                  info.type == DR_MEMTYPE_IMAGE
                                  _IF_DEBUG("failed munmap"));
            all_memory_areas_lock();
            ASSERT(!vmvector_overlap(all_memory_areas, addr, addr + len) ||
                   are_dynamo_vm_areas_stale());
            /* Re-add to the all-mems list. */
            update_all_memory_areas(addr, addr + len, info.prot, info.type);
            all_memory_areas_unlock();
        }
        break;
    }
    case SYS_mremap: {
        app_pc old_base = (app_pc) dcontext->sys_param0;
        size_t old_size = (size_t) dcontext->sys_param1;
        base = (app_pc) mc->xax;
        size = (size_t) dcontext->sys_param2;
        /* even if no shift, count as munmap plus mmap */
        RSTATS_INC(num_app_munmaps);
        RSTATS_INC(num_app_mmaps);
        success = !(result == -EINVAL ||
                    result == -EAGAIN ||
                    result == -ENOMEM ||
                    result == -EFAULT);
        /* The syscall either failed OR the retcode is less than the
         * largest uint value of any errno and the addr returned is
         * is page-aligned.
         */
        ASSERT_CURIOSITY(!success ||
                         ((app_pc)result < (app_pc)(ptr_int_t)-0x1000 &&
                          ALIGNED(base, PAGE_SIZE)));
        if (!success)
             goto exit_post_system_call;

        if (base != old_base || size < old_size) { /* take action only if
                                                    * there was a change */
            dr_mem_info_t info;
            /* fragments were shifted...don't try to fix them, just flush */
            app_memory_deallocation(dcontext, (app_pc)old_base, old_size,
                                    false /* don't own thread_initexit_lock */,
                                    false /* not image, FIXME: somewhat arbitrary */);
            /* i#173
             * use memory prot and type obtained from pre_system_call
             */
            info.prot = dcontext->sys_param3;
            info.type = dcontext->sys_param4;
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
                ASSERT(memprot == info.prot);
            });
            app_memory_allocation(dcontext, base, size, info.prot,
                                  info.type == DR_MEMTYPE_IMAGE
                                  _IF_DEBUG("mremap"));
            /* Now modify the all-mems list. */
            /* We don't expect an existing entry for the new region. */
            all_memory_areas_lock();
            
            /* i#175: overlap w/ existing regions is not an error */
            DEBUG_DECLARE(ok =)
                remove_from_all_memory_areas(old_base, old_base+old_size);
            ASSERT(ok);
            update_all_memory_areas(base, base + size, info.prot, info.type);
            all_memory_areas_unlock();
        }
        break;
    }
    case SYS_mprotect: {
        base = (app_pc) dcontext->sys_param0;
        size = dcontext->sys_param1;
        prot = dcontext->sys_param2;
#ifdef VMX86_SERVER
        /* PR 475111: workaround for PR 107872 */
        if (os_in_vmkernel_userworld() &&
            result == -EBUSY && prot == PROT_NONE) {
            result = mprotect_syscall(base, size, PROT_READ);
            SET_RETURN_VAL(dcontext, result);
            success = (result >= 0);
            LOG(THREAD, LOG_VMAREAS, 1, 
                "re-doing mprotect -EBUSY for "PFX"-"PFX" => %d\n",
                base, base + size, (int)result);
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
                "syscall: mprotect failed: "PFX"-"PFX" prot->%d\n",
                base, base+size, osprot_to_memprot(prot));
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
                                                 osprot_to_memprot(prot),
                                                 &new_memprot,
                                                 NULL);
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
                all_memory_areas_lock();
                ASSERT(vmvector_overlap(all_memory_areas, base, base + size) ||
                       /* we could synch up: instead we relax the assert if 
                        * DR areas not in allmem */
                       are_dynamo_vm_areas_stale());
                LOG(GLOBAL, LOG_VMAREAS, 3, 
                    "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
                    base, base + size, osprot_to_memprot(prot));
                update_all_memory_areas(base, base + size, memprot,
                                        -1/*type unchanged*/);
                all_memory_areas_unlock();
            }
        }
        break;
    }
    case SYS_brk: {
        /* i#91/PR 396352: need to watch SYS_brk to maintain all_memory_areas.
         * This code should work regardless of whether syscall failed
         * (if it failed, the old break will be returned).  We stored
         * the old break in sys_param1 in pre-syscall.
         */
        app_pc old_brk = (app_pc) dcontext->sys_param1;
        app_pc new_brk = (app_pc) result;
        DEBUG_DECLARE(app_pc req_brk = (app_pc) dcontext->sys_param0;);
#ifdef DEBUG
        if (DYNAMO_OPTION(early_inject) &&
            req_brk != NULL /* Ignore calls that don't increase brk. */) {
            DO_ONCE({
                ASSERT_CURIOSITY(new_brk > old_brk && "i#1004: first brk() "
                                 "allocation failed with -early_inject");
            });
        }
#endif
        /* i#851: the brk might not be page aligned */
        old_brk = (app_pc) ALIGN_FORWARD(old_brk, PAGE_SIZE);
        new_brk = (app_pc) ALIGN_FORWARD(new_brk, PAGE_SIZE);
        if (new_brk < old_brk) {
            all_memory_areas_lock();
            DEBUG_DECLARE(ok =)
                remove_from_all_memory_areas(new_brk, old_brk);
            ASSERT(ok);
            all_memory_areas_unlock();
        } else if (new_brk > old_brk) {
            allmem_info_t *info;
            uint prot;
            all_memory_areas_lock();
            sync_all_memory_areas();
            info = vmvector_lookup(all_memory_areas, old_brk - 1);
            /* If the heap hasn't been created yet (no brk syscalls), then info
             * will be NULL.  We assume the heap is RW- on creation.
             */
            prot = ((info != NULL) ? info->prot : MEMPROT_READ|MEMPROT_WRITE);
            update_all_memory_areas(old_brk, new_brk, prot, DR_MEMTYPE_DATA);
            all_memory_areas_unlock();
        }
        break;
    }

    /****************************************************************************/
    /* SPAWNING -- fork mostly handled above */

    case SYS_clone: {
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: clone returned "PFX"\n", mc->xax);
        /* We switch the lib tls segment back to dr's segment.
         * Please refer to comment on os_switch_lib_tls.
         * It is only called in parent thread.
         * The child thread's tls setup is done in os_tls_app_seg_init.
         */
        if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
            os_switch_lib_tls(dcontext, false/*to dr*/);
        }
        break;
    }

    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork returned "PFX"\n", mc->xax);
        break;
    }

    case SYS_vfork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork returned "PFX"\n", mc->xax);
        if (was_clone_thread_syscall(dcontext)) {
            /* restore xsp in parent */
            LOG(THREAD, LOG_SYSCALLS, 2,
                "vfork: restoring xsp from "PFX" to "PFX"\n",
                mc->xsp, dcontext->sys_param1);
            mc->xsp = dcontext->sys_param1;
        }
        if (mc->xax != 0) {
            /* We switch the lib tls segment back to dr's segment.
             * Please refer to comment on os_switch_lib_tls.
             * It is only called in parent thread.
             * The child thread's tls setup is done in os_tls_app_seg_init.
             */
            if (IF_CLIENT_INTERFACE_ELSE(INTERNAL_OPTION(private_loader), false)) {
                os_switch_lib_tls(dcontext, false/*to dr*/);
            }
        }
        break;
    }

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

    case SYS_rt_sigaction: {   /* 174 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigaction(int sig, const struct sigaction *act, 
             struct sigaction *oact, size_t sigsetsize)
         */
        /* FIXME i#148: Handle syscall failure. */
        int sig  = (int) dcontext->sys_param0;
        const kernel_sigaction_t *act =
            (const kernel_sigaction_t *) dcontext->sys_param1;
        kernel_sigaction_t *oact = (kernel_sigaction_t *) dcontext->sys_param2;
        size_t sigsetsize = (size_t) dcontext->sys_param3;
        if (!success)
            goto exit_post_system_call;

        handle_post_sigaction(dcontext, sig, act, oact, sigsetsize);
        break;
    }
    case SYS_rt_sigprocmask: { /* 175 */
        /* in /usr/src/linux/kernel/signal.c:
           asmlinkage long
           sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset, 
             size_t sigsetsize)
         */
        /* FIXME i#148: Handle syscall failure. */
        handle_post_sigprocmask(dcontext, (int) dcontext->sys_param0,
                                (kernel_sigset_t *) dcontext->sys_param1,
                                (kernel_sigset_t *) dcontext->sys_param2,
                                (size_t) dcontext->sys_param3);
        break;
    }

#ifndef X64
    case SYS_sigreturn:        /* 119 */
#endif
    case SYS_rt_sigreturn:     /* 173 */
        /* there is no return value: it's just the value of eax, so avoid
         * assert below
         */
        success = true;
        break;

    case SYS_setitimer:       /* 104 */
        handle_post_setitimer(dcontext, success, (int) dcontext->sys_param0,
                              (const struct itimerval *) dcontext->sys_param1,
                              (struct itimerval *) dcontext->sys_param2);
        break;
    case SYS_getitimer:      /* 105 */
        handle_post_getitimer(dcontext, success, (int) dcontext->sys_param0,
                              (struct itimerval *) dcontext->sys_param1);
        break;
#ifdef X64
    case SYS_arch_prctl: {
        if (success && INTERNAL_OPTION(mangle_app_seg))
            handle_post_arch_prctl(dcontext, 
                                   dcontext->sys_param0, 
                                   dcontext->sys_param1);
        break;
    }
#endif

    case SYS_getrlimit: {
        int resource = dcontext->sys_param0;
        if (success && resource == RLIMIT_NOFILE) {
            /* we stole some space: hide it from app */
            struct rlimit *rlim = (struct rlimit *) dcontext->sys_param1;
            safe_write_ex(&rlim->rlim_cur, sizeof(rlim->rlim_cur),
                          &app_rlimit_nofile.rlim_cur, NULL);
            safe_write_ex(&rlim->rlim_max, sizeof(rlim->rlim_max),
                          &app_rlimit_nofile.rlim_max, NULL);
        }
        break;
    }

#ifdef VMX86_SERVER
    default:
        if (is_vmkuw_sysnum(sysnum)) {
            vmkuw_post_system_call(dcontext);
            break;
        }
#endif

    } /* switch */

    DODEBUG({
        if (ignorable_system_call(sysnum)) {
            STATS_INC(post_syscall_ignorable);
        } else {
            /* Many syscalls can fail though they aren't ignored.  However, they
             * shouldn't happen without us knowing about them.  See PR 402769 
             * for SYS_close case.
             */
            if (!(success || sysnum == SYS_close ||
                  dcontext->expect_last_syscall_to_fail)) {
                LOG(THREAD, LOG_SYSCALLS, 1,
                    "Unexpected failure of non-ignorable syscall %d", sysnum);
            }
        }
    });


 exit_post_system_call:

#ifdef CLIENT_INTERFACE 
    /* The instrument_post_syscall should be called after DR finishes all
     * its operations, since DR needs to know the real syscall results, 
     * and any changes made by the client are simply to fool the app.
     * Also, dr_syscall_invoke_another() needs to set eax, which shouldn't 
     * affect the result of the 1st syscall. Xref i#1.
     */
    /* after restore of xbp so client sees it as though was sysenter */
    instrument_post_syscall(dcontext, sysnum);
#endif

    dcontext->whereami = old_whereami;
}

/***************************************************************************/
#ifdef HAVE_PROC_MAPS
/* Memory areas provided by kernel in /proc */

/* On all supported linux kernels /proc/self/maps -> /proc/$pid/maps 
 * However, what we want is /proc/$tid/maps, so we can't use "self"
 */
#define PROC_SELF_MAPS "/proc/self/maps"

/* these are defined in /usr/src/linux/fs/proc/array.c */
#define MAPS_LINE_LENGTH        4096
/* for systems with sizeof(void*) == 4: */
#define MAPS_LINE_FORMAT4         "%08lx-%08lx %s %08lx %*s "UINT64_FORMAT_STRING" %4096s"
#define MAPS_LINE_MAX4  49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
/* for systems with sizeof(void*) == 8: */
#define MAPS_LINE_FORMAT8         "%016lx-%016lx %s %016lx %*s "UINT64_FORMAT_STRING" %4096s"
#define MAPS_LINE_MAX8  73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */

#define MAPS_LINE_MAX   MAPS_LINE_MAX8

/* can't use fopen -- strategy: read into buffer, look for newlines.
 * fail if single line too large for buffer -- so size it appropriately:
 */
/* since we're called from signal handler, etc., keep our stack usage
 * low by using static bufs (it's over 4K after all).
 * FIXME: now we're using 16K right here: should we shrink?
 */
#define BUFSIZE (MAPS_LINE_LENGTH+8)
static char buf_scratch[BUFSIZE];
static char comment_buf_scratch[BUFSIZE];
/* To satisfy our two uses (inner use with memory_info_buf_lock versus
 * outer use with maps_iter_buf_lock), we have two different locks and
 * two different sets of static buffers.  This is to avoid lock
 * ordering issues: we need an inner lock for use in places like signal
 * handlers, but an outer lock when the iterator user allocates memory.
 */
static char buf_iter[BUFSIZE];
static char comment_buf_iter[BUFSIZE];
#endif /* HAVE_PROC_MAPS */

static bool
maps_iterator_start(maps_iter_t *iter, bool may_alloc)
{
#ifdef HAVE_PROC_MAPS
    char maps_name[24]; /* should only need 16 for 5-digit tid */

    /* Don't assign the local ptrs until the lock is grabbed to make
     * their relationship clear. */
    if (may_alloc) {
        mutex_lock(&maps_iter_buf_lock);
        iter->buf = (char *) &buf_iter;
        iter->comment_buffer = (char *) &comment_buf_iter;
    } else {
        mutex_lock(&memory_info_buf_lock);
        iter->buf = (char *) &buf_scratch;
        iter->comment_buffer = (char *) &comment_buf_scratch;
    }

    /* We need the maps for our thread id, not process id.
     * "/proc/self/maps" uses pid which fails if primary thread in group
     * has exited.
     */
    snprintf(maps_name, BUFFER_SIZE_ELEMENTS(maps_name),
             "/proc/%d/maps", get_thread_id());
    iter->maps = os_open(maps_name, OS_OPEN_READ);
    ASSERT(iter->maps != INVALID_FILE);
    iter->buf[BUFSIZE-1] = '\0'; /* permanently */

    iter->may_alloc = may_alloc;
    iter->newline = NULL;
    iter->bufread = 0;
    iter->vm_start = NULL;
    iter->comment = iter->comment_buffer;
    return true;
#else /* HAVE_PROC_MAPS */
    /* FIXME PR 235433: for VMX86_SERVER use VSI queries */
    return false;
#endif /* HAVE_PROC_MAPS */
}

static void
maps_iterator_stop(maps_iter_t *iter)
{
#ifdef HAVE_PROC_MAPS
    ASSERT((iter->may_alloc && OWN_MUTEX(&maps_iter_buf_lock)) ||
           (!iter->may_alloc && OWN_MUTEX(&memory_info_buf_lock)));
    os_close(iter->maps);
    if (iter->may_alloc)
        mutex_unlock(&maps_iter_buf_lock);
    else
        mutex_unlock(&memory_info_buf_lock);
#endif /* HAVE_PROC_MAPS */
}

static bool
maps_iterator_next(maps_iter_t *iter)
{
#ifdef HAVE_PROC_MAPS
    char perm[16];
    char *line;
    int len;
    app_pc prev_start = iter->vm_start;
    ASSERT((iter->may_alloc && OWN_MUTEX(&maps_iter_buf_lock)) ||
           (!iter->may_alloc && OWN_MUTEX(&memory_info_buf_lock)));
    if (iter->newline == NULL) {
        iter->bufwant = BUFSIZE-1;
        iter->bufread = os_read(iter->maps, iter->buf, iter->bufwant);
        ASSERT(iter->bufread <= iter->bufwant);
        LOG(GLOBAL, LOG_VMAREAS, 6,
            "get_memory_info_from_os: bytes read %d/want %d\n",
            iter->bufread, iter->bufwant);
        if (iter->bufread <= 0)
            return false;
        iter->buf[iter->bufread] = '\0';
        iter->newline = strchr(iter->buf, '\n');
        line = iter->buf;
    } else {
        line = iter->newline + 1;
        iter->newline = strchr(line, '\n');
        if (iter->newline == NULL) {
            /* FIXME clean up: factor out repetitive code */
            /* shift 1st part of line to start of buf, then read in rest */
            /* the memory for the processed part can be reused  */
            iter->bufwant = line - iter->buf;
            ASSERT(iter->bufwant <= iter->bufread);
            len = iter->bufread - iter->bufwant; /* what is left from last time */
            /* since strings may overlap, should use memmove, not strncpy */
            /* FIXME corner case: if len == 0, nothing to move */
            memmove(iter->buf, line, len);
            iter->bufread = os_read(iter->maps, iter->buf+len, iter->bufwant);
            ASSERT(iter->bufread <= iter->bufwant);
            if (iter->bufread <= 0)
                return false;
            iter->bufread += len; /* bufread is total in buf */
            iter->buf[iter->bufread] = '\0';
            iter->newline = strchr(iter->buf, '\n');
            line = iter->buf;
        }
    }
    LOG(GLOBAL, LOG_VMAREAS, 6, 
        "\nget_memory_info_from_os: newline=[%s]\n",
        iter->newline ? iter->newline : "(null)");

    /* buffer is big enough to hold at least one line */
    ASSERT(iter->newline != NULL);
    *iter->newline = '\0';
    LOG(GLOBAL, LOG_VMAREAS, 6, 
        "\nget_memory_info_from_os: line=[%s]\n", line);
    iter->comment_buffer[0]='\0';
    len = sscanf(line,
#ifdef IA32_ON_IA64
                 MAPS_LINE_FORMAT8, /* cross-compiling! */
#else              
                 sizeof(void*) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
#endif
                 (unsigned long*)&iter->vm_start, (unsigned long*)&iter->vm_end,
                 perm, (unsigned long*)&iter->offset, &iter->inode,
                 iter->comment_buffer);
    if (iter->vm_start == iter->vm_end) {
        /* i#366 & i#599: Merge an empty regions caused by stack guard pages
         * into the stack region if the stack region is less than one page away.
         * Otherwise skip it.  Some Linux kernels (2.6.32 has been observed)
         * have empty entries for the stack guard page.  We drop the permissions
         * on the guard page, because Linux always insists that it has rwxp
         * perms, no matter how we change the protections.  The actual stack
         * region has the perms we expect.
         * XXX: We could get more accurate info if we looked at
         * /proc/self/smaps, which has a Size: 4k line for these "empty"
         * regions.
         */
        app_pc empty_start = iter->vm_start;
        bool r;
        LOG(GLOBAL, LOG_VMAREAS, 2,
            "maps_iterator_next: skipping or merging empty region 0x%08x\n",
            iter->vm_start);
        /* don't trigger the maps-file-changed check.
         * slight risk of a race where we'll pass back earlier/overlapping
         * region: we'll live with it.
         */
        iter->vm_start = NULL;
        r = maps_iterator_next(iter);
        /* We could check to see if we're combining with the [stack] section,
         * but that doesn't work if there are multiple stacks or the stack is
         * split into multiple maps entries, so we merge any empty region within
         * one page of the next region.
         */
        if (empty_start <= iter->vm_start &&
            iter->vm_start <= empty_start + PAGE_SIZE) {
            /* Merge regions if the next region was zero or one page away. */
            iter->vm_start = empty_start;
        }
        return r;
    }
    if (iter->vm_start <= prev_start) {
        /* the maps file has expanded underneath us (presumably due to our
         * own committing while iterating): skip ahead */
        LOG(GLOBAL, LOG_VMAREAS, 2, 
            "maps_iterator_next: maps file changed: skipping 0x%08x\n", prev_start);
        iter->vm_start = prev_start;
        return maps_iterator_next(iter);
    }
    if (len<6)
        iter->comment_buffer[0]='\0';
    iter->prot = permstr_to_memprot(perm);
    return true;
#else /* HAVE_PROC_MAPS */
    /* FIXME PR 235433: for VMX86_SERVER use VSI queries */
    return false;
#endif /* HAVE_PROC_MAPS */
}

#ifndef HAVE_PROC_MAPS
/* PR 361594: os-independent alternative to /proc/maps, though this
 * relies on user libraries
 */
typedef struct _dl_iterate_data_t {
    app_pc target_addr;
    const char *target_path;
    char *path_out;
    size_t path_size;
    app_pc mod_start;
    app_pc mod_end;
} dl_iterate_data_t;

static int
dl_iterate_get_path_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    dl_iterate_data_t *iter_data = (dl_iterate_data_t *) data;
    /* info->dlpi->addr is offset from preferred so we need to calculate the
     * absolute address of the base.
     * we can calculate the absolute address of the first segment, but ELF
     * doesn't seem to guarantee that either the elf header (base of
     * file) or the program headers (info->dlpi_phdr) are later than
     * the min_vaddr, so it's a little confusing as to what would be
     * in the maps file or whatever and would thus be the base we're looking
     * to match: for now we assume the page with min_vaddr is that base.
     * If elf header, program header, and 1st segment could all be on
     * separate pages, I don't see any way to find the elf header in
     * such cases short of walking backward and looking for the magic #s.
     */
    app_pc pref_start, pref_end;
    app_pc min_vaddr =
        module_vaddr_from_prog_header((app_pc)info->dlpi_phdr, info->dlpi_phnum, NULL);
    app_pc base = info->dlpi_addr + min_vaddr;
    /* Note that dl_iterate_phdr doesn't give a name for the executable or
     * ld-linux.so presumably b/c those are mapped by the kernel so the
     * user-space loader doesn't need to know their file paths.
     */
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "dl_iterate_get_path_cb: addr="PFX" hdrs="PFX" base="PFX" name=%s\n",
        info->dlpi_addr, info->dlpi_phdr, base, info->dlpi_name);
    /* all we have is an addr somewhere in the module, so we need the end */
    if (module_walk_program_headers(base,
                                    /* FIXME: don't have view size: but
                                     * anything larger than header sizes works
                                     */
                                    PAGE_SIZE, 
                                    false, &pref_start, &pref_end, NULL, NULL)) {
        /* we're passed back start,end of preferred base */
        if ((iter_data->target_addr != NULL &&
             iter_data->target_addr >= base &&
             iter_data->target_addr < base + (pref_end - pref_start)) ||
            (iter_data->target_path != NULL &&
             /* if we're passed an ambiguous name, we return first hit.
              * if passed full path, should normally be what was used to
              * load, so should match.
              */
             strstr(info->dlpi_name, iter_data->target_path) != NULL)) {
            if (iter_data->path_size > 0) {
                /* We want just the path, not the filename */
                char *slash = strrchr(info->dlpi_name, '/');
                ASSERT_CURIOSITY(slash != NULL);
                ASSERT_CURIOSITY((slash - info->dlpi_name) < iter_data->path_size);
                strncpy(iter_data->path_out, info->dlpi_name,
                        MIN(iter_data->path_size, (slash - info->dlpi_name)));
                iter_data->path_out[iter_data->path_size] = '\0';
            }
            iter_data->mod_start = base;
            iter_data->mod_end = base + (pref_end - pref_start);
            return 1; /* done iterating */
        }
    } else {
        ASSERT_NOT_REACHED();
    }
    return 0; /* keep looking */
}
#endif

/* Finds the bounds of the library with name "name".  If "name" is NULL,
 * "start" must be non-NULL and must be an address within the library.
 * Note that we can't just walk backward and look for is_elf_so_header() b/c
 * some ELF files are mapped twice and it's not clear how to know if
 * one has hit the original header or a later header: this is why we allow
 * any address in the library.
 * The resulting start and end are the bounds of the library.
 * Return value is the number of distinct memory regions that comprise the library.
 */
/* Gets the library bounds from walking the map file (as opposed to using our cached
 * module_list) since is only used for dr and client libraries which aren't on the list.
 */
static int
get_library_bounds(const char *name, app_pc *start/*IN/OUT*/, app_pc *end/*OUT*/,
                   char *fullpath/*OPTIONAL OUT*/, size_t path_size)
{
    int count = 0;
#ifdef HAVE_PROC_MAPS
    bool found_library = false;
    char libname[MAXIMUM_PATH];
    const char *name_cmp = name;
    maps_iter_t iter;
    app_pc last_base = NULL;
    app_pc last_end = NULL;
    size_t image_size = 0;
#else
    dl_iterate_data_t iter_data;
#endif
    app_pc cur_end = NULL;
    app_pc mod_start = NULL;
    ASSERT(name != NULL || start != NULL);

#ifndef HAVE_PROC_MAPS
    /* PR 361594: os-independent alternative to /proc/maps */
    /* We don't have the base and we can't walk backwards (see comments above)
     * so we rely on dl_iterate_phdr() from glibc 2.2.4+, which will
     * also give us the path, needed for inject_library_path for execve.
     */
    iter_data.target_addr = (start == NULL) ? NULL : *start;
    iter_data.target_path = name;
    iter_data.path_out = fullpath;
    iter_data.path_size = (fullpath == NULL) ? 0 : path_size;
    DEBUG_DECLARE(int res = )
        dl_iterate_phdr(dl_iterate_get_path_cb, &iter_data);
    ASSERT(res == 1);
    mod_start = iter_data.mod_start;
    cur_end = iter_data.mod_end;
    count = 1;
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "get_library_bounds %s => "PFX"-"PFX" %s\n",
        name == NULL ? "<null>" : name, mod_start, cur_end,
        fullpath == NULL ? "<no path requested>" : fullpath);
#else
    maps_iterator_start(&iter, false/*won't alloc*/);
    libname[0] = '\0';
    while (maps_iterator_next(&iter)) {
        LOG(GLOBAL, LOG_VMAREAS, 5,"start="PFX" end="PFX" prot=%x comment=%s\n",
            iter.vm_start, iter.vm_end, iter.prot, iter.comment);

        /* Record the base of each differently-named set of entries up until
         * we find our target, when we'll clobber libpath
         */
        if (!found_library &&
            strncmp(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname)) != 0) {
            last_base = iter.vm_start;
            /* last_end is used to know what's readable beyond last_base */
            if (TEST(PROT_READ, iter.prot))
                last_end = iter.vm_end;
            else
                last_end = last_base;
            /* remember name so we can find the base of a multiply-mapped so */
            strncpy(libname, iter.comment, BUFFER_SIZE_ELEMENTS(libname));
            NULL_TERMINATE_BUFFER(libname);
        }
 
        if ((name_cmp != NULL &&
             (strstr(iter.comment, name_cmp) != NULL ||
              /* Include mid-library (non-.bss) anonymous mappings.  Our private loader
               * fills mapping holes with anonymous memory instead of a
               * MEMPROT_NONE mapping from the original file.
               * .bss, which can be merged with a subsequent +rw anon mapping,
               * is handled post-loop.
               */
              (found_library && iter.comment[0] == '\0' && image_size != 0 &&
               iter.vm_end - mod_start < image_size))) ||
            (name == NULL && *start >= iter.vm_start && *start < iter.vm_end)) {
            if (!found_library) {
                size_t mod_readable_sz;
                char *dst = (fullpath != NULL) ? fullpath : libname;
                size_t dstsz = (fullpath != NULL) ? path_size :
                    BUFFER_SIZE_ELEMENTS(libname);
                char *slash = strrchr(iter.comment, '/');
                ASSERT_CURIOSITY(slash != NULL);
                ASSERT_CURIOSITY((slash - iter.comment) < dstsz);
                /* we keep the last '/' at end */
                ++slash; 
                strncpy(dst, iter.comment, MIN(dstsz, (slash - iter.comment)));
                /* if max no null */
                dst[dstsz - 1] = '\0';
                if (name == NULL)
                    name_cmp = dst;
                found_library = true;
                /* Most library have multiple segments, and some have the
                 * ELF header repeated in a later mapping, so we can't rely
                 * on is_elf_so_header() and header walking.
                 * We use the name tracking to remember the first entry
                 * that had this name.
                 */
                if (last_base == NULL) {
                    mod_start = iter.vm_start;
                    mod_readable_sz = iter.vm_end - iter.vm_start;
                } else {
                    mod_start = last_base;
                    mod_readable_sz = last_end - last_base;
                }
                if (is_elf_so_header(mod_start, mod_readable_sz)) {
                    app_pc mod_base, mod_end;
                    if (module_walk_program_headers(mod_start, mod_readable_sz, false,
                                                    &mod_base, &mod_end, NULL, NULL)) {
                        image_size = mod_end - mod_base;
                        ASSERT_CURIOSITY(image_size != 0);
                    } else {
                        ASSERT_NOT_REACHED();
                    }
                } else {
                    ASSERT(false && "expected elf header");
                }
            }
            count++;
            cur_end = iter.vm_end;
#if 0
            /* FIXME: this is for libdynamorio.so only: */
            if (TESTANY(SELFPROT_ANY_DATA_SECTION, dynamo_options.protect_mask) &&
                TESTALL(MEMPROT_READ|MEMPROT_WRITE, iter.prot)) {
                /* FIXME: PR 212458/212459: we could get entire rw region here and
                 * subtract out .data, .fspdata, .cspdata, and .nspdata,
                 * but better to explicitly add .got, etc., since we should
                 * examine each rw section and decide on frequency of unprotection.
                 */
            }
#endif
        } else if (found_library) {
            /* hit non-matching, we expect module segments to be adjacent */
            break;
        }
    }

    /* Xref PR 208443: .bss sections are anonymous (no file name listed in
     * maps file), but not every library has one.  We have to parse the ELF
     * header to know since we can't assume that a subsequent anonymous
     * region is .bss. */
    if (image_size != 0 && cur_end - mod_start < image_size) {
        /* Found a .bss section. Check current mapping (note might only be
         * part of the mapping (due to os region merging? FIXME investigate). */
        ASSERT_CURIOSITY(iter.vm_start == cur_end /* no gaps, FIXME might there be
                                                   * a gap if the file has large
                                                   * alignment and no data section?
                                                   * curiosity for now*/);
        ASSERT_CURIOSITY(iter.inode == 0); /* .bss is anonymous */
        ASSERT_CURIOSITY(iter.vm_end - mod_start >= image_size);/* should be big enough */
        count++;
        cur_end = mod_start + image_size;
    } else {
        /* Shouldn't have more mapped then the size of the module, unless it's a
         * second adjacent separate map of the same file.  Curiosity for now. */
        ASSERT_CURIOSITY(image_size == 0 || cur_end - mod_start == image_size);
    }
    maps_iterator_stop(&iter);
#endif

    if (start != NULL)
        *start = mod_start;
    if (end != NULL)
        *end = cur_end;
    return count;
}

/* initializes dynamorio library bounds.
 * does not use any heap.
 * assumed to be called prior to find_executable_vm_areas.
 */
static int
get_dynamo_library_bounds(void)
{
    /* Note that we're not counting DYNAMORIO_PRELOAD_NAME as a DR area, to match
     * Windows, so we should unload it like we do there. The other reason not to
     * count it is so is_in_dynamo_dll() can be the only exception to the
     * never-execute-from-DR-areas list rule
     */
    int res;
    app_pc check_start, check_end;
    char *libdir;
    const char *dynamorio_libname;
#ifdef STATIC_LIBRARY
    /* We don't know our image name, so look up our bounds with an internal
     * address.
     */
    dynamorio_libname = NULL;
    check_start = (app_pc)&get_dynamo_library_bounds;
#else /* !STATIC_LIBRARY */
    /* PR 361594: we get our bounds from linker-provided symbols.
     * Note that referencing the value of these symbols will crash:
     * always use the address only.
     */
    extern int dynamorio_so_start, dynamorio_so_end;
    dynamo_dll_start = (app_pc) &dynamorio_so_start;
    dynamo_dll_end = (app_pc) ALIGN_FORWARD(&dynamorio_so_end, PAGE_SIZE);
# ifndef HAVE_PROC_MAPS
    check_start = dynamo_dll_start;
# endif
    dynamorio_libname = IF_UNIT_TEST_ELSE(UNIT_TEST_EXE_NAME,DYNAMORIO_LIBRARY_NAME);
#endif /* STATIC_LIBRARY */
    res = get_library_bounds(dynamorio_libname,
                             &check_start, &check_end,
                             dynamorio_library_path,
                             BUFFER_SIZE_ELEMENTS(dynamorio_library_path));
    LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME" library path: %s\n",
        dynamorio_library_path);
#ifndef STATIC_LIBRARY
    ASSERT(check_start == dynamo_dll_start && check_end == dynamo_dll_end);
#else
    dynamo_dll_start = check_start;
    dynamo_dll_end   = check_end;
#endif
    LOG(GLOBAL, LOG_VMAREAS, 1, "DR library bounds: "PFX" to "PFX"\n",
        dynamo_dll_start, dynamo_dll_end);
    ASSERT(res > 0);

    /* Issue 20: we need the path to the alt arch */
    strncpy(dynamorio_alt_arch_path, dynamorio_library_path,
            BUFFER_SIZE_ELEMENTS(dynamorio_alt_arch_path));
    /* Assumption: libdir name is not repeated elsewhere in path */
    libdir = strstr(dynamorio_alt_arch_path, IF_X64_ELSE(DR_LIBDIR_X64, DR_LIBDIR_X86));
    if (libdir != NULL) {
        const char *newdir = IF_X64_ELSE(DR_LIBDIR_X86, DR_LIBDIR_X64);
        /* do NOT place the NULL */
        strncpy(libdir, newdir, strlen(newdir));
    } else {
        SYSLOG_INTERNAL_WARNING("unable to determine lib path for cross-arch execve");
    }
    NULL_TERMINATE_BUFFER(dynamorio_alt_arch_path);
    LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME" alt arch path: %s\n",
        dynamorio_alt_arch_path);

    return res;
}

/* get full path to our own library, (cached), used for forking and message file name */
char*
get_dynamorio_library_path(void)
{
    if (!dynamorio_library_path[0]) { /* not cached */
        get_dynamo_library_bounds();
    }
    return dynamorio_library_path;
}

#ifdef HAVE_PROC_MAPS
/* Get full path+name of executable file from /proc/self/exe.  Returns an empty
 * string on error.
 * FIXME i#47: This will return DR's path when using early injection.
 */
static char *
read_proc_self_exe(bool ignore_cache)
{
    static char exepath[MAXIMUM_PATH];
    static bool tried = false;
    if (!tried || ignore_cache) {
        tried = true;
        /* assume we have /proc/self/exe symlink: could add HAVE_PROC_EXE
         * but we have no alternative solution except assuming the first
         * /proc/self/maps entry is the executable
         */
        ssize_t res;
        DEBUG_DECLARE(int len = )
            snprintf(exepath, BUFFER_SIZE_ELEMENTS(exepath),
                     "/proc/%d/exe", get_process_id());
        ASSERT(len > 0);
        NULL_TERMINATE_BUFFER(exepath);
        /* i#960: readlink does not null terminate, so we do it. */
        res = dynamorio_syscall(SYS_readlink, 3, exepath, exepath,
                                BUFFER_SIZE_ELEMENTS(exepath)-1);
        ASSERT(res < BUFFER_SIZE_ELEMENTS(exepath));
        exepath[MAX(res, 0)] = '\0';
        NULL_TERMINATE_BUFFER(exepath);
    }
    return exepath;
}
#endif /* HAVE_PROC_MAPS */

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


/***************************************************************************/
#ifndef HAVE_PROC_MAPS
/* PR 361594: os-independent alternative to /proc/maps */

# define VSYSCALL_PAGE_SO_NAME "linux-gate.so"
/* For now we assume no OS config has user addresses above this value 
 * We just go to max 32-bit (64-bit not supported yet: want lazy probing),
 * if don't have any kind of mmap iterator
 */
# define USER_MAX 0xfffff000

/* callback for dl_iterate_phdr() for adding existing modules to our lists */
static int
dl_iterate_get_areas_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    int *count = (int *) data;
    uint i;
    /* see comments in dl_iterate_get_path_cb() */
    app_pc modend;
    app_pc min_vaddr = module_vaddr_from_prog_header((app_pc)info->dlpi_phdr,
                                                     info->dlpi_phnum, &modend);
    app_pc modbase = info->dlpi_addr + min_vaddr;
    size_t modsize = modend - min_vaddr;
    LOG(GLOBAL, LOG_VMAREAS, 2,
        "dl_iterate_get_areas_cb: addr="PFX" hdrs="PFX" base="PFX" name=%s\n",
        info->dlpi_addr, info->dlpi_phdr, modbase, info->dlpi_name);
    ASSERT(info->dlpi_phnum == module_num_program_headers(modbase));

    ASSERT(count != NULL);
    if (*count == 0) {
        /* since we don't get a name for the executable, for now we
         * assume that the first iter is the executable itself.
         * XXX: this seems to hold, but there's no guarantee: can we do better?
         */
        executable_start = modbase;
    }

# ifndef X64
    if (modsize == PAGE_SIZE && info->dlpi_name[0] == '\0') {
        /* Candidate for VDSO.  Xref PR 289138 on using AT_SYSINFO to locate. */
        /* Xref VSYSCALL_PAGE_START_HARDCODED but later linuxes randomize */
        char *soname;
        if (module_walk_program_headers(modbase, modsize, false,
                                        NULL, NULL, &soname, NULL) &&
            strncmp(soname, VSYSCALL_PAGE_SO_NAME,
                    strlen(VSYSCALL_PAGE_SO_NAME)) == 0) {
            ASSERT(!dynamo_initialized); /* .data should be +w */
            ASSERT(vsyscall_page_start == NULL);
            vsyscall_page_start = modbase;
            LOG(GLOBAL, LOG_VMAREAS, 1, "found vsyscall page @ "PFX"\n",
                vsyscall_page_start);
        }
    }
# endif
    if (modbase != vsyscall_page_start)
        module_list_add(modbase, modsize, false, info->dlpi_name, 0/*don't have inode*/);

    for (i = 0; i < info->dlpi_phnum; i++) {
        app_pc start, end;
        uint prot;
        size_t align;
        if (module_read_program_header(modbase, i, &start, &end, &prot, &align)) {
            start += info->dlpi_addr;
            end += info->dlpi_addr;
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "\tsegment %d: "PFX"-"PFX" %s align=%d\n",
                i, start, end, memprot_string(prot), align);
            start = (app_pc) ALIGN_BACKWARD(start, PAGE_SIZE);
            end = (app_pc) ALIGN_FORWARD(end, PAGE_SIZE);
            LOG(GLOBAL, LOG_VMAREAS, 4,
                "find_executable_vm_areas: adding: "PFX"-"PFX" prot=%d\n",
                start, end, prot);
            all_memory_areas_lock();
            update_all_memory_areas(start, end, prot, DR_MEMTYPE_IMAGE);
            all_memory_areas_unlock();
            if (app_memory_allocation(NULL, start, end - start, prot, true/*image*/
                                      _IF_DEBUG("ELF SO")))
                (*count)++;
        }
    }
    return 0; /* keep iterating */
}

/* helper for find_vm_areas_via_probe() and get_memory_info_from_os() 
 * returns the passed-in pc if the probe was successful; else, returns
 * where the next probe should be (to skip DR memory).
 * if the probe was successful, returns in prot the results.
 */
static app_pc
probe_address(dcontext_t *dcontext, app_pc pc_in,
              byte *our_heap_start, byte *our_heap_end, OUT uint *prot)
{
    app_pc base;
    size_t size;
    app_pc pc = (app_pc) ALIGN_BACKWARD(pc_in, PAGE_SIZE);
    ASSERT(ALIGNED(pc, PAGE_SIZE));
    ASSERT(prot != NULL);
    *prot = MEMPROT_NONE;

    /* skip our own vmheap */
    if (pc >= our_heap_start && pc < our_heap_end)
        return our_heap_end;
# ifdef STACK_GUARD_PAGE
    /* if no vmheap and we probe our own stack, the SIGSEGV handler will
     * report stack overflow as it checks that prior to handling TRY
     */
    if (is_stack_overflow(dcontext, pc))
        return pc + PAGE_SIZE;
# endif
# ifdef VMX86_SERVER
    /* Workaround for PR 380621 */
    if (is_vmkernel_addr_in_user_space(pc, &base)) {
        LOG(GLOBAL, LOG_VMAREAS, 4, "%s: skipping vmkernel region "PFX"-"PFX"\n",
            __func__, pc, base);
        return base;
    }
# endif
    /* Only for find_vm_areas_via_probe(), skip modules added by 
     * dl_iterate_get_areas_cb.  Subsequent probes are about gettting 
     * info from OS, so do the actual probe.  See PR 410907.
     */
    if (!dynamo_initialized && get_memory_info(pc, &base, &size, prot))
        return base + size;

    TRY_EXCEPT(dcontext, /* try */ {
        PROBE_READ_PC(pc);
        *prot |= MEMPROT_READ;
    }, /* except */ {
        /* nothing: just continue */
    });
    /* x86 can't be writable w/o being readable.  avoiding nested TRY though. */
    if (TEST(MEMPROT_READ, *prot)) {
        TRY_EXCEPT(dcontext, /* try */ {
            PROBE_WRITE_PC(pc);
            *prot |= MEMPROT_WRITE;
        }, /* except */ {
            /* nothing: just continue */
        });
    }

    LOG(GLOBAL, LOG_VMAREAS, 5, "%s: probe "PFX" => %s\n",
        __func__, pc, memprot_string(*prot));

    /* PR 403000 - for unaligned probes return the address passed in as arg. */
    return pc_in;
}

/* helper for find_vm_areas_via_probe() */
static inline int
probe_add_region(app_pc *last_start, uint *last_prot,
                 app_pc pc, uint prot, bool force)
{
    int count = 0;
    if (force || prot != *last_prot) {
        /* We record unreadable regions as the absence of an entry */
        if (*last_prot != MEMPROT_NONE) {
            all_memory_areas_lock();
            /* images were done separately */
            update_all_memory_areas(*last_start, pc, *last_prot, DR_MEMTYPE_DATA);
            all_memory_areas_unlock();
            if (app_memory_allocation(NULL, *last_start, pc - *last_start,
                                      *last_prot, false/*!image*/ _IF_DEBUG("")))
                count++;
        }
        *last_prot = prot;
        *last_start = pc;
    }
    return count;
}

/* non-/proc/maps version of find_executable_vm_areas() */
static int
find_vm_areas_via_probe(void)
{
    /* PR 361594: now that sigsegv handler is set up, loop&probe.
     * First, dl_iterate_phdr() to get modules, and walk their
     * segments to get internal regions: then can avoid
     * wasting time probing modules.
     *
     * TODO PR 364552:
     * Would be nice to probe lazily to avoid touching all non-module
     * pages and avoid wasting our time on faults in large empty areas
     * of the address space.  This is especially important for 64-bit.
     * If done lazily, to avoid races, should do "lock add *page,0".
     * If that faults, then just try to read.  Note that we need
     * nested SIGSEGV support to handle probing while inside a SIGSEGV
     * handler (see PR 287309).
     *
     * Note that we have no good way (at least that's not racy, or
     * that'll work if there's no NX) to check for +x, and as such we
     * require HAVE_PROC_MAPS for PROGRAM_SHEPHERDING (also xref PR
     * 210383: NX transparency).
     *
     * Note also that we assume a "normal" segment setup: no funny
     * +x but -rw segments.
     */
    int count = 0;
    dcontext_t *dcontext = get_thread_private_dcontext();
    app_pc pc, last_start = NULL;
    uint prot, last_prot = MEMPROT_NONE;
    byte *our_heap_start, *our_heap_end;
    get_vmm_heap_bounds(&our_heap_start, &our_heap_end);

    DEBUG_DECLARE(int res = )
       dl_iterate_phdr(dl_iterate_get_areas_cb, &count);
    ASSERT(res == 0);

    ASSERT(dcontext != NULL);

#ifdef VMX86_SERVER
    /* We only need to probe inside allocated regions */
    void *iter = vmk_mmaps_iter_start();
    if (iter != NULL) { /* backward compatibility: support lack of iter */
        byte *start;
        size_t length;
        char name[MAXIMUM_PATH];
        LOG(GLOBAL, LOG_ALL, 1, "VSI mmaps:\n");
        while (vmk_mmaps_iter_next(iter, &start, &length, (int *)&prot,
                                   name, BUFFER_SIZE_ELEMENTS(name))) {
            LOG(GLOBAL, LOG_ALL, 1, "\t"PFX"-"PFX": %d %s\n",
                start, start + length, prot, name);
            ASSERT(ALIGNED(start, PAGE_SIZE));
            last_prot = MEMPROT_NONE;
            for (pc = start; pc < start + length; ) {
                prot = MEMPROT_NONE;
                app_pc next_pc =
                    probe_address(dcontext, pc, our_heap_start, our_heap_end, &prot);
                count += probe_add_region(&last_start, &last_prot, pc, prot,
                                          next_pc != pc);
                if (next_pc != pc) {
                    pc = next_pc;
                    last_prot = MEMPROT_NONE; /* ensure we add adjacent region */
                    last_start = pc;
                } else
                    pc += PAGE_SIZE;
            }
            count += probe_add_region(&last_start, &last_prot, pc, prot, true);
            last_start = pc;
        }
        vmk_mmaps_iter_stop(iter);
        return count;
    } /* else, fall back to full probing */
#else
# ifdef X64
/* no lazy probing support yet */
#  error X64 requires HAVE_PROC_MAPS: PR 364552
# endif
#endif
    ASSERT(ALIGNED(USER_MAX, PAGE_SIZE));
    for (pc = (app_pc) PAGE_SIZE; pc < (app_pc) USER_MAX; ) {
        prot = MEMPROT_NONE;
        app_pc next_pc = probe_address(dcontext, pc, our_heap_start, our_heap_end, &prot);
        count += probe_add_region(&last_start, &last_prot, pc, prot, next_pc != pc);
        if (next_pc != pc) {
            pc = next_pc;
            last_prot = MEMPROT_NONE; /* ensure we add adjacent region */
            last_start = pc;
        } else
            pc += PAGE_SIZE;
    }
    count += probe_add_region(&last_start, &last_prot, pc, prot, true);
    return count;
}

#endif /* !HAVE_PROC_MAPS */
/***************************************************************************/


/* assumed to be called after find_dynamo_library_vm_areas() */
int
find_executable_vm_areas(void)
{
    int count = 0;

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
    byte *our_heap_start, *our_heap_end;
    get_vmm_heap_bounds(&our_heap_start, &our_heap_end);
    if (our_heap_end - our_heap_start > 0) {
        all_memory_areas_lock();
        update_all_memory_areas(our_heap_start, our_heap_end, MEMPROT_NONE,
                                DR_MEMTYPE_DATA);
        all_memory_areas_unlock();
    }

#ifndef HAVE_PROC_MAPS
    count = find_vm_areas_via_probe();
#else
    maps_iter_t iter;
    maps_iterator_start(&iter, true/*may alloc*/);
    while (maps_iterator_next(&iter)) {
        bool image = false;
        size_t size = iter.vm_end - iter.vm_start;
        /* i#479, hide private module and match Windows's behavior */
        bool skip = dynamo_vm_area_overlap(iter.vm_start, iter.vm_end) &&
            !is_in_dynamo_dll(iter.vm_start) /* our own text section is ok */
            /* client lib text section is ok (xref i#487) */
            IF_CLIENT_INTERFACE(&& !is_in_client_lib(iter.vm_start));
        DEBUG_DECLARE(const char *map_type = "Private");
        /* we can't really tell what's a stack and what's not, but we rely on
         * our passing NULL preventing rwx regions from being added to executable
         * or future list, even w/ -executable_if_alloc
         */

        LOG(GLOBAL, LOG_VMAREAS, 2,
            "start="PFX" end="PFX" prot=%x comment=%s\n",
            iter.vm_start, iter.vm_end, iter.prot, iter.comment);
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
            LOG(GLOBAL, LOG_VMAREAS, 2, PFX"-"PFX" skipping: internal DR region\n",
                iter.vm_start, iter.vm_end);            
        } else if (strncmp(iter.comment, VSYSCALL_PAGE_MAPS_NAME,
                    strlen(VSYSCALL_PAGE_MAPS_NAME)) == 0
            IF_X64_ELSE(|| strncmp(iter.comment, VSYSCALL_REGION_MAPS_NAME,
                                   strlen(VSYSCALL_REGION_MAPS_NAME)) == 0,
            /* Older kernels do not label it as "[vdso]", but it is hardcoded there */
            /* 32-bit */
                        || iter.vm_start == VSYSCALL_PAGE_START_HARDCODED)) { 
# ifndef X64
            /* We assume no vsyscall page for x64; thus, checking the
             * hardcoded address shouldn't have any false positives.
             */
            ASSERT(iter.vm_end - iter.vm_start == PAGE_SIZE);
            ASSERT(!dynamo_initialized); /* .data should be +w */
            ASSERT(vsyscall_page_start == NULL);
            /* we're not considering as "image" even if part of ld.so (xref i#89) and
             * thus we aren't adjusting our code origins policies to remove the
             * vsyscall page exemption.
             */
            DODEBUG({ map_type = "VDSO"; });
            vsyscall_page_start = iter.vm_start;
            LOG(GLOBAL, LOG_VMAREAS, 1, "found vsyscall page @ "PFX" %s\n",
                vsyscall_page_start, iter.comment);
# else
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
            if (!TESTALL((PROT_READ|PROT_EXEC), iter.prot))
                iter.prot |= (PROT_READ|PROT_EXEC);
# endif
        } else if (mmap_check_for_module_overlap(iter.vm_start, size,
                                                 TEST(MEMPROT_READ, iter.prot),
                                                 iter.inode, false)) {
            /* we already added the whole image region when we hit the first map for it */
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
        } else if (TEST(MEMPROT_READ, iter.prot) &&
                   is_elf_so_header(iter.vm_start, size)) {
            size_t image_size = size;
            app_pc mod_base, mod_end;
            char *exec_match;
            bool found_exec = false;
            image = true;
            DODEBUG({ map_type = "ELF SO"; });
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "Found already mapped module first segment :\n"
                "\t"PFX"-"PFX"%s inode="UINT64_FORMAT_STRING" name=%s\n",
                iter.vm_start, iter.vm_end, TEST(MEMPROT_EXEC, iter.prot) ? " +x": "",
                iter.inode, iter.comment);
            ASSERT_CURIOSITY(iter.inode != 0); /* mapped images should have inodes */
            ASSERT_CURIOSITY(iter.offset == 0); /* first map shouldn't have offset */
            /* Get size by walking the program headers. Size includes the .bss section. */
            if (module_walk_program_headers(iter.vm_start, size, false,
                                            &mod_base, &mod_end, NULL, NULL)) {
                image_size = mod_end - mod_base;
            } else {
                ASSERT_NOT_REACHED();
            }
            LOG(GLOBAL, LOG_VMAREAS, 2,
                "Found already mapped module total module :\n"
                "\t"PFX"-"PFX" inode="UINT64_FORMAT_STRING" name=%s\n",
                iter.vm_start, iter.vm_start+image_size, iter.inode, iter.comment);

            /* look for executable */
            exec_match = get_application_name();
            if (exec_match != NULL && exec_match[0] != '\0')
                found_exec = (strcmp(iter.comment, exec_match) == 0);
            if (found_exec) {
                executable_start = iter.vm_start;
                LOG(GLOBAL, LOG_VMAREAS, 2,
                    "Found executable %s @"PFX"-"PFX" %s\n", get_application_name(),
                    iter.vm_start, iter.vm_start+image_size, iter.comment);
            }

            module_list_add(iter.vm_start, image_size, false, iter.comment, iter.inode);
        } else if (iter.inode != 0) {
            DODEBUG({ map_type = "Mapped File"; });
        }

        /* add all regions (incl. dynamo_areas and stack) to all_memory_areas */
        LOG(GLOBAL, LOG_VMAREAS, 4,
            "find_executable_vm_areas: adding: "PFX"-"PFX" prot=%d\n",
            iter.vm_start, iter.vm_end, iter.prot);
        all_memory_areas_lock();
        update_all_memory_areas(iter.vm_start, iter.vm_end, iter.prot,
                                image ? DR_MEMTYPE_IMAGE : DR_MEMTYPE_DATA);
        all_memory_areas_unlock();
    
        /* FIXME: best if we could pass every region to vmareas, but
         * it has no way of determining if this is a stack b/c we don't have
         * a dcontext at this point -- so we just don't pass the stack
         */
        if (!skip /* i#479, hide private module and match Windows's behavior */ &&
            app_memory_allocation(NULL, iter.vm_start, (iter.vm_end - iter.vm_start),
                                  iter.prot, image _IF_DEBUG(map_type))) {
            count++;
        }

    }
    maps_iterator_stop(&iter);
#endif /* HAVE_PROC_MAPS */

    LOG(GLOBAL, LOG_VMAREAS, 4, "init: all memory areas:\n");
    DOLOG(4, LOG_VMAREAS, print_all_memory_areas(GLOBAL););

    /* now that we've walked memory print all modules */
    LOG(GLOBAL, LOG_VMAREAS, 2, "Module list after memory walk\n");
    DOLOG(1, LOG_VMAREAS, { print_modules(GLOBAL, DUMP_NOT_XML); });

    STATS_ADD(num_app_code_modules, count);

    /* now that we have the modules set up, query libc */
    get_libc_errno_location(true/*force init*/);

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
                       MEMPROT_READ|MEMPROT_WRITE|MEMPROT_EXEC,
                       true /* from image */ _IF_DEBUG(dynamorio_library_path));
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
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
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
         if (DYNAMO_OPTION(use_all_memory_areas)) {
             ok = get_memory_info((app_pc)get_mcontext(dcontext)->xsp,
                                  &ostd->stack_base, &size, NULL);
         } else {
             ok = get_memory_info_from_os((app_pc)get_mcontext(dcontext)->xsp,
                                          &ostd->stack_base, &size, NULL);
         }
         ASSERT(ok);
         ostd->stack_top = ostd->stack_base + size;
         LOG(THREAD, LOG_THREADS, 1, "App stack is "PFX"-"PFX"\n",
             ostd->stack_base, ostd->stack_top);
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
    if (executable_start != NULL/*defensive*/ && reached_image_entry_yet()) {
        return INITIAL_STACK_EMPTY;
    } else {
        /* If our stack walk ends early we could have false positives, but
         * that's better than false negatives if we miss the image entry
         * or we were unable to find the executable_start
         */
        os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
        if (target_pc == ostd->stack_bottom_pc) {
            return INITIAL_STACK_BOTTOM_REACHED;
        } else {
            return INITIAL_STACK_BOTTOM_NOT_REACHED;
        }
    }
}
#endif /* RETURN_AFTER_CALL */

/* Use our cached data structures to retrieve memory info */
bool
query_memory_ex(const byte *pc, OUT dr_mem_info_t *out_info)
{
    allmem_info_t *info;
    bool found;
    app_pc start, end;
    ASSERT(out_info != NULL);
    all_memory_areas_lock();
    sync_all_memory_areas();
    if (vmvector_lookup_data(all_memory_areas, (app_pc)pc, &start, &end,
                             (void **) &info)) {
        ASSERT(info != NULL);
        out_info->base_pc = start;
        out_info->size = (end - start);
        out_info->prot = info->prot;
        out_info->type = info->type;
#ifdef HAVE_PROC_MAPS
        DOCHECK(2, {
            byte *from_os_base_pc;
            size_t from_os_size;
            uint from_os_prot;
            found = get_memory_info_from_os(pc, &from_os_base_pc, &from_os_size,
                                            &from_os_prot);
            ASSERT(found);
            /* we merge adjacent identical-prot image sections: .bss into .data,
             * DR's various data segments, etc., so that mismatch is ok.
             */
            if ((from_os_prot == info->prot ||
                 /* allow maps to have +x (PR 213256) */
                 (from_os_prot & (~MEMPROT_EXEC)) == info->prot) &&
                ((info->type == DR_MEMTYPE_IMAGE &&
                  from_os_base_pc >= start &&
                  from_os_size <= (end - start)) ||
                 (from_os_base_pc == start &&
                  from_os_size == (end - start)))) {
                /* ok.  easier to think of forward logic. */
            } else {
                /* /proc/maps could break/combine regions listed so region bounds as
                 * listed by all_memory_areas and /proc/maps won't agree.
                 * FIXME: Have seen instances where all_memory_areas lists the region as
                 * r--, where /proc/maps lists it as r-x.  Infact, all regions listed in
                 * /proc/maps are executable, even guard pages --x (see case 8821)
                 */
                /* we add the whole client lib as a single entry */
                if (IF_CLIENT_INTERFACE_ELSE(!is_in_client_lib(start) ||
                                             !is_in_client_lib(end - 1), true)) {
                    SYSLOG_INTERNAL_WARNING
                        ("get_memory_info mismatch! "
                         "(can happen if os combines entries in /proc/pid/maps)\n"
                         "\tos says: "PFX"-"PFX" prot="PFX"\n"
                         "\tcache says: "PFX"-"PFX" prot="PFX"\n",
                         from_os_base_pc, from_os_base_pc + from_os_size,
                         from_os_prot, start, end, info->prot);
                }
            }
        });
#endif
    } else {
        app_pc prev, next;
        found = vmvector_lookup_prev_next(all_memory_areas, (app_pc)pc,
                                          &prev, &next);
        ASSERT(found);
        if (prev != NULL) {
            found = vmvector_lookup_data(all_memory_areas, prev,
                                         NULL, &out_info->base_pc, NULL);
            ASSERT(found);
        } else
            out_info->base_pc = NULL;
        out_info->size = (next - out_info->base_pc);
        out_info->prot = MEMPROT_NONE;
        out_info->type = DR_MEMTYPE_FREE;
        /* It's possible there is memory here that was, say, added by
         * a client without our knowledge.  We can end up in an
         * infinite loop trying to forge a SIGSEGV in that situation
         * if executing from what we think is unreadable memory, so
         * best to check with the OS (xref PR 363811).
         */
#ifdef HAVE_PROC_MAPS
        byte *from_os_base_pc;
        size_t from_os_size;
        uint from_os_prot;
        if (get_memory_info_from_os(pc, &from_os_base_pc, &from_os_size,
                                    &from_os_prot) && 
            /* maps file shows our reserved-but-not-committed regions, which
             * are holes in all_memory_areas
             */
            from_os_prot != MEMPROT_NONE) {
            SYSLOG_INTERNAL_ERROR("all_memory_areas is missing region "PFX"-"PFX"!",
                                  from_os_base_pc, from_os_base_pc + from_os_size);
            DOLOG(4, LOG_VMAREAS, print_all_memory_areas(THREAD_GET););
            ASSERT_NOT_REACHED();
            /* be paranoid */
            out_info->base_pc = from_os_base_pc;
            out_info->size = from_os_size;
            out_info->prot = from_os_prot;
            out_info->type = DR_MEMTYPE_DATA; /* hopefully we won't miss an image */
        }
#else
        /* We now have nested probes, but currently probing sometimes calls
         * get_memory_info(), so we can't probe here unless we remove that call
         * there.
         */
#endif
    }
    all_memory_areas_unlock();
    return true;
}

/* Use our cached data structures to retrieve memory info */
bool
get_memory_info(const byte *pc, byte **base_pc, size_t *size,
                uint *prot /* OUT optional, returns MEMPROT_* value */)
{
    dr_mem_info_t info;
#ifdef CLIENT_INTERFACE
    if (is_vmm_reserved_address((byte*)pc, 1)) {
        if (!query_memory_ex_from_os(pc, &info) || info.type == DR_MEMTYPE_FREE)
            return false;
    } else
#endif
        if (!query_memory_ex(pc, &info) || info.type == DR_MEMTYPE_FREE)
            return false;
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
#ifndef HAVE_PROC_MAPS
    app_pc probe_pc, next_pc;
    app_pc start_pc = (app_pc) pc, end_pc = (app_pc) pc + PAGE_SIZE;
    byte *our_heap_start, *our_heap_end;
    uint cur_prot = MEMPROT_NONE;
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(info != NULL);
    /* don't crash if no dcontext, which happens (PR 452174) */
    if (dcontext == NULL)
        return false;
    get_vmm_heap_bounds(&our_heap_start, &our_heap_end);
    /* FIXME PR 235433: replace w/ real query to avoid all these probes */

    next_pc = probe_address(dcontext, (app_pc) pc, our_heap_start,
                            our_heap_end, &cur_prot);
    if (next_pc != pc) {
        if (pc >= our_heap_start && pc < our_heap_end) {
            /* Just making all readable for now */
            start_pc = our_heap_start;
            end_pc = our_heap_end;
            cur_prot = MEMPROT_READ;
        } else {
            /* FIXME: should iterate rest of cases */
            return false;
        }
    } else {
        for (probe_pc = (app_pc) ALIGN_BACKWARD(pc, PAGE_SIZE) - PAGE_SIZE;
             probe_pc > (app_pc) NULL; probe_pc -= PAGE_SIZE) {
            uint prot = MEMPROT_NONE;
            next_pc = probe_address(dcontext, probe_pc, our_heap_start,
                                    our_heap_end, &prot);
            if (next_pc != pc || prot != cur_prot)
                break;
        }
        start_pc = probe_pc + PAGE_SIZE;
        ASSERT(ALIGNED(USER_MAX, PAGE_SIZE));
        for (probe_pc = (app_pc) ALIGN_FORWARD(pc, PAGE_SIZE);
             probe_pc < (app_pc) USER_MAX; probe_pc += PAGE_SIZE) {
            uint prot = MEMPROT_NONE;
            next_pc = probe_address(dcontext, probe_pc, our_heap_start,
                                    our_heap_end, &prot);
            if (next_pc != pc || prot != cur_prot)
                break;
        }
        end_pc = probe_pc;
    }
    info->base_pc = start_pc;
    info->size = end_pc - start_pc;
    info->prot = cur_prot;
    if (cur_prot == MEMPROT_NONE) {
        /* FIXME: how distinguish from reserved but inaccessable?
         * Could try mprotect() and see whether it fails
         */
        info->type = DR_MEMTYPE_FREE;
        have_type = true;
    }
#else
    maps_iter_t iter;
    app_pc last_end = NULL;
    app_pc next_start = (app_pc) POINTER_MAX;
    bool found = false;
    ASSERT(info != NULL);
    maps_iterator_start(&iter, false/*won't alloc*/);
    while (maps_iterator_next(&iter)) {
        if (pc >= iter.vm_start && pc < iter.vm_end) {
            info->base_pc = iter.vm_start;
            info->size = (iter.vm_end - iter.vm_start);
            info->prot = iter.prot;
            /* On early (pre-Fedora 2) kernels the vsyscall page is listed
             * with no permissions at all in the maps file.  Here's RHEL4:
             *   ffffe000-fffff000 ---p 00000000 00:00 0
             * We return "rx" as the permissions in that case.
             */
            if (vsyscall_page_start != NULL &&
                pc >= vsyscall_page_start && pc < vsyscall_page_start+PAGE_SIZE) {
                ASSERT(iter.vm_start == vsyscall_page_start);
                ASSERT(iter.vm_end - iter.vm_start == PAGE_SIZE);
                if (iter.prot == MEMPROT_NONE) {
                    info->prot = (MEMPROT_READ|MEMPROT_EXEC);
                }
            }
            found = true;
            break;
        } else if (pc < iter.vm_start) {
            next_start = iter.vm_start;
            break;
        }
        last_end = iter.vm_end;
    }
    maps_iterator_stop(&iter);
    if (!found) {
        info->base_pc = last_end;
        info->size = (next_start - last_end);
        info->prot = MEMPROT_NONE;
        info->type = DR_MEMTYPE_FREE;
        have_type = true;
    }
#endif
    if (!have_type) {
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
            is_elf_so_header(info->base_pc, fault_handling_initialized ? 0 : info->size))
            info->type = DR_MEMTYPE_IMAGE;
        else {
            /* FIXME: won't quite match find_executable_vm_areas marking as
             * image: can be doubly-mapped so; don't want to count vdso; etc.
             */
            info->type = DR_MEMTYPE_DATA;
        }
    }
    return true;
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

/* HACK to get recursive write lock for internal and external use
 * FIXME: code blatantly copied from dynamo_vm_areas_{un}lock(); eliminate duplication!
 */
void
all_memory_areas_lock()
{
    /* ok to ask for locks or mark stale before all_memory_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(all_memory_areas != NULL ||
           get_num_threads() <= 1 /* must be only DR thread */);
    if (all_memory_areas == NULL)
        return;
    if (self_owns_write_lock(&all_memory_areas->lock)) {
        all_memory_areas_recursion++;
        /* we have a 5-deep path:
         *   global_heap_alloc | heap_create_unit | get_guarded_real_memory | 
         *   heap_low_on_memory | release_guarded_real_memory
         */
        ASSERT_CURIOSITY(all_memory_areas_recursion <= 4);
    } else
        write_lock(&all_memory_areas->lock);
}

void
all_memory_areas_unlock()
{
    /* ok to ask for locks or mark stale before all_memory_areas is allocated,
     * during heap init and before we can allocate it.  no lock needed then.
     */
    ASSERT(all_memory_areas != NULL ||
           get_num_threads() <= 1 /*must be only DR thread*/);
    if (all_memory_areas == NULL)
        return;
    if (all_memory_areas_recursion > 0) {
        ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
        all_memory_areas_recursion--;
    } else
        write_unlock(&all_memory_areas->lock);
}


/* in utils.c, exported only for our hack! */
extern void deadlock_avoidance_unlock(mutex_t *lock, bool ownable);

void
mutex_wait_contended_lock(mutex_t *lock)
{
#ifdef CLIENT_INTERFACE
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool set_client_safe_for_synch = 
                      ((dcontext != NULL) && IS_CLIENT_THREAD(dcontext) && 
                        ((mutex_t *)dcontext->client_data->client_grab_mutex == lock));
#endif
    
    /* i#96/PR 295561: use futex(2) if available */
    if (kernel_futex_support) {
        /* Try to get the lock.  If already held, it's fine to store any value
         * > LOCK_SET_STATE (we don't rely on paired incs/decs) so that
         * the next unlocker will call mutex_notify_released_lock().
         */
        ptr_int_t res;
        while (atomic_exchange_int(&lock->lock_requests, LOCK_CONTENDED_STATE) !=
               LOCK_FREE_STATE) {
#ifdef CLIENT_INTERFACE
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = true;
#endif
            /* We'll abort the wait if lock_requests has changed at all.
             * We can't have a series of changes that result in no apparent
             * change w/o someone acquiring the lock, b/c
             * mutex_notify_released_lock() sets lock_requests to LOCK_FREE_STATE.
             */
            res = futex_wait(&lock->lock_requests, LOCK_CONTENDED_STATE);
            if (res != 0 && res != -EWOULDBLOCK)
                thread_yield();
#ifdef CLIENT_INTERFACE
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = false;
#endif
            /* we don't care whether properly woken (res==0), var mismatch
             * (res==-EWOULDBLOCK), or error: regardless, someone else
             * could have acquired the lock, so we try again
             */
        }
    } else {
        /* we now have to undo our earlier request */
        atomic_dec_and_test(&lock->lock_requests);
        
        while (!mutex_trylock(lock)) {
#ifdef CLIENT_INTERFACE
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = true;
#endif
            thread_yield();
#ifdef CLIENT_INTERFACE
            if (set_client_safe_for_synch)
                dcontext->client_data->client_thread_safe_for_synch = false;
#endif
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
    if (kernel_futex_support) {
        /* Set to LOCK_FREE_STATE to avoid concurrent lock attempts from
         * resulting in a futex_wait value match w/o anyone owning the lock
         */
        lock->lock_requests = LOCK_FREE_STATE;
        /* No reason to wake multiple threads: just one */
        futex_wake(&lock->lock_requests);
    } /* else nothing to do */
}

/* read_write_lock_t implementation doesn't expect the contention path
   helpers to guarantee the lock is held (unlike mutexes) so simple
   yields are still acceptable.
*/
void
rwlock_wait_contended_writer(read_write_lock_t *rwlock)
{
    thread_yield();
}

void
rwlock_notify_writer(read_write_lock_t *rwlock)
{
    /* nothing to do here */
}

void
rwlock_wait_contended_reader(read_write_lock_t *rwlock)
{
    thread_yield();
}

void
rwlock_notify_readers(read_write_lock_t *rwlock)
{
    /* nothing to do here */
}

/***************************************************************************/

/* events are un-signaled when successfully waited upon. */
typedef struct linux_event_t {
    /* volatile int rather than volatile bool since it's used as a futex.
     * 0 is unset, 1 is set, no other value is used.
     * Any function that sets this flag must also notify possibly waiting
     * thread(s). See i#96/PR 295561.
     */
    volatile int signaled;
    mutex_t lock;
} linux_event_t;


/* FIXME: this routine will need to have a macro wrapper to let us assign different ranks to 
 * all events for DEADLOCK_AVOIDANCE.  Currently a single rank seems to work.
 */
event_t
create_event()
{
    event_t e = (event_t) global_heap_alloc(sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
    e->signaled = 0;
    ASSIGN_INIT_LOCK_FREE(e->lock, event_lock); /* FIXME: we'll need to pass the event name here */
    return e;
}

void
destroy_event(event_t e)
{
    DELETE_LOCK(e->lock);
    global_heap_free(e, sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
}

void
signal_event(event_t e)
{
    mutex_lock(&e->lock);
    e->signaled = 1;
    futex_wake(&e->signaled);
    LOG(THREAD_GET, LOG_THREADS, 3,"thread %d signalling event "PFX"\n",get_thread_id(),e);
    mutex_unlock(&e->lock);
}

void
reset_event(event_t e)
{
    mutex_lock(&e->lock);
    e->signaled = 0;
    LOG(THREAD_GET, LOG_THREADS, 3,"thread %d resetting event "PFX"\n",get_thread_id(),e);
    mutex_unlock(&e->lock);
}

/* FIXME: compare use and implementation with  man pthread_cond_wait */
/* i#96/PR 295561: use futex(2) if available. */
void
wait_for_event(event_t e)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif
    /* Use a user-space event on Linux, a kernel event on Windows. */
    LOG(THREAD, LOG_THREADS, 3, "thread %d waiting for event "PFX"\n",get_thread_id(),e);
    while (true) {
        if (e->signaled == 1) {
            mutex_lock(&e->lock);
            if (e->signaled == 0) {
                /* some other thread beat us to it */
                LOG(THREAD, LOG_THREADS, 3, "thread %d was beaten to event "PFX"\n",
                    get_thread_id(),e);
                mutex_unlock(&e->lock);
            } else {
                /* reset the event */
                e->signaled = 0;
                mutex_unlock(&e->lock);
                LOG(THREAD, LOG_THREADS, 3,
                    "thread %d finished waiting for event "PFX"\n", get_thread_id(),e);
                return;
            }
        } else {
            /* Waits only if the signaled flag is not set as 1. Return value
             * doesn't matter because the flag will be re-checked. 
             */
            futex_wait(&e->signaled, 0);
        }
        if (e->signaled == 0) {
            /* If it still has to wait, give up the cpu. */
            thread_yield();
        }
    }
}

/***************************************************************************
 * DIRECTORY ITERATOR
 */

/* These structs are written to the buf that we pass to getdents.  We can
 * iterate them by adding d_reclen to the current buffer offset and interpreting
 * that as the next entry.
 */
struct linux_dirent {
    long           d_ino;       /* Inode number. */
    off_t          d_off;       /* Offset to next linux_dirent. */
    unsigned short d_reclen;    /* Length of this linux_dirent. */
    char           d_name[];    /* File name, null-terminated. */
#if 0  /* Has to be #if 0 because it's after the flexible array. */
    char           d_pad;       /* Always zero? */
    char           d_type;      /* File type, since Linux 2.6.4. */
#endif
};

#define CURRENT_DIRENT(iter) \
        ((struct linux_dirent *)(&iter->buf[iter->off]))

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
        iter->end = dynamorio_syscall(SYS_getdents, 3, iter->fd, iter->buf,
                                      sizeof(iter->buf));
        ASSERT(iter->end <= sizeof(iter->buf));
        if (iter->end <= 0) {  /* No more dents, or error. */
            iter->name = NULL;
            if (iter->end < 0) {
                LOG(GLOBAL, LOG_SYSCALLS, 1,
                    "getdents syscall failed with errno %d\n", -iter->end);
            }
            return false;
        }
    }
    iter->name = CURRENT_DIRENT(iter)->d_name;
    return true;
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
    tids = HEAP_ARRAY_ALLOC(dcontext, thread_id_t, tids_alloced,
                            ACCT_THREAD_MGT, PROTECTED);
    task_dir = os_open_directory("/proc/self/task", OS_OPEN_READ);
    ASSERT(task_dir != INVALID_FILE);
    os_dir_iterator_start(&iter, task_dir);
    while (os_dir_iterator_next(&iter)) {
        thread_id_t tid;
        DEBUG_DECLARE(int r;)
        if (strcmp(iter.name, ".") == 0 ||
            strcmp(iter.name, "..") == 0)
            continue;
        IF_DEBUG(r =)
            sscanf(iter.name, "%u", &tid);
        ASSERT_MESSAGE(CHKLVL_ASSERTS, "failed to parse /proc/pid/task entry",
                       r == 1);
        if (tid <= 0)
            continue;
        if (num_threads == tids_alloced) {
            /* realloc, essentially.  Less expensive than counting first. */
            new_tids = HEAP_ARRAY_ALLOC(dcontext, thread_id_t, tids_alloced * 2,
                                        ACCT_THREAD_MGT, PROTECTED);
            memcpy(new_tids, tids, sizeof(thread_id_t) * tids_alloced);
            HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, tids_alloced,
                            ACCT_THREAD_MGT, PROTECTED);
            tids = new_tids;
            tids_alloced *= 2;
        }
        tids[num_threads++] = tid;
    }
    ASSERT(iter.end == 0);  /* No reading errors. */
    os_close(task_dir);

    /* realloc back down to num_threads for caller simplicity. */
    new_tids = HEAP_ARRAY_ALLOC(dcontext, thread_id_t, num_threads,
                                ACCT_THREAD_MGT, PROTECTED);
    memcpy(new_tids, tids, sizeof(thread_id_t) * num_threads);
    HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, tids_alloced,
                    ACCT_THREAD_MGT, PROTECTED);
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
    uint threads_to_signal = 0;

    mutex_lock(&thread_initexit_lock);
    CLIENT_ASSERT(thread_takeover_records == NULL,
                  "Only one thread should attempt app take over!");

    /* Find tids for which we have no thread record, meaning they are not under
     * our control.  Shift them to the beginning of the tids array.
     */
    tids = os_list_threads(dcontext, &num_threads);
    for (i = 0; i < num_threads; i++) {
        thread_record_t *tr = thread_lookup(tids[i]);
        if (tr == NULL)
            tids[threads_to_signal++] = tids[i];
    }

    if (threads_to_signal > 0) {
        takeover_record_t *records;

        /* Assuming pthreads, prepare signal_field for sharing. */
        handle_clone(dcontext, PTHREAD_CLONE_FLAGS);

        /* Create records with events for all the threads we want to signal. */
        LOG(GLOBAL, LOG_THREADS, 1,
            "TAKEOVER: publishing takeover records\n");
        records = HEAP_ARRAY_ALLOC(dcontext, takeover_record_t,
                                   threads_to_signal, ACCT_THREAD_MGT,
                                   PROTECTED);
        for (i = 0; i < threads_to_signal; i++) {
            LOG(GLOBAL, LOG_THREADS, 1,
                "TAKEOVER: will signal thread %d\n", tids[i]);
            records[i].tid = tids[i];
            records[i].event = create_event();
        }

        /* Publish the records and the initial take over dcontext. */
        thread_takeover_records = records;
        num_thread_takeover_records = threads_to_signal;
        takeover_dcontext = dcontext;

        /* Signal the other threads. */
        for (i = 0; i < threads_to_signal; i++) {
            thread_signal(get_process_id(), records[i].tid, SUSPEND_SIGNAL);
        }
        mutex_unlock(&thread_initexit_lock);

        /* Wait for all the threads we signaled. */
        ASSERT_OWN_NO_LOCKS();
        for (i = 0; i < threads_to_signal; i++) {
            wait_for_event(records[i].event);
        }

        /* Now that we've taken over the other threads, we can safely free the
         * records and reset the shared globals.
         */
        mutex_lock(&thread_initexit_lock);
        LOG(GLOBAL, LOG_THREADS, 1,
            "TAKEOVER: takeover complete, unpublishing records\n");
        thread_takeover_records = NULL;
        num_thread_takeover_records = 0;
        takeover_dcontext = NULL;
        for (i = 0; i < threads_to_signal; i++) {
            destroy_event(records[i].event);
        }
        HEAP_ARRAY_FREE(dcontext, records, takeover_record_t,
                        threads_to_signal, ACCT_THREAD_MGT, PROTECTED);
    }

    mutex_unlock(&thread_initexit_lock);
    HEAP_ARRAY_FREE(dcontext, tids, thread_id_t, num_threads,
                    ACCT_THREAD_MGT, PROTECTED);

    return threads_to_signal > 0;
}

/* Takes over the current thread from the signal handler.  We notify the thread
 * that signaled us by signalling our event in thread_takeover_records.
 */
void
os_thread_take_over(priv_mcontext_t *mc)
{
    int r;
    uint i;
    thread_id_t mytid;
    dcontext_t *dcontext;
    priv_mcontext_t *dc_mc;
    event_t event = NULL;

    LOG(GLOBAL, LOG_THREADS, 1,
        "TAKEOVER: received signal in thread %d\n", get_sys_thread_id());

    /* Do standard DR thread initialization.  Mirrors code in
     * create_clone_record and new_thread_setup, except we're not putting a
     * clone record on the dstack.
     */
    r = dynamo_thread_init(NULL, mc _IF_CLIENT_INTERFACE(false));
    ASSERT(r == SUCCESS);
    dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);
    share_siginfo_after_take_over(dcontext, takeover_dcontext);
    dynamo_thread_under_dynamo(dcontext);
    dc_mc = get_mcontext(dcontext);
    *dc_mc = *mc;
    dcontext->whereami = WHERE_APP;
    dcontext->next_tag = mc->pc;

    /* Wake up the thread that initiated the take over. */
    mytid = get_thread_id();
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

    /* Start interpreting from the signal context. */
    call_switch_stack(dcontext, dcontext->dstack, dispatch,
                      false/*not on initstack*/, false/*shouldn't return*/);
    ASSERT_NOT_REACHED();
}

/***************************************************************************/

uint
os_random_seed(void)
{
    uint seed;
    /* reading from /dev/urandom for a non-blocking random */
    int urand = os_open("/dev/urandom", OS_OPEN_READ);
    DEBUG_DECLARE(int read = )os_read(urand, &seed, sizeof(seed));
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

        ASSERT(TESTALL(MEMPROT_READ|MEMPROT_EXEC, prot)); /* code */

        if (!get_memory_info(code_end, &data_start, &data_size, &prot))
            return false;

        ASSERT(data_start == code_end);
        ASSERT(TESTALL(MEMPROT_READ|MEMPROT_WRITE, prot)); /* data */

        app_pc text_start = code_start;
        app_pc text_end = data_start + data_size;

        /* TODO: performance: should do this only in case relocation info is not present */
        DEBUG_DECLARE(uint found = )
            find_address_references(dcontext, text_start, text_end, 
                                    code_start, code_end);
        LOG(GLOBAL, LOG_RCT, 2, PFX"-"PFX" : %d ind targets of %d code size", 
            text_start, text_end,
            found, code_size);
        return true;
    }
    return false;
}

#ifdef X64
bool
rct_add_rip_rel_addr(dcontext_t *dcontext, app_pc tgt _IF_DEBUG(app_pc src))
{
    /* FIXME PR 276762: not implemented */
    return false;
}
#endif
#endif /* RCT_IND_BRANCH */

#ifdef HOT_PATCHING_INTERFACE
void*
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
hook_text(byte *hook_code_buf, const app_pc image_addr, 
          intercept_function_t hook_func, const void *callee_arg,
          const after_intercept_action_t action_after,
          const bool abort_if_hooked, const bool ignore_cti,
          byte **app_code_copy_p, byte **alt_exit_tgt_p)
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
insert_jmp_at_tramp_entry(byte *trampoline, byte *target)
{
    ASSERT_NOT_IMPLEMENTED(false);
}
#endif  /* HOT_PATCHING_INTERFACE */

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
os_current_user_directory(char *directory_prefix /* INOUT */,
                          uint directory_len,
                          bool create)
{
    /* XXX: could share some of this code w/ corresponding windows routine */
    uid_t uid = dynamorio_syscall(SYS_getuid, 0);
    char *directory = directory_prefix;
    char *dirend = directory_prefix + strlen(directory_prefix);
    snprintf(dirend, directory_len - (dirend - directory_prefix), "%cdpc-%d",
             DIRSEP, uid);
    directory_prefix[directory_len - 1] = '\0';
    if (!os_file_exists(directory, true/*is dir*/) && create) {
        /* XXX: we should ensure we do not follow symlinks */
        /* XXX: should add support for CREATE_DIR_FORCE_OWNER */
        if (!os_create_dir(directory, CREATE_DIR_REQUIRE_NEW)) {
            LOG(GLOBAL, LOG_CACHE, 2, 
                "\terror creating per-user dir %s\n", directory);
            return false;
        } else {
            LOG(GLOBAL, LOG_CACHE, 2, 
                "\tcreated per-user dir %s\n", directory);
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

bool
os_file_has_elf_so_header(const char *filename)
{
    bool result = false;
    ELF_HEADER_TYPE elf_header;
    file_t fd;

    fd = os_open(filename, OS_OPEN_READ);
    if (fd == INVALID_FILE)
        return false;
    if (os_read(fd, &elf_header, sizeof(elf_header)) == sizeof(elf_header) &&
        is_elf_so_header((app_pc)&elf_header, sizeof(elf_header)))
        result = true;
    os_close(fd);
    return result;
}

#ifndef X64
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
    uint32 divisor = (uint32) divisor64;

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
    asm ("divl %2" : "=a" (res.lo), "=d" (*remainder) :
         "rm" (divisor), "0" (res.lo), "1" (upper));
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
    return (uint64) remainder;
}
#endif /* !X64 */

#endif /* !NOT_DYNAMORIO_CORE_PROPER: around most of file, to exclude preload */

#if defined(STANDALONE_UNIT_TEST)

void
test_uint64_divmod(void)
{
#ifndef X64
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
#endif /* !X64 */
}

void
unit_test_os(void)
{
    test_uint64_divmod();
}

#endif /* STANDALONE_UNIT_TEST */
