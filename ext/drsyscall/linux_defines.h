/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _LINUX_DEFINES_H
#define _LINUX_DEFINES_H 1

/* for linux-specific types/defines for fcntl and ipc */
#define __USE_GNU 1

#ifdef HAVE_ASM_I386
/* tying them all to this one header for now */
# define GLIBC_2_3_2 1
#endif

#include <sys/types.h>
#ifdef GLIBC_2_3_2
# include <asm-i386/stat.h>
#else
# include <asm/stat.h>
# include <sys/statfs.h>
#endif
#include <utime.h> /* struct utimbuf */
#include <sys/times.h> /* struct tms */
#include <sys/resource.h> /* struct rlimit */
#include <sys/time.h> /* struct timezone */
#include <sys/sysinfo.h> /* struct sysinfo */
#include <sys/timex.h> /* struct timex */
#include <linux/utsname.h> /* struct new_utsname */
#include <sched.h> /* struct sched_param */

#ifndef ANDROID
/* Avoid conflicts w/ DR's REG_* enum w/ more recent distros
 * by directly getting siginfo_t instead of including "<signal.h>".
 * Xref DRi#34.  We could instead update to use DR_REG_* and unset
 * DynamoRIO_REG_COMPATIBILITY.
 */
# define __need_siginfo_t
# define __need_sigevent_t
# include <asm/siginfo.h>
#else
# include <signal.h>
#endif

#include <linux/capability.h> /* cap_user_header_t */
/* capability.h conflicts with and is superset of these:
 * #include <sys/ustat.h> (struct ustat)
 * #include <sys/statfs.h> (struct statfs)
 */
#include <poll.h>
#include <sys/epoll.h> /* struct epoll_event */
#include <time.h> /* struct itimerspec */
#include <errno.h> /* for EBADF */
#include <linux/sysctl.h> /* struct __sysctl_args */

/* block bits/stat.h which is included from fcntl.h on FC16 (glibc 2.14) */
#define _BITS_STAT_H	1
#include <fcntl.h> /* F_GETFD, etc. */

#ifdef X86
# include <asm/ldt.h> /* struct user_desc */
#endif
#include <linux/futex.h>
#include <linux/mman.h> /* MREMAP_FIXED */

/* ipc */
#ifdef GLIBC_2_3_2
# include <sys/ipc.h>
# include <asm/ipc.h>
# include <sys/sem.h>
# include <sys/shm.h>
# include <sys/msg.h>
#else
# include <linux/ipc.h>
# include <linux/sem.h>
# include <linux/shm.h>
# include <linux/msg.h>
#endif

/* socket */
#include <sys/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/un.h>
#include <linux/netlink.h>

#ifndef SYS_ACCEPT4
# define SYS_ACCEPT4 18
#endif

/* ioctl */
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <termios.h>
#include <linux/serial.h>
#include <linux/ax25.h>
#include <linux/cdrom.h>
#include <linux/fs.h>

/* linux/cdk.h was removed from the kernel in 3.6 */
#define STL_BINTR   0x00007314
#define STL_BSTART  0x00007315
#define STL_BSTOP   0x00007316
#define STL_BRESET  0x00007317

/* i#911: linux/ext2_fs.h references a now-removed type umode_t in
 * FC16 (in flux apparently) so we define on our own:
 */
#ifndef EXT2_IOC_GETFLAGS
# ifndef FS_IOC_GETFLAGS
#  define FS_IOC_GETFLAGS                _IOR('f', 1, long)
#  define FS_IOC_SETFLAGS                _IOW('f', 2, long)
#  define FS_IOC_GETVERSION              _IOR('v', 1, long)
#  define FS_IOC_SETVERSION              _IOW('v', 2, long)
# endif
# define EXT2_IOC_GETFLAGS               FS_IOC_GETFLAGS
# define EXT2_IOC_SETFLAGS               FS_IOC_SETFLAGS
# define EXT2_IOC_GETVERSION             FS_IOC_GETVERSION
# define EXT2_IOC_SETVERSION             FS_IOC_SETVERSION
#endif

