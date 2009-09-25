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
 * os.c - Linux specific routines
 */

/* for mmap-related #defines */
#include <sys/types.h>
#include <sys/mman.h>
/* for open */
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>              /* for CLONE_* */
#include "../globals.h"
#include <string.h>
#include <unistd.h> /* for write and usleep and _exit */
#include <limits.h>
#include <sys/sysinfo.h>        /* for get_nprocs_conf */

#ifdef X86
/* must be prior to <link.h> => <elf.h> => INT*_{MIN,MAX} */
# include "instr.h" /* for get_segment_base() */
#endif

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
#include "syscall.h"            /* our own local copy */
#include "../module_shared.h"
#include "os_private.h"
#include "../synch.h"

#ifdef CLIENT_INTERFACE
# include "instrument.h"
#endif

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

#ifdef CLIENT_INTERFACE
/* Should we place this in a client header?  Currently mentioned in
 * dr_raw_tls_calloc() docs.
 */
# define MAX_NUM_CLIENT_TLS 64
static bool client_tls_allocated[MAX_NUM_CLIENT_TLS];
DECLARE_CXTSWPROT_VAR(static mutex_t client_tls_lock, INIT_LOCK_FREE(client_tls_lock));
#endif

#include <stddef.h> /* for offsetof */

#include <sys/utsname.h> /* for struct utsname */

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

/* does the kernel provide tids that must be used to distinguish threads in a group? */
static bool kernel_thread_groups;

static bool kernel_64bit;

pid_t pid_cached;

#ifdef PROFILE_RDTSC
uint kilo_hertz; /* cpu clock speed */
#endif

/* lock to guard reads from /proc/self/maps in get_memory_info_from_os(). */
static mutex_t memory_info_buf_lock = INIT_LOCK_FREE(memory_info_buf_lock);
/* lock for iterator where user needs to allocate memory */
static mutex_t maps_iter_buf_lock = INIT_LOCK_FREE(maps_iter_buf_lock);

/* Xref PR 258731, dup of STDOUT/STDERR in case app wants to close them. */
DR_API file_t our_stdout = INVALID_FILE;
DR_API file_t our_stderr = INVALID_FILE;
DR_API file_t our_stdin = INVALID_FILE;

/* Track all memory regions seen by DR. We track these ourselves to prevent
 * repeated reads of /proc/self/maps (case 3771). An allmem_info_t struct is
 * stored in the custom field.
 * all_memory_areas is updated along with dynamo_areas, due to cyclic
 * dependencies.
 */
static vm_area_vector_t *all_memory_areas;

typedef struct _allmem_info_t {
    uint prot;
    dr_mem_type_t type;
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

static void
process_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot
             _IF_DEBUG(char *map_type));

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
                   * will all agree on it's size since it seems to be defined differently
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

/* With KEEP_SYMBOLS_FOR_LIBC_BACKTRACE our __errno_location gets interpreted
 * in the code cache and on x64 we hit an assert on our own segment use
 * in get_thread_private_dcontext():
 *   "no support yet for application using non-NPTL segment"
 * FIXME: though we used to use this for the app too: presumably we don't
 * need that anymore, since having our own private copy is just as good
 * to isolated DR errno changes from app errno uses.
 */