#ifndef ANDROID /* Android headers already have this. */
/* Including linux/resource.h leads to conflicts with other types so we define
 * this struct ourselves:
 */
struct rlimit64 {
    __u64 rlim_cur;
    __u64 rlim_max;
};
#endif

#include <linux/fd.h>
#include <linux/hdreg.h>
#include <linux/if.h>
#include <linux/if_plip.h>
#include <linux/kd.h>
#include <linux/lp.h>
#include <linux/mroute.h>
#ifdef GLIBC_2_3_2
# include <linux/mtio.h>
#elif !defined(ANDROID)
# include <sys/mtio.h>
#endif
#include <linux/netrom.h>
#include <linux/scc.h>

/* i#911: linux/smb_fs.h is missing on FC16 so we define on our own */
#define SMB_IOC_GETMOUNTUID             _IOR('u', 1, __kernel_old_uid_t)

#include <linux/sockios.h>
#include <linux/route.h>
#include <linux/if_arp.h>
#include <linux/soundcard.h>
#if 0 /* XXX: header not avail: ioctl code below disabled as well */
# include <linux/umsdos_fs.h>
#endif
#include <linux/vt.h>
#include <linux/ipmi.h> /* PR 531644 */
#ifndef GLIBC_2_3_2
# include <linux/net.h>
#endif

/* prctl */
#include <sys/prctl.h>
/* may not be building w/ most recent headers */
#ifndef PR_GET_FPEMU
# define PR_GET_FPEMU  9
# define PR_SET_FPEMU 10
#endif
#ifndef PR_GET_FPEXC
# define PR_GET_FPEXC    11
# define PR_SET_FPEXC    12
#endif
#ifndef PR_GET_TIMING
# define PR_GET_TIMING   13
# define PR_SET_TIMING   14
#endif
#ifndef PR_GET_NAME
# define PR_SET_NAME    15
# define PR_GET_NAME    16
#endif
#ifndef PR_GET_ENDIAN
# define PR_GET_ENDIAN   19
# define PR_SET_ENDIAN   20
#endif
#ifndef PR_GET_SECCOMP
# define PR_GET_SECCOMP  21
# define PR_SET_SECCOMP  22
#endif
#ifndef PR_CAPBSET_READ
# define PR_CAPBSET_READ 23
# define PR_CAPBSET_DROP 24
#endif
#ifndef PR_GET_TSC
# define PR_GET_TSC 25
# define PR_SET_TSC 26
#endif
#ifndef PR_GET_SECUREBITS
# define PR_GET_SECUREBITS 27
# define PR_SET_SECUREBITS 28
#endif
#ifndef PR_GET_TIMERSLACK
# define PR_SET_TIMERSLACK 29
# define PR_GET_TIMERSLACK 30
#endif

/* kernel's sigset_t packs info into bits, while glibc's uses a short for
 * each (-> 8 bytes vs. 128 bytes)
 */
#define MAX_SIGNUM  64
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

typedef struct _kernel_sigset_t {
    unsigned long sig[_NSIG_WORDS];
} kernel_sigset_t;

/* differs from libc sigaction.  we do not support 2.1.20 version of this. */
typedef struct _kernel_sigaction_t {
    void *handler;
    unsigned long flags;
    void (*restorer)(void);
    kernel_sigset_t mask;
} kernel_sigaction_t;
/* not in main defines */
#define SA_RESTORER 0x04000000

#ifdef GLIBC_2_3_2
union semun {
    int val; /* value for SETVAL */
    struct semid_ds *buf; /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array; /* array for GETALL, SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
};

/* not in older defines: version flag or-ed in for semctl, msgctl, shmctl */
# define IPC_64  0x0100
#endif

#ifndef ANDROID
/* ustat is deprecated and the header is not always available. */
struct ustat {
    __daddr_t f_tfree;
    __ino_t f_tinode;
    char f_fname[6];
    char f_fpack[6];
};
#endif

#endif /* _LINUX_DEFINES_H */