__attribute__ ((visibility ("hidden")))
int *
__errno_location() {
    /* Each dynamo thread should have a separate errno */
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (dcontext == NULL)
        return &init_errno;
    else {
        /* WARNING: init_errno is in data segment so can be RO! */
        return &(dcontext->upcontext_ptr->errno);
    }
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
extern char **__environ;
#include <errno.h> /* for EINVAL */
/* avoid problems with use of errno as var name in rest of file */
#undef errno
#define __set_errno(val) (*__errno_location ()) = (val)
#define __get_errno() (*__errno_location())
int
our_unsetenv(const char *name)
{
    size_t len;
    char **ep;
    
    if (name == NULL || *name == '\0' || strchr (name, '=') != NULL) {
        __set_errno (EINVAL);
        return -1;
    }
    
    len = strlen (name);
    
    /* FIXME: glibc code grabs a lock here, we don't have access to that lock
     * LOCK;
     */
    
    ep = __environ;
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


#if !defined(STATIC_LIBRARY) && !defined(STANDALONE_UNIT_TEST)
/* shared library init and exit */
int
_init()
{
    /* if do not want to use drpreload.so, we can take over here */
    extern void dynamorio_app_take_over(void);
    bool takeover = false;
# ifdef INIT_TAKE_OVER
    takeover = true;
# endif
# ifdef VMX86_SERVER
    /* PR 391765: take over here instead of using preload */
    takeover = os_in_vmkernel_classic();
# endif
    if (takeover) {
        if (dynamorio_app_init() == 0 /* success */) {
            dynamorio_app_take_over();
        }
    }
    return 0;
}

int
_fini()
{
    return 0;
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
    get_uname();

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

/* we need to re-cache after a fork */
static char *
get_application_name_helper(bool ignore_cache)
{
    static char name_buf[MAXIMUM_PATH];
    
    if (!name_buf[0] || ignore_cache) {
        /* FIXME PR 363063: move getnamefrompid() here and replace /proc reliance
         * ideally w/ os-independent method, but could use VSI for ESX
         */
        extern void getnamefrompid(int pid, char *name, uint maxlen);
        getnamefrompid(get_process_id(), name_buf, BUFFER_SIZE_ELEMENTS(name_buf));
    }
    return name_buf;
}

/* get application name, (cached), used for event logging */
/* FIXME : Not yet implemented */
char *
get_application_name(void)
{
    return get_application_name_helper(false);
}

const char *
get_application_short_name()
{
    /* FIXME: NYI: need get_application_name() to have full path, here not */
    return get_application_name();
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
    FILE *cpuinfo;
    char line[CPUMHZ_LINE_LENGTH];
    ulong cpu_mhz = 1, cpu_khz = 0; 
    
    /* FIXME: use os_open; for now only called for init, so ok to use fopen */
    cpuinfo=fopen(PROC_CPUINFO,"r");
    ASSERT(cpuinfo != NULL);

    while (!feof(cpuinfo)) {
        if (fgets(line, sizeof(line), cpuinfo) == NULL)
            break;
        if (sscanf(line, CPUMHZ_LINE_FORMAT, &cpu_mhz, &cpu_khz) == 2) {
            LOG(GLOBAL, LOG_ALL, 2, "Processor speed exactly %lu.%03luMHz\n", cpu_mhz, cpu_khz);
            break;
        }
    }
    fclose(cpuinfo);
    return cpu_mhz*1000 + cpu_khz;
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

/* seconds since 1970 */
uint
query_time_seconds(void)
{
    return (uint) dynamorio_syscall(SYS_time, 1, NULL);
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
    DELETE_LOCK(memory_info_buf_lock);
    DELETE_LOCK(maps_iter_buf_lock);
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

/* clean up may be impossible - just terminate */
void
os_terminate(dcontext_t *dcontext, terminate_flags_t flags)
{
    /* FIXME: terminate type is ignored */
    ASSERT_NOT_IMPLEMENTED(flags == TERMINATE_PROCESS);
    exit_process_syscall(1);
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

#ifdef HAVE_TLS
/* GDT slot we use for set_thread_area.
 * This depends on the kernel, not on the app!
 */
static int tls_gdt_index = -1;
# define GDT_NUM_TLS_SLOTS 3
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
    memset(ldt, 0, sizeof(ldt));
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
    memset(ldt, 0, sizeof(ldt));
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
    ldt->limit_in_pages = 0;
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

#define WRITE_FS(val) \
    ASSERT(sizeof(val) == sizeof(reg_t));                           \
    asm volatile("mov %0,%%"ASM_XAX"; mov %%"ASM_XAX", %"ASM_SEG";" \
                 : : "m" ((val)) : ASM_XAX);

static uint
read_selector(reg_id_t seg)
{
    uint sel;
    /* pre-P6 family leaves upper 2 bytes undefined, so we clear them
     * we don't clear and then use movw b/c that takes an extra clock cycle
     */
    if (seg == SEG_FS) {
        asm volatile("movl %%fs, %%eax; andl $0x0000ffff, %%eax; "
                     "movl %%eax, %0" : "=m"(sel) : : "eax");
    } else if (seg == SEG_GS) {
        asm volatile("movl %%gs, %%eax; andl $0x0000ffff, %%eax; "
                     "movl %%eax, %0" : "=m"(sel) : : "eax");
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
    return sel;
}

/* FIXME: assumes that fs/gs is not already in use by app */
static bool
is_segment_register_initialized(void)
{
    return (read_selector(SEG_TLS) != 0);
}

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
#ifdef CLIENT_INTERFACE
    void *client_tls[MAX_NUM_CLIENT_TLS];
#endif
} os_local_state_t;

#define TLS_LOCAL_STATE_OFFSET (offsetof(os_local_state_t, state))

/* offset from top of page */
#define TLS_OS_LOCAL_STATE     0x00

#define TLS_SELF_OFFSET        (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, self))
#define TLS_THREAD_ID_OFFSET   (TLS_OS_LOCAL_STATE + offsetof(os_local_state_t, tid))
#define TLS_DCONTEXT_OFFSET    (TLS_OS_LOCAL_STATE + TLS_DCONTEXT_SLOT)

/* N.B.: imm and idx are ushorts!
 * imm must be a preprocessor constant
 * cannot find a gcc asm constraint that will put an immed w/o a $ in, so
 * we use the preprocessor to paste the constant in.
 * Also, var needs to match the pointer size, or else we'll get stack corruption.
 */
#define WRITE_TLS_SLOT_IMM(imm, var)                                  \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                            \
    ASSERT(sizeof(var) == sizeof(void*));                             \
    asm("mov %0, %%"ASM_XAX : : "m"((var)) : ASM_XAX);                \
    asm("mov %"ASM_XAX", "ASM_SEG":"STRINGIFY(imm) : : "m"((var)) : ASM_XAX);

#define READ_TLS_SLOT_IMM(imm, var)                  \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());           \
    ASSERT(sizeof(var) == sizeof(void*));            \
    asm("mov "ASM_SEG":"STRINGIFY(imm)", %"ASM_XAX); \
    asm("mov %%"ASM_XAX", %0" : "=m"((var)) : : ASM_XAX);

#define WRITE_TLS_SLOT(idx, var)                            \
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());                  \
    ASSERT(sizeof(var) == sizeof(void*));                   \
    asm("mov %0, %%"ASM_XAX : : "m"((var)) : ASM_XAX);      \
    asm("movzx %0, %%"ASM_XDX"" : : "m"((idx)) : ASM_XDX);  \
    asm("mov %%"ASM_XAX", %"ASM_SEG":(%%"ASM_XDX")" : : : ASM_XAX, ASM_XDX);

/* FIXME: get_thread_private_dcontext() is a bottleneck, so it would be
 * good to figure out how to easily change this to use an immediate since it is
 * known at compile time -- see comments above for the _IMM versions
 */
#define READ_TLS_SLOT(idx, var)                                    \
    ASSERT(sizeof(var) == sizeof(void*));                          \
    asm("movzx %0, %%"ASM_XAX : : "m"((idx)) : ASM_XAX);           \
    asm("mov %"ASM_SEG":(%%"ASM_XAX"), %%"ASM_XAX : : : ASM_XAX);  \
    asm("mov %%"ASM_XAX", %0" : "=m"((var)) : : ASM_XAX);

/* converts a local_state_t offset to a segment offset */
ushort
os_tls_offset(ushort tls_offs)
{
    /* no ushort truncation issues b/c TLS_LOCAL_STATE_OFFSET is 0 */
    IF_NOT_HAVE_TLS(ASSERT_NOT_REACHED());
    ASSERT(TLS_LOCAL_STATE_OFFSET == 0);
    return (TLS_LOCAL_STATE_OFFSET + tls_offs);
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
            memset(ldt, 0, sizeof(ldt));
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
            DODEBUGINT({
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
#endif

local_state_extended_t *
get_local_state_extended()
{
    os_local_state_t *os_tls;
    ushort offs = TLS_SELF_OFFSET;
    ASSERT(is_segment_register_initialized());
    READ_TLS_SLOT(offs, os_tls);
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

void
os_tls_init()
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
    ASSERT(proc_is_cache_aligned(os_tls->self + TLS_LOCAL_STATE_OFFSET));
    /* Verify that local_state_extended_t should indeed be used. */
    ASSERT(DYNAMO_OPTION(ibl_table_in_tls));

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
        if (cur_gs == NULL || is_dynamo_address(cur_gs)) {
            res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, segment);
            if (res >= 0) {
                os_tls->tls_type = TLS_TYPE_ARCH_PRCTL;
                LOG(GLOBAL, LOG_THREADS, 1,
                    "os_tls_init: arch_prctl successful for base "PFX"\n", segment);
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
        /* Base here must be 32-bit */
        IF_X64(ASSERT(DYNAMO_OPTION(heap_in_lower_4GB) && segment <= (byte*)UINT_MAX));
        if (!dynamo_initialized) {
            /* We don't want to break the assumptions of pthreads or wine,
             * so we try to take the last slot.  We don't want to hardcode
             * the index b/c the kernel will let us clobber entries so we want
             * to only pass in -1.
             */
            int i;
            int avail_index[GDT_NUM_TLS_SLOTS];
            our_modify_ldt_t clear_desc;
            ASSERT(tls_gdt_index == -1);
            for (i = 0; i < GDT_NUM_TLS_SLOTS; i++)
                avail_index[i] = -1;
            for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
                initialize_ldt_struct(&desc, segment, PAGE_SIZE, -1);
                res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
                LOG(GLOBAL, LOG_THREADS, 4,
                    "%s: set_thread_area -1 => %d res, %d index\n",
                    __func__, res, desc.entry_number);
                if (res >= 0) {
                    /* We assume monotonic increases */
                    avail_index[i] = desc.entry_number;
                    ASSERT(avail_index[i] > tls_gdt_index);
                    tls_gdt_index = desc.entry_number;
                } else
                    break;
            }
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
            if (tls_gdt_index > -1)
                res = 0;
        } else {
            /* For subsequent threads we need to clobber the parent's
             * entry with our own
             */
            ASSERT(tls_gdt_index > -1);
            initialize_ldt_struct(&desc, segment, PAGE_SIZE, tls_gdt_index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 4,
                "%s: set_thread_area -1 => %d res, %d index\n",
                __func__, res, desc.entry_number);
            ASSERT(res < 0 || desc.entry_number == tls_gdt_index);
        }
        if (res >= 0) {
            LOG(GLOBAL, LOG_THREADS, 1,
                "os_tls_init: set_thread_area successful for base "PFX" @index %d\n",
                segment, tls_gdt_index);
            os_tls->tls_type = TLS_TYPE_GDT;
            index = tls_gdt_index;
            selector = GDT_SELECTOR(index);
            WRITE_FS(selector); /* macro needs lvalue! */
        } else {
            IF_VMX86(ASSERT_NOT_REACHED()); /* since no modify_ldt */
            LOG(GLOBAL, LOG_THREADS, 1,
                "os_tls_init: set_thread_area failed: error %d\n", res);
        }
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
        WRITE_FS(selector); /* macro needs lvalue! */
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
        ((byte*)local_state) - offsetof(os_local_state_t, state);
    tls_type_t tls_type = os_tls->tls_type;
    int index = os_tls->ldt_index;

    if (!other_thread) {
        WRITE_FS(zero); /* macro needs lvalue! */
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
            WRITE_FS(zero); /* macro needs lvalue! */
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
        ((byte*)dcontext->local_state) - offsetof(os_local_state_t, state);
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
        WRITE_FS(zero); /* macro needs lvalue! */
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
    if (INTERNAL_OPTION(profile_pcs)) {
        pcprofile_thread_init(dcontext);
    }

    LOG(THREAD, LOG_THREADS, 1, "cur gs base is "PFX"\n", get_segment_base(SEG_GS));
    LOG(THREAD, LOG_THREADS, 1, "cur fs base is "PFX"\n", get_segment_base(SEG_FS));
}

void
os_thread_exit(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;

    DELETE_LOCK(ostd->suspend_lock);

    signal_thread_exit(dcontext);
    if (INTERNAL_OPTION(profile_pcs)) {
        pcprofile_thread_exit(dcontext);
    }

    /* for non-debug we do fast exit path and don't free local heap */
    DODEBUG({
        heap_free(dcontext, ostd, sizeof(os_thread_data_t) HEAPACCT(ACCT_OTHER));
    });
}

/* this one is called before child's new logfiles are set up */
void
os_fork_init(dcontext_t *dcontext)
{
    /* re-populate cached data that contains pid */
    pid_cached = get_process_id();
    get_application_pid_helper(true);
    get_application_name_helper(true);
    if (INTERNAL_OPTION(profile_pcs)) {
        pcprofile_fork_init(dcontext);
    }
    signal_fork_init(dcontext);
}

void
os_thread_under_dynamo(dcontext_t *dcontext)
{
    /* FIXME: what about signals? */
    if (INTERNAL_OPTION(profile_pcs)) {
        start_itimer(dcontext);
    }
}

void
os_thread_not_under_dynamo(dcontext_t *dcontext)
{
    /* FIXME: what about signals? */
    if (INTERNAL_OPTION(profile_pcs)) {
        stop_itimer();
    }
}

static pid_t
get_process_group_id()
{
    return dynamorio_syscall(SYS_getpgid, 0);
}

process_id_t
get_process_id()
{
    return dynamorio_syscall(SYS_getpid, 0);
}

process_id_t
get_parent_id(void)
{
    return dynamorio_syscall(SYS_getppid, 0);
}

thread_id_t 
get_thread_id()
{
    if (kernel_thread_groups) {
        return dynamorio_syscall(SYS_gettid, 0);
    } else {
        return dynamorio_syscall(SYS_getpid, 0);
    }
}

thread_id_t
get_tls_thread_id(void)
{
    ptr_int_t tid; /* can't use thread_id_t since it's 32-bits */
    /* need dedicated-storage var for _TLS_SLOT macros, can't use expr */
    ushort offs;
    if (!is_segment_register_initialized())
        return INVALID_THREAD_ID;
    offs = TLS_THREAD_ID_OFFSET;
    READ_TLS_SLOT(offs, tid);
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(tid)));
    return (thread_id_t) tid;
}

/* returns the thread-private dcontext pointer for the calling thread */
dcontext_t*
get_thread_private_dcontext(void)
{
#ifdef HAVE_TLS
    dcontext_t *dcontext;
    /* FIXME: need dedicated-storage var for _TLS_SLOT macros, can't use expr */
    ushort offs;
    /* We have to check this b/c this is called from __errno_location prior
     * to os_tls_init, as well as after os_tls_exit, and early in a new
     * thread's initialization (see comments below on that).
     */
    if (!is_segment_register_initialized())
        return NULL;
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
    DOCHECK(1, {
        ASSERT(get_tls_thread_id() == get_thread_id() ||
               /* ok for fork as mentioned above */
               pid_cached != get_process_id());
    });
    offs = TLS_DCONTEXT_OFFSET;
    READ_TLS_SLOT(offs, dcontext);
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
    /* FIXME: need dedicated-storage var for _TLS_SLOT macros, can't use expr */
    ushort offs = TLS_DCONTEXT_OFFSET;
    ASSERT(is_segment_register_initialized());
    WRITE_TLS_SLOT(offs, dcontext);
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
    /* FIXME: need dedicated-storage var for _TLS_SLOT macros, can't use expr */
    ushort offs = TLS_THREAD_ID_OFFSET;
    ptr_int_t new_tid = new; /* can't use thread_id_t since it's 32-bits */
    ASSERT(is_segment_register_initialized());
    DODEBUG({
        ptr_int_t old_tid; /* can't use thread_id_t since it's 32-bits */
        READ_TLS_SLOT(offs, old_tid);
        IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(old_tid)));
        ASSERT(old_tid == old);
    });
    WRITE_TLS_SLOT(offs, new_tid);
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
static inline uint
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

static bool
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
    ASSERT(!executable || preferred == NULL ||
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
                     IF_X64(| (DYNAMO_OPTION(heap_in_lower_4GB) ? MAP_32BIT : 0)),
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
    ASSERT(!executable ||
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

/* yield the current thread */
void
thread_yield()
{
    dynamorio_syscall(SYS_sched_yield, 0);
}

static bool
thread_signal(thread_record_t *tr, int signum)
{
    /* FIXME: for non-NPTL use SYS_kill */
    /* Note that the pid is equivalent to the thread group id.
     * However, we can have threads sharing address space but not pid
     * (if created via CLONE_VM but not CLONE_THREAD), so make sure to
     * use the pid of the target thread, not our pid.
     */
    return (dynamorio_syscall(SYS_tgkill, 3, tr->dcontext->owning_process,
                              tr->id, signum) == 0);
}

void
thread_sleep(uint64 milliseconds)
{
    struct timespec req;
    struct timespec remain;
    int count = 0;
    req.tv_sec = (milliseconds / 1000);
    /* docs say can go up to 1000000000, but doesn't work on FC9 */
    req.tv_nsec = (milliseconds % 1000) * 1000;
    while (dynamorio_syscall(SYS_nanosleep, 2, &req, &remain) == -1) {
        /* interrupted by signal or something: finish the interval */
        ASSERT(remain.tv_sec < (milliseconds / 1000) &&
               remain.tv_nsec < (milliseconds % 1000) * 1000);
        if (count++ > 3) {
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
        ASSERT(!ostd->suspended);
        if (!thread_signal(tr, SUSPEND_SIGNAL)) {
            ostd->suspend_count--;
            mutex_unlock(&ostd->suspend_lock);
            return false;
        }
    }
    /* we can unlock before the wait loop b/c we're using a separate "resumed"
     * bool and thread_resume holds the lock across its wait.  this way a resume
     * can proceed as soon as the suspended thread is suspended, before the
     * suspending thread gets scheduled again.
     */
    mutex_unlock(&ostd->suspend_lock);
    /* FIXME PR 295561: use futex */
    while (!ostd->suspended) {
        thread_yield();
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
    ostd->suspend_count--;
    if (ostd->suspend_count > 0) {
        mutex_unlock(&ostd->suspend_lock);
        return true; /* still suspended */
    }
    ostd->wakeup = true;
    /* FIXME PR 295561: use futex */
    while (!ostd->resumed)
        thread_yield();
    ostd->wakeup = false;
    ostd->resumed = false;
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
    return thread_signal(tr, SUSPEND_SIGNAL);
}

bool
is_thread_terminated(dcontext_t *dcontext)
{
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
    ASSERT(ostd != NULL);
    return ostd->terminated;
}

bool
thread_get_mcontext(thread_record_t *tr, dr_mcontext_t *mc)
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
thread_set_mcontext(thread_record_t *tr, dr_mcontext_t *mc)
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

int
get_num_processors()
{
    static uint num_cpu = 0;         /* cached value */
    if (!num_cpu) {
        num_cpu = get_nprocs_conf();
        ASSERT(num_cpu);
    }
    return num_cpu;
}


#if defined(CLIENT_INTERFACE) || defined(HOT_PATCHING_INTERFACE)
shlib_handle_t 
load_shared_library(char *name)
{
    return dlopen(name, RTLD_LAZY);
}
#endif

#if defined(CLIENT_INTERFACE)
shlib_routine_ptr_t
lookup_library_routine(shlib_handle_t lib, char *name)
{
    return dlsym(lib, name);
}

void
unload_shared_library(shlib_handle_t lib)
{
    if (!DYNAMO_OPTION(avoid_dlclose))
        dlclose(lib);
}

void
shared_library_error(char *buf, int maxlen)
{
    char *err = dlerror();
    strncpy(buf, err, maxlen-1);
    buf[maxlen-1] = '\0'; /* strncpy won't put on trailing null if maxes out */
}

/* addr is any pointer known to lie within the library */
bool
shared_library_bounds(IN shlib_handle_t lib, IN byte *addr,
                      OUT byte **start, OUT byte **end)
{
    /* PR 366195: dlopen() handle truly is opaque, so we have to use addr */
    ASSERT(start != NULL && end != NULL);
    *start = addr;
    return (get_library_bounds(NULL, start, end, NULL, 0) > 0);
}
#endif /* defined(CLIENT_INTERFACE) */

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

/* FIXME : implement? is easy for files, but is only used for dirs on linux
 * right now which don't know how to do without sideeffects */
bool
os_file_exists(const char *fname, bool is_dir)
{
#if 0 /* compile out to prevent warning */
    ASSERT_NOT_IMPLEMENTED(true);
#endif
    return true;
}

bool
os_get_file_size(const char *file, uint64 *size)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

bool
os_get_file_size_by_handle(file_t fd, uint64 *size)
{
    int64 cur_pos = os_tell(fd);
    int64 file_size = -1;
    int result;
    bool ret = true;

    if (cur_pos == -1)
        return false;

    /* FIXME - multithread safety? Presumably caller is synchronizing file access. */
    ASSERT_NOT_TESTED();
    result = llseek_syscall(fd, 0, SEEK_END, &file_size);

    if (result != 0 || file_size == -1)
        ret = false;
    else if (size != NULL)
        *size = file_size;

    result = llseek_syscall(fd, cur_pos, SEEK_SET, &file_size);
    ASSERT(result == 0);

    return ret;
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

/* not easily accessible in header files */
#ifdef X64
/* not needed */
# define O_LARGEFILE    0
#else
# define O_LARGEFILE    0100000
#endif

/* we assume that opening for writing wants to create file */
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
                            O_APPEND : 0)|
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

ssize_t
os_write(file_t f, const void *buf, size_t count)
{
    return write_syscall(f, buf, count);
}

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

/* seek the current file position to offset bytes from origin, return true if succesful */
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
os_rename_file(const char *orig_name, const char *new_name, bool replace)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME PR 295534: NYI */ 
    return false;
}

bool
os_delete_mapped_file(const char *filename)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME PR 295534: NYI */ 
    return false;
}

byte *
os_map_file(file_t f, size_t size, uint64 offs, app_pc addr, uint prot,
            bool copy_on_write)
{
    /* Linux mmap has 32-bit off_t; mmap2 considers it a page-size multiple;
     * we stick w/ mmap and assume no need for larger offsets for now.
     */
    ASSERT(CHECK_TRUNCATE_TYPE_uint(offs));
    byte *map = mmap_syscall(addr, size, memprot_to_osprot(prot),
                             copy_on_write ? MAP_PRIVATE : MAP_SHARED,
                             f, offs);
    ASSERT_NOT_TESTED();
    if (!mmap_syscall_succeeded(map))
        map = NULL;
    return map;
}

bool
os_unmap_file(byte *map, size_t size)
{
    long res = munmap_syscall(map, size);
    ASSERT_NOT_TESTED();
    return (res == 0);
}

bool
os_get_disk_free_space(/*IN*/ file_t file_handle,
                       /*OUT*/ uint64 *AvailableQuotaBytes /*OPTIONAL*/,
                       /*OUT*/ uint64 *TotalQuotaBytes /*OPTIONAL*/,
                       /*OUT*/ uint64 *TotalVolumeBytes /*OPTIONAL*/)
{
    ASSERT_NOT_IMPLEMENTED(false); /* FIXME PR 295534: NYI */ 
    return false;
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

bool
safe_read_ex(const void *base, size_t size, void *out_buf, size_t *bytes_read)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool res = false;
    STATS_INC(num_safe_reads);
    if (dcontext != NULL) {
        TRY_EXCEPT(dcontext, {
            memcpy(out_buf, base, size);
            res = true;
         } , { /* EXCEPT */
            /* nothing: res is already false */
         });
    } else {
        /* this is subject to races, but should only happen at init/attach when
         * there should only be one live thread.
         */
        if (is_readable_without_exception(base, size)) {
            memcpy(out_buf, base, size);
            res = true;
        }
    }
    if (res) {
        if (bytes_read != NULL)
            *bytes_read = size;
        return true;
    } else {
        if (bytes_read != NULL)
            *bytes_read = 0;
        return false;
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
    uint prot;
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
    /* FIXME case 9745: disabling all_memory_areas here since causing a ton
     * of asserts; re-enable once we're sure it truly matches the real
     * world
     */
    return is_readable_without_exception_internal(pc, size, 
#ifdef HAVE_PROC_MAPS
                                                  true /* do query os */
#else
                                                  false
#endif
                                                  );
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

/* change protections on memory region starting at pc of length length */
bool
set_protection(byte *pc, size_t length, uint prot/*MEMPROT_*/)
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
    all_memory_areas_lock();
    ASSERT(vmvector_overlap(all_memory_areas, start_page, start_page + num_bytes) ||
           /* we could synch up: instead we relax the assert if DR areas not in allmem */
           are_dynamo_vm_areas_stale());
    LOG(GLOBAL, LOG_VMAREAS, 3, "\tupdating all_memory_areas "PFX"-"PFX" prot->%d\n",
        start_page, start_page + num_bytes, osprot_to_memprot(flags));
    update_all_memory_areas(start_page, start_page + num_bytes,
                            osprot_to_memprot(flags), -1/*type unchanged*/);
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

/* make pc's page writable
 * FIXME: how get current protection?  would like to keep old read/exec flags
 */
bool
make_writable(byte *pc, size_t size)
{
    long res;
    app_pc start_page = (app_pc) PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC|PROT_READ|PROT_WRITE;

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

/* make pc's page unwritable 
 * FIXME: how get current protection?  would like to keep old read/exec flags
 */
void
make_unwritable(byte *pc, size_t size)
{
    long res;
    app_pc start_page = (app_pc) PAGE_START(pc);
    size_t prot_size = (size == 0) ? PAGE_SIZE : size;
    uint prot = PROT_EXEC|PROT_READ;

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

static inline reg_t *
sys_param_addr(dcontext_t *dcontext, int num)
{
    /* we force-inline get_mcontext() and so don't take it as a param */
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
        dr_mcontext_t *mc = get_mcontext(dcontext);
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
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
                              (get_num_threads() == 1 && !dynamo_exited));
        ASSERT_NOT_REACHED();
    }
}

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
     * We also pass the current DYNAMORIO_OPTIONS to the new image.
     * FIXME: security hole here -- have some other way to pass the options?
     */
    /* FIXME i#191: supposed to preserve things like pending signal
     * set across execve: going to ignore for now
     */
    char *fname = (char *)  sys_param(dcontext, 0);
    char **envp = (char **) sys_param(dcontext, 2);
    int i, preload = -1, ldpath = -1, ops = -1;
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

    if (envp == NULL) {
        LOG(THREAD, LOG_SYSCALLS, 3, "\tenv is NULL\n");
        i = 0;
    } else {
        for (i = 0; envp[i] != NULL; i++) {
            /* execve env vars should never be set here */
            ASSERT(strstr(envp[i], DYNAMORIO_VAR_EXECVE) != envp[i]);
            if (strstr(envp[i], DYNAMORIO_VAR_OPTIONS) == envp[i]) {
                ops = i;
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
    dcontext->sys_param0 = (reg_t) envp;

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
    int idx_preload = preload, idx_ldpath = ldpath, idx_ops = ops;
    char *options = option_string; /* global var */
    int num_new;
    char **new_envp;
    uint logdir_length;

    num_new = 
        (get_log_dir(PROCESS_DIR, NULL, NULL) ? 1 : 0) + /* logdir */
        2 + /* execve indicator var plus final NULL */
        ((preload<0) ? 1 : 0) +
        ((ldpath<0) ? 1 : 0) +
        ((ops < 0 && options != NULL) ? 1 : 0);
    new_envp = heap_alloc(dcontext, sizeof(char*)*(num_old+num_new)
                          HEAPACCT(ACCT_OTHER));
    memcpy(new_envp, envp, sizeof(char*)*num_old);

    *sys_param_addr(dcontext, 2) = (reg_t) new_envp;  /* OUT */
    /* store for reset in case execve fails */
    dcontext->sys_param1 = (reg_t) new_envp;
    dcontext->sys_param2 = (reg_t) num_old;
    /* if no var to overwrite, need new var at end */
    if (preload < 0)
        idx_preload = i++;
    if (ldpath < 0)
        idx_ldpath = i++;
    if (ops < 0 && options != NULL)
        idx_ops = i++;

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

    if (ops < 0 && options != NULL) {
        sz = strlen(DYNAMORIO_VAR_OPTIONS) + strlen(options) + 2;
        var = heap_alloc(dcontext, sizeof(char)*sz HEAPACCT(ACCT_OTHER));
        snprintf(var, sz, "%s=%s", DYNAMORIO_VAR_OPTIONS, options);
        *(var+sz-1) = '\0'; /* null terminate */
        new_envp[idx_ops] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n",
            idx_ops, new_envp[idx_ops]);
    }

    if (get_log_dir(PROCESS_DIR, NULL, &logdir_length)) {
        sz = strlen(DYNAMORIO_VAR_EXECVE_LOGDIR) + 1 +
            logdir_length;
        var = heap_alloc(dcontext, 
                         sizeof(char)*sz HEAPACCT(ACCT_OTHER));
        snprintf(var, sz, "%s=", DYNAMORIO_VAR_EXECVE_LOGDIR);
        get_log_dir(PROCESS_DIR, var+strlen(var), &logdir_length);
        *(var+sz-1) = '\0'; /* null terminate */
        new_envp[i++] = var;
        LOG(THREAD, LOG_SYSCALLS, 2, "\tnew env %d: %s\n", i-1, new_envp[i-1]);
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
}

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
    dr_mcontext_t *mc = get_mcontext(dcontext);
    bool exit_process = false;

    if (mc->xax == SYS_exit_group) {
        /* We can have multiple thread groups within the same address space.
         * We need to know whether this is the only group left.
         * FIXME: we can have races where new threads are created after our
         * check: we'll live with that for now, but the right approach is to
         * suspend all threads via synch_with_all_threads(), do the check,
         * and if exit_process then exit w/o resuming: though have to
         * coordinate lock access w/ cleanup_and_terminate.
         * Xref i#94.
         */
        process_id_t mypid = get_process_id();
        thread_record_t **threads;
        int num_threads, i;
        exit_process = true;
        mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num_threads);
        for (i=0; i<num_threads; i++) {
            if (threads[i]->pid != mypid) {
                exit_process = false;
                break;
            }
        }
        if (!exit_process) {
            /* We need to clean up the other threads in our group here. */
            thread_id_t myid = get_thread_id();
            dr_mcontext_t mcontext;
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

    if (get_num_threads() == 1 && !dynamo_exited) {
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

    /* N.B.: in all other cases, we just let interp exit,
     * and then either _fini is executed on app's stack,
     * or dynamorio_app_exit is called on app's stack...
     * here, we must switch back to app's stack, and hope
     * it's valid, because once we exit our stack is gone!
     * Can't access local vars once kill dstack, so we grab
     * the exit val and push it on the new stack right now
     * as an argument for the later call to _exit
     */
    cleanup_and_terminate(dcontext, mc->xax, sys_param(dcontext, 0),
                          sys_param(dcontext, 1), exit_process);
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
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
        LOG(THREAD, LOG_SYSCALLS, 2,
            "syscall: mmap2 addr="PFX" size="PIFX" prot=0x%x"
            " flags="PIFX" offset="PIFX" fd=%d\n",
            addr, len, prot, sys_param(dcontext, 3),
            sys_param(dcontext, 5), sys_param(dcontext, 4));
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t) addr;
        dcontext->sys_param1 = len;
        dcontext->sys_param2 = prot;
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
            if (ma != NULL)
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
        app_pc addr = (void *) sys_param(dcontext, 0);
        size_t old_len = (size_t) sys_param(dcontext, 1);
        size_t new_len = (size_t) sys_param(dcontext, 2);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: mremap addr="PFX" size="PFX"\n",
            addr, old_len);
        /* post_system_call does the work */
        dcontext->sys_param0 = (reg_t) addr;
        dcontext->sys_param1 = old_len;
        dcontext->sys_param2 = new_len;
        DODEBUG({
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
            DODEBUG(dcontext->mprot_multi_areas = (addr + len) > end ? true : false;);
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

        /* For thread creation clone syscalls a clone_record_t structure
         * containing the pc after the app's syscall instr and other data
         * (see i#27) is placed at the bottom of the dstack (which is allocated
         * by create_clone_record() - it also saves app stack and switches
         * to dstack).  xref i#149/PR 403015.
         * Note: This must be done after sys_param0 is set.
         */
        if (is_clone_thread_syscall(dcontext))
            create_clone_record(dcontext, sys_param_addr(dcontext, 1) /*newsp*/);

        break;
    }

    case SYS_vfork: {
        /* treat as if sys_clone with flags just as sys_vfork does */
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        uint flags = CLONE_VFORK | CLONE_VM | SIGCHLD;
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork\n");
        handle_clone(dcontext, flags);

        /* save for post_system_call, treated as if SYS_clone */
        dcontext->sys_param0 = (reg_t) flags;

        /* vfork has the same needs as clone.  Pass info via a clone_record_t
         * structure to child.  See SYS_clone for info about i#149/PR 403015.
         */
        /* FIXME: PR 403015 bug: there are no params to vfork: how is this working? */
        if (is_clone_thread_syscall(dcontext))
            create_clone_record(dcontext, sys_param_addr(dcontext, 1) /*newsp*/);
        break;
    }

    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork\n");
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
        handle_sigreturn(dcontext, true);
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
    case SYS_rt_sigqueueinfo: /* 178 */
    case SYS_setitimer:       /* 104 */
    case SYS_getitimer: {      /* 105 */
        /* FIXME: handle all of these syscalls! */
        LOG(THREAD, LOG_ASYNCH|LOG_SYSCALLS, 1,
            "WARNING: unhandled signal system call %d\n", mc->xax);
        break;
    }

    case SYS_close: {
        /* in fs/open.c: asmlinkage long sys_close(unsigned int fd) */
        uint fd = (uint) sys_param(dcontext, 0);
        LOG(THREAD, LOG_SYSCALLS, 3, "syscall: close fd %d\n", fd);
#ifdef DEBUG
        if (stats->loglevel > 0) {
            /* I assume other threads' files, and for that matter forked children's
             * files, will not be completely closed just by us closing them
             */
            if (fd == main_logfile || fd == dcontext->logfile) {
                LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                    "\tWARNING: app trying to close DynamoRIO file!  Not allowing it.\n");
                execute_syscall = false;    /* PR 402766 - break, not return. */
                SET_RETURN_VAL(dcontext, -EBADF);
                DODEBUG({ dcontext->expect_last_syscall_to_fail = true; });
                break;
            }
        }
#endif
        /* Xref PR 258731 - duplicate STDOUT/STDERR when app closes them so we (or
         * a client) can continue to use them for logging. */
        if (DYNAMO_OPTION(dup_stdout_on_close) && fd == STDOUT) {
            our_stdout = dup_syscall(fd);
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "WARNING: app is closing stdout=%d - duplicating descriptor for "
                "DynamoRIO usage got %d.\n", fd, our_stdout);
        }
        if (DYNAMO_OPTION(dup_stderr_on_close) && fd == STDERR) {
            our_stderr = dup_syscall(fd);
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "WARNING: app is closing stderr=%d - duplicating descriptor for "
                "DynamoRIO usage got %d.\n", fd, our_stderr);
        }
        if (DYNAMO_OPTION(dup_stdin_on_close) && fd == STDIN) {
            our_stdin = dup_syscall(fd);
            LOG(THREAD, LOG_TOP|LOG_SYSCALLS, 1,
                "WARNING: app is closing stdin=%d - duplicating descriptor for "
                "DynamoRIO usage got %d.\n", fd, our_stdin);
        }
        break;
    }

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
            i1->type == i2->type);
}

static void *
allmem_info_merge(void *dst_data, void *src_data)
{
    DODEBUG({
      allmem_info_t *src = (allmem_info_t *) src_data;
      allmem_info_t *dst = (allmem_info_t *) dst_data;
      ASSERT(src->prot == dst->prot &&
             src->type == dst->type);
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

void
update_all_memory_areas(app_pc start, app_pc end_in, uint prot, int type)
{
    allmem_info_t *info;
    app_pc end = (app_pc) ALIGN_FORWARD(end_in, PAGE_SIZE);
    ASSERT(ALIGNED(start, PAGE_SIZE));
    /* all_memory_areas lock is held higher up the call chain to avoid rank
     * order violation with heap_unit_lock */
    ASSERT_OWN_WRITE_LOCK(true, &all_memory_areas->lock);
    sync_all_memory_areas();

    if (vmvector_overlap(all_memory_areas, start, end)) {
        bool removed;
        if (type == -1) {
            /* we assume the entire region is the same type, if -1 passed in */
            if (vmvector_lookup_data(all_memory_areas, start,
                                     NULL, NULL, (void **) &info)) {
                type = info->type;
                DODEBUG({
                    /* [start..end_in) may cross multiple different-prot regions,
                     * but should all be same type
                     */
                    ASSERT(vmvector_lookup_data(all_memory_areas, end_in - 1,
                                                NULL, NULL, (void **) &info));
                    ASSERT(info->type == type);
                });
            } else
                ASSERT_NOT_REACHED();
        }
        LOG(THREAD_GET, LOG_VMAREAS|LOG_SYSCALLS, 4,
            "update_all_memory_areas: overlap found, removing and adding: "
            PFX"-"PFX" prot=%d\n", start, end, prot);
        /* New region to be added overlaps with one or more existing
         * regions.  Split already existing region(s) accordingly and add
         * the new region */
        removed = vmvector_remove(all_memory_areas, start, end);
        ASSERT(removed);
    } else {
        /* can't pass in -1 when no existing entry */
        ASSERT(type >= 0);
    }
    LOG(THREAD_GET, LOG_VMAREAS|LOG_SYSCALLS, 3,
        "update_all_memory_areas: adding: "PFX"-"PFX" prot=%d type=%d\n",
        start, end, prot, type);
    info = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, allmem_info_t, ACCT_MEM_MGT, PROTECTED);
    info->prot = prot;
    ASSERT(type >= 0);
    info->type = type;
    vmvector_add(all_memory_areas, start, end, (void *)info);
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
        DODEBUG({
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
                /* Issue 89: the vdso might be loaded inside ld.so as below, 
                 * which causes ASSERT_CURIOSITY fail.
                 * b7fa3000-b7fbd000 r-xp 00000000 08:01 108679     /lib/ld-2.8.90.so
                 * b7fbd000-b7fbe000 r-xp b7fbd000 00:00 0          [vdso]
                 * b7fbe000-b7fbf000 r--p 0001a000 08:01 108679     /lib/ld-2.8.90.so
                 * b7fbf000-b7fc0000 rw-p 0001b000 08:01 108679     /lib/ld-2.8.90.so
                 * So we should add a vsyscall exemptions check here.
                 */
                ASSERT_CURIOSITY(ma->start + ma->os_data.alignment == base ||
                                 base == vsyscall_page_start);
            }
        });
    }
    os_get_module_info_unlock();
    return ma != NULL;
}

/* All processing for mmap and mmap2. */
static void
process_mmap(dcontext_t *dcontext, app_pc base, size_t size, uint prot
            _IF_DEBUG(char *map_type))
{
    bool image = false;
    uint memprot = osprot_to_memprot(prot);
    app_pc area_start;
    app_pc area_end;
    bool overlaps_image;
    allmem_info_t *info;

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
    /* FIXME: if we have an early injection before ld.so, we need a way to
     * find the vdso if using sysenter so that we can hook it and syscalls 
     * will work properly on recent Linux distributions
     * So we should look for VSYSCALL_PAGE_MAPS_NAME here as well as in
     * find_executable_vm_areas. xref i#89.
     */
    overlaps_image = mmap_check_for_module_overlap(base, size,
                                                   TEST(MEMPROT_READ, memprot), 0, true);
    if (overlaps_image) {
        /* FIXME - how can we distinguish between the loader mapping the segments
         * over the initial map from someone just mapping over part of a module? If
         * is the latter case need to adjust the view size or remove from module list. */
        image = true;
        DODEBUG({ map_type = "ELF SO"; });
    } else if (TEST(MEMPROT_READ, memprot) && is_elf_so_header(base, size)) {
        maps_iter_t iter;
        bool found_map = false;;
        uint64 inode = 0;
        char *filename = "";
        LOG(THREAD, LOG_SYSCALLS|LOG_VMAREAS, 2, "dlopen "PFX"-"PFX"%s\n",
            base, base+size, TEST(MEMPROT_EXEC, memprot) ? " +x": "");
        image = true;
        DODEBUG({ map_type = "ELF SO"; });
        /* Mapping in a new module.  From what we've observed of the loader's
         * behavior, it first maps the file in with size equal to the final
         * memory image size (I'm not sure how it gets that size without reading
         * in the elf header and then walking through all the program headers to
         * get the largest virtual offset).  This is neccesary to reserve all the
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
         * tell when the loader is finished.  The downside is that at the intial map
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
        module_list_add(base, ALIGN_FORWARD(size, PAGE_SIZE), true, inode, filename);
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
}

/* Returns false if system call should NOT be executed
 * Returns true if system call should go ahead
 */
/* FIXME: split out specific handlers into separate routines
 */
void
post_system_call(dcontext_t *dcontext)
{
    dr_mcontext_t *mc = get_mcontext(dcontext);
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
            thread_id_t child = get_thread_id();
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
        }
    }


    LOG(THREAD, LOG_SYSCALLS, 2,
        "post syscall: sysnum="PFX", result="PFX" (%d)\n",
        sysnum, mc->xax, (int)mc->xax);

    switch (sysnum) {

    /****************************************************************************/
    /* MEMORY REGIONS */

#ifndef X64
    case SYS_mmap2:
#endif
    case SYS_mmap: {
        DEBUG_DECLARE(char *map_type;)
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
            DEBUG_DECLARE(map_type = "mmap";)
        }
        else {
#endif
            size = (size_t) dcontext->sys_param1;
            prot = (uint) dcontext->sys_param2;
            DEBUG_DECLARE(map_type = IF_X64_ELSE("mmap2","mmap");)
#ifndef X64
        }
#endif
        process_mmap(dcontext, base, size, prot _IF_DEBUG(map_type));
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
            DEBUG_DECLARE(ok =)
                query_memory_ex(old_base, &info);
            ASSERT(ok);
            DODEBUG({
                    /* we don't expect to see remappings of modules */
                    os_get_module_info_lock();
                    ASSERT_CURIOSITY(!module_overlaps(base, size));
                    os_get_module_info_unlock();
                });
            /* Verify that the current prot on the new region (according to
             * the os) is the same as what the prot used to be for the old
             * region.
             */
            DODEBUG({
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
            
            /* i#175
             * there are cases that base = mremap(old_base, old_size, size),
             * where old_base = base, and old_size > size, 
             * they do overlap and should not cause assertion failure.
             * So if the new mremaped memory fall into old memory region,
             * assertion success.
             */
            ASSERT((base >= old_base             && 
                    base < (old_base + old_size) &&
                    (base + size) > old_base     && 
                    (base + size) <= (old_base + size)) ||
                   !vmvector_overlap(all_memory_areas, base, base + size) ||
                   are_dynamo_vm_areas_stale());
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
        /* FIXME i#143: we need to tweak the returned oldprot for
         * writable areas we've made read-only
         */
        if (!success) {
            uint memprot = 0;
            /* Revert the prot bits if needed. */
            DEBUG_DECLARE(ok =)
                get_memory_info_from_os(base, NULL, NULL, &memprot);
            ASSERT(ok);
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
        if (new_brk < old_brk) {
            all_memory_areas_lock();
            DEBUG_DECLARE(ok =)
                remove_from_all_memory_areas(new_brk, old_brk);
            ASSERT(ok);
            all_memory_areas_unlock();
        } else if (new_brk > old_brk) {
            allmem_info_t *info;
            all_memory_areas_lock();
            sync_all_memory_areas();
            info = vmvector_lookup(all_memory_areas, old_brk - 1);
            ASSERT(info != NULL);
            update_all_memory_areas(old_brk, new_brk,
                                    /* be paranoid */
                                    (info != NULL) ? info->prot :
                                    MEMPROT_READ|MEMPROT_WRITE, DR_MEMTYPE_DATA);
            all_memory_areas_unlock();
        }
        break;
    }

    /****************************************************************************/
    /* SPAWNING -- fork mostly handled above */

    case SYS_clone: {
        /* in /usr/src/linux/arch/i386/kernel/process.c */
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: clone returned "PFX"\n", mc->xax);
        break;
    }

    case SYS_fork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: fork returned "PFX"\n", mc->xax);
        break;
    }

    case SYS_vfork: {
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: vfork returned "PFX"\n", mc->xax);
        break;
    }

    case SYS_execve: {
        /* if we get here it means execve failed (doesn't return on success)
         * we have to restore env to how it was
         */
        char **old_envp = (char **) dcontext->sys_param0;
        char **new_envp = (char **) dcontext->sys_param1;
        int num_old = (int) dcontext->sys_param2;
        success = false;
        ASSERT(result < 0);
        LOG(THREAD, LOG_SYSCALLS, 2, "syscall: execve failed\n");
#ifdef STATIC_LIBRARY
        /* nothing to clean up */
        break;
#endif
        if (new_envp != NULL) {
            int i;
            LOG(THREAD, LOG_SYSCALLS, 2, "\tcleaning up our env vars\n");
            /* we replaced existing ones and/or added new ones */
            for (i=0; i<num_old; i++) {
                if (old_envp[i] != new_envp[i]) {
                    heap_free(dcontext, new_envp[i],
                              sizeof(char)*(strlen(new_envp[i])+1)
                              HEAPACCT(ACCT_OTHER));
                }
            }
            for (; new_envp[i] != NULL; i++) {
                heap_free(dcontext, new_envp[i],
                          sizeof(char)*(strlen(new_envp[i])+1)
                          HEAPACCT(ACCT_OTHER));
            }
            i++; /* need to de-allocate final null slot too */
            heap_free(dcontext, new_envp, sizeof(char*)*i HEAPACCT(ACCT_OTHER));
            /* restore prev envp */
            *sys_param_addr(dcontext, 2) = (reg_t) old_envp;
        }
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
                SYSLOG_INTERNAL_ERROR_ONCE("Unexpected failure of non-ignorable"
                                           " syscall (%d)", sysnum);
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
        if (iter_data->target_addr >= base &&
            iter_data->target_addr < base + (pref_end - pref_start)) {
            if (iter_data->path_size > 0) {
                /* We want just the path, not the filename */
                char *slash = rindex(info->dlpi_name, '/');
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
    ASSERT(start != NULL);
    /* We don't have the base and we can't walk backwards (see comments above)
     * so we rely on dl_iterate_phdr() from glibc 2.2.4+, which will
     * also give us the path, needed for inject_library_path for execve.
     */
    iter_data.target_addr = *start;
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

        if ((name_cmp != NULL && strstr(iter.comment, name_cmp) != NULL) ||
            (name == NULL && *start >= iter.vm_start && *start < iter.vm_end)) {
            if (!found_library) {
                size_t mod_readable_sz;
                char *dst = (fullpath != NULL) ? fullpath : libname;
                size_t dstsz = (fullpath != NULL) ? path_size :
                    BUFFER_SIZE_ELEMENTS(libname);
                char *slash = rindex(iter.comment, '/');
                ASSERT_CURIOSITY(slash != NULL);
                ASSERT_CURIOSITY((slash - iter.comment) < dstsz);
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
    /* PR 361594: we get our bounds from linker-provided symbols.
     * Note that referencing the value of these symbols will crash:
     * always use the address only.
     */
    extern int dynamorio_so_start, dynamorio_so_end;
    app_pc check_start, check_end;
    char *libdir;
    dynamo_dll_start = (app_pc) &dynamorio_so_start;
    dynamo_dll_end = (app_pc) ALIGN_FORWARD(&dynamorio_so_end, PAGE_SIZE);
#ifndef HAVE_PROC_MAPS
    check_start = dynamo_dll_start;
#endif
    res = get_library_bounds(DYNAMORIO_LIBRARY_NAME,
                             &check_start, &check_end,
                             dynamorio_library_path,
                             BUFFER_SIZE_ELEMENTS(dynamorio_library_path));
    LOG(GLOBAL, LOG_VMAREAS, 1, PRODUCT_NAME" library path: %s\n",
        dynamorio_library_path);
    ASSERT(check_start == dynamo_dll_start && check_end == dynamo_dll_end);
    LOG(GLOBAL, LOG_VMAREAS, 1, "DR library bounds: "PFX" to "PFX"\n",
        dynamo_dll_start, dynamo_dll_end);
#ifdef STATIC_LIBRARY
    dynamorio_library_path[0] = '\0';
#else
    ASSERT(res > 0);
#endif

    /* Issue 20: we need the path to the alt arch */
    strncpy(dynamorio_alt_arch_path, dynamorio_library_path,
            BUFFER_SIZE_ELEMENTS(dynamorio_alt_arch_path));
    /* Assumption: libdir name is not repeated elsewhere in path */
    libdir = strstr(dynamorio_alt_arch_path, IF_X64_ELSE(DR_LIBDIR_X64, DR_LIBDIR_X86));
    if (libdir != NULL) {
        char *newdir = IF_X64_ELSE(DR_LIBDIR_X86, DR_LIBDIR_X64);
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
 * We just go to max 32-bit (64-bit not supported yet: want lazy probing)
 */
# ifdef X64
#  error X64 requires HAVE_PROC_MAPS: PR 364552
# endif
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
        module_list_add(modbase, modsize, false, 0/*don't have inode*/, info->dlpi_name);

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
        DEBUG_DECLARE(char *map_type = "Private");
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
         * mmap_check_for_module_overlap. And in mmap_check_for_module_overlap,
         * a vsyscall exemptions check is added.
         */
        if (strncmp(iter.comment, VSYSCALL_PAGE_MAPS_NAME,
                    strlen(VSYSCALL_PAGE_MAPS_NAME)) == 0
            /* Older kernels do not label it as "[vdso]", but it is hardcoded there */
            IF_NOT_X64(|| iter.vm_start == VSYSCALL_PAGE_START_HARDCODED)) {
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
            module_list_add(iter.vm_start, image_size, false, iter.inode, iter.comment);
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
        if (app_memory_allocation(NULL, iter.vm_start, (iter.vm_end - iter.vm_start),
                                  iter.prot, image _IF_DEBUG(map_type))) {
            count++;
        }

    }
    maps_iterator_stop(&iter);
#endif /* HAVE_PROC_MAPS */

    DODEBUG({
        LOG(GLOBAL, LOG_VMAREAS, 4, "init: all memory areas:\n");
        DOLOG(4, LOG_VMAREAS, print_all_memory_areas(GLOBAL););
    });

    /* now that we've walked memory print all modules */
    LOG(GLOBAL, LOG_VMAREAS, 2, "Module list after memory walk\n");
    DOLOG(1, LOG_VMAREAS, { print_modules(GLOBAL, DUMP_NOT_XML); });

    STATS_ADD(num_app_code_modules, count);
    return count;
}

/* initializes dynamorio library bounds.
 * does not use any heap.
 * assumed to be called prior to find_executable_vm_areas.
 */
int
find_dynamo_library_vm_areas(void)
{
    /* We didn't add inside get_dynamo_library_bounds b/c it was called pre-alloc.
     * We don't bother to break down the sub-regions.
     * Assumption: we don't need to have the protection flags for DR sub-regions.
     */
    add_dynamo_vm_area(get_dynamorio_dll_start(), get_dynamorio_dll_end(),
                       MEMPROT_READ|MEMPROT_WRITE|MEMPROT_EXEC,
                       true /* from image */ _IF_DEBUG(dynamorio_library_path));
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
         /* FIXME: once our mem list is robust use it */
         ok = get_memory_info_from_os((app_pc)get_mcontext(dcontext)->xsp,
                                      &ostd->stack_base, &size, NULL);
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
    os_thread_data_t *ostd = (os_thread_data_t *) dcontext->os_field;
    if (target_pc == ostd->stack_bottom_pc) {
        return INITIAL_STACK_BOTTOM_REACHED;
    } else {
        return INITIAL_STACK_BOTTOM_NOT_REACHED;
    }
    /* we never start with an INITIAL_STACK_EMPTY */
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
        DODEBUG({
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
                SYSLOG_INTERNAL_ERROR("get_memory_info mismatch! "
                    "(can happen if os combines entries in /proc/pid/maps)\n"
                    "\tos says: "PFX"-"PFX" prot="PFX"\n"
                    "\tcache says: "PFX"-"PFX" prot="PFX"\n",
                    from_os_base_pc, from_os_base_pc + from_os_size,
                    from_os_prot, start, end, info->prot);
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
    /* silly to loop over all x64 pages */
    IF_X64(ASSERT_NOT_IMPLEMENTED(false));

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
        if (TEST(MEMPROT_READ, info->prot) && is_elf_so_header(info->base_pc, info->size))
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
    IF_CLIENT_INTERFACE(dcontext_t *dcontext = get_thread_private_dcontext();)
    /* FIXME: we don't actually use system calls to synchronize on Linux,
     * one day we would use futex(2) on this path (PR 295561). 
     * For now we use a busy-wait lock.
     * If we do use a true wait need to set client_thread_safe_for_synch around it */

    /* we now have to undo our earlier request */
    atomic_dec_and_test(&lock->lock_requests);

    while (!mutex_trylock(lock)) {
#ifdef CLIENT_INTERFACE
        if (dcontext != NULL && IS_CLIENT_THREAD(dcontext) &&
            (mutex_t *)dcontext->client_data->client_grab_mutex == lock)
            dcontext->client_data->client_thread_safe_for_synch = true;
#endif
        thread_yield();
#ifdef CLIENT_INTERFACE
        if (dcontext != NULL && IS_CLIENT_THREAD(dcontext) &&
            (mutex_t *)dcontext->client_data->client_grab_mutex == lock)
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

    return;
    
}

void
mutex_notify_released_lock(mutex_t *lock)
{
    /* nothing to do here */
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
    volatile bool signaled;
    mutex_t lock;
} linux_event_t;


/* FIXME: this routine will need to have a macro wrapper to let us assign different ranks to 
 * all events for DEADLOCK_AVOIDANCE.  Currently a single rank seems to work.
 */
event_t
create_event()
{
    event_t e = (event_t) global_heap_alloc(sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
    e->signaled = false;
    ASSIGN_INIT_LOCK_FREE(e->lock, event_lock); /* FIXME: we'll need to pass the event name here */
    return e;
}

void
destroy_event(event_t e)
{
    DELETE_LOCK(e->lock);
    global_heap_free(e, sizeof(linux_event_t) HEAPACCT(ACCT_OTHER));
}

/* FIXME PR 295561: use futex */
void
signal_event(event_t e)
{
    mutex_lock(&e->lock);
    e->signaled = true;
    LOG(THREAD_GET, LOG_THREADS, 3,"thread %d signalling event "PFX"\n",get_thread_id(),e);
    mutex_unlock(&e->lock);
}

void
reset_event(event_t e)
{
    mutex_lock(&e->lock);
    e->signaled = false;
    LOG(THREAD_GET, LOG_THREADS, 3,"thread %d resetting event "PFX"\n",get_thread_id(),e);
    mutex_unlock(&e->lock);
}

/* FIXME: compare use and implementation with  man pthread_cond_wait */
/* FIXME PR 295561: use futex */
void
wait_for_event(event_t e)
{
#ifdef DEBUG
    dcontext_t *dcontext = get_thread_private_dcontext();
#endif
    /* Use a user-space event on Linux, a kernel event on Windows. */
    LOG(THREAD, LOG_THREADS, 3, "thread %d waiting for event "PFX"\n",get_thread_id(),e);
    while (true) {
        if (e->signaled) {
            mutex_lock(&e->lock);
            if (!e->signaled) {
                /* some other thread beat us to it */
                LOG(THREAD, LOG_THREADS, 3, "thread %d was beaten to event "PFX"\n",
                    get_thread_id(),e);
                mutex_unlock(&e->lock);
            } else {
                /* reset the event */
                e->signaled = false;
                mutex_unlock(&e->lock);
                LOG(THREAD, LOG_THREADS, 3,
                    "thread %d finished waiting for event "PFX"\n", get_thread_id(),e);
                return;
            }
        }
        thread_yield();
    }
}

uint
os_random_seed()
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
          byte **app_code_copy_p, byte **alt_exit_cti_p)
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
#endif /* RCT_IND_BRANCH */

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
#if 0
    /* FIXME: case 9922 */
    ASSERT_NOT_IMPLEMENTED(true);
#endif
    return false;
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
