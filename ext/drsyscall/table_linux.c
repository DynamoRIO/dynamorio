/* **********************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "sysnum_linux.h"
#include "linux_defines.h"
#include "table_defines.h"

#ifdef AARCH64
#include "aarch64_syscall_num.h"
#endif

extern syscall_info_t syscall_ioctl_info[];

/* Created from ./mksystable_linux.pl
 * And then manually:
 * - filling in params for those marked "Missing prototype"
 * - filling in params for those marked "special-case"
 * - replacing U with W or R
 * - updating sizeof(char) and sizeof(void)
 *
 * FIXME i#92: still a lot of missing details below!
 */

syscall_info_t syscall_info[] = {
    {{PACKNUM(219,0,0,AARCH64_restart_syscall),0},"restart_syscall", OK, RLONG, 0,},
    {{PACKNUM(60,1,1,AARCH64_exit),0},"exit", OK, RLONG, 1,},
    {{PACKNUM(57,2,2,AARCH64_fork),0},"fork", OK, RLONG, 0,},
    {{PACKNUM(0,3,3,AARCH64_read),0},"read", OK, RLONG, 3,
     {
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(1,4,4,AARCH64_write),0},"write", OK, RLONG, 3,
     {
         {1, -2, R},
     }
    },
    {{PACKNUM(2,5,5,AARCH64_open),0},"open", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    }, /*special-cased: 3rd arg not always required*/
    {{PACKNUM(3,6,6,AARCH64_close),0},"close", OK, RLONG, 1,},
    {{PACKNUM(-1,7,7,AARCH64_ni),0},"waitpid", OK, RLONG, 3,
     {
         {1, sizeof(int), W},
     }
    },
    {{PACKNUM(85,8,8,AARCH64_creat),0},"creat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(86,9,9,AARCH64_link),0},"link", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(87,10,10,AARCH64_unlink),0},"unlink", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(59,11,11,AARCH64_execve),0},"execve", OK, RLONG, 3,
     {
         {0, 0, R|CT, CSTRING},
         {1, sizeof(char **), R|CT, DRSYS_TYPE_CSTRARRAY},
         {2, sizeof(char **), R|CT, DRSYS_TYPE_CSTRARRAY},
     }
    },
    {{PACKNUM(80,12,12,AARCH64_chdir),0},"chdir", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(201,13,13,AARCH64_time),0},"time", OK, RLONG, 1,
     {
         {0, sizeof(time_t), W},
     }
    },
    {{PACKNUM(133,14,14,AARCH64_mknod),0},"mknod", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(90,15,15,AARCH64_chmod),0},"chmod", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,16,16,AARCH64_ni),0},"lchown16", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,17,17,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
#ifndef AARCH64
    {{PACKNUM(-1,18,18,AARCH64_stat),0},"stat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct __old_kernel_stat), W},
     }
    },
#endif
    {{PACKNUM(4,-1,-1,AARCH64_stat),0},"stat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(8,19,19,AARCH64_lseek),0},"lseek", OK, RLONG, 3,},
    {{PACKNUM(39,20,20,AARCH64_getpid),0},"getpid", OK, RLONG, 0,},
    {{PACKNUM(165,21,21,AARCH64_mount),0},"mount", OK, RLONG, 5,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2,0, R|CT, CSTRING},/*FIXME: 4 varies: ignore for now*/
     }
    },
    {{PACKNUM(-1,22,22,AARCH64_ni),0},"oldumount", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,23,23,AARCH64_ni),0},"setuid16", OK, RLONG, 1,},
    {{PACKNUM(-1,24,24,AARCH64_ni),0},"getuid16", OK, RLONG, 0,},
    {{PACKNUM(-1,25,25,AARCH64_ni),0},"stime", OK, RLONG, 1,
     {
         {0, sizeof(time_t), R},
     }
    },
    {{PACKNUM(101,26,26,AARCH64_ptrace),0},"ptrace", OK, RLONG, 4,},
    {{PACKNUM(37,27,27,AARCH64_alarm),0},"alarm", OK, RLONG, 1,},
#ifndef AARCH64
    {{PACKNUM(-1,28,28,AARCH64_fstat),0},"fstat", OK, RLONG, 2,
     {
         {1, sizeof(struct __old_kernel_stat), W},
     }
    },
#endif
    {{PACKNUM(5,-1,-1,AARCH64_fstat),0},"fstat", OK, RLONG, 2,
     {
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(34,29,29,AARCH64_pause),0},"pause", OK, RLONG, 0,},
    {{PACKNUM(132,30,30,AARCH64_utime),0},"utime", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct utimbuf), R},
     }
    },
    {{PACKNUM(-1,31,31,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,32,32,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(21,33,33,AARCH64_access),0},"access", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,34,34,AARCH64_ni),0},"nice", OK, RLONG, 1,},
    {{PACKNUM(-1,35,35,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(162,36,36,AARCH64_sync),0},"sync", OK, RLONG, 0,},
    {{PACKNUM(62,37,37,AARCH64_kill),0},"kill", OK, RLONG, 2,},
    {{PACKNUM(82,38,38,AARCH64_rename),0},"rename", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(83,39,39,AARCH64_mkdir),0},"mkdir", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(84,40,40,AARCH64_rmdir),0},"rmdir", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(32,41,41,AARCH64_dup),0},"dup", OK, RLONG, 1,},
    {{PACKNUM(22,42,42,AARCH64_pipe),0},"pipe", OK, RLONG, 1,
     {
         {0,2*sizeof(int), W},
     }
    },
    {{PACKNUM(100,43,43,AARCH64_times),0},"times", OK, RLONG, 1,
     {
         {0, sizeof(struct tms), W},
     }
    },
    {{PACKNUM(-1,44,44,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(12,45,45,AARCH64_brk),0},"brk", OK, RLONG, 1,},
    {{PACKNUM(-1,46,46,AARCH64_ni),0},"setgid16", OK, RLONG, 1,},
    {{PACKNUM(-1,47,47,AARCH64_ni),0},"getgid16", OK, RLONG, 0,},
    {{PACKNUM(-1,48,48,AARCH64_ni),0},"signal", OK, RLONG, 2,},
    {{PACKNUM(-1,49,49,AARCH64_ni),0},"geteuid16", OK, RLONG, 0,},
    {{PACKNUM(-1,50,50,AARCH64_ni),0},"getegid16", OK, RLONG, 0,},
    {{PACKNUM(163,51,51,AARCH64_acct),0},"acct", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,52,52,AARCH64_umount),0},"umount", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,53,53,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(16,54,54,AARCH64_ioctl),0},"ioctl", OK|SYSINFO_SECONDARY_TABLE, RLONG, 3,
     {
         {1,}  /* ioctl request number */
     }, (drsys_sysnum_t*)syscall_ioctl_info
    },
    {{PACKNUM(72,55,55,AARCH64_fcntl),0},"fcntl", OK, RLONG, 2,}, /*special-cased: 3rd arg not always required*/
    {{PACKNUM(-1,56,56,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(109,57,57,AARCH64_setpgid),0},"setpgid", OK, RLONG, 2,},
    {{PACKNUM(-1,58,58,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,59,59,AARCH64_ni),0},"olduname", OK, RLONG, 1, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(95,60,60,AARCH64_umask),0},"umask", OK, RLONG, 1,},
    {{PACKNUM(161,61,61,AARCH64_chroot),0},"chroot", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
#ifdef ANDROID
    /* XXX i#1850: no ustat?  Not in my NDK 21 headers. */
    {{PACKNUM(136,62,62,AARCH64_ustat),0},"ustat", UNKNOWN, RLONG, 2,},
#else
    {{PACKNUM(136,62,62,AARCH64_ustat),0},"ustat", OK, RLONG, 2,
     {
         {1, sizeof(struct ustat), W},
     }
    },
#endif
    {{PACKNUM(33,63,63,AARCH64_dup2),0},"dup2", OK, RLONG, 2,},
    {{PACKNUM(110,64,64,AARCH64_getppid),0},"getppid", OK, RLONG, 0,},
    {{PACKNUM(111,65,65,AARCH64_getpgrp),0},"getpgrp", OK, RLONG, 0,},
    {{PACKNUM(112,66,66,AARCH64_setsid),0},"setsid", OK, RLONG, 0,},
    {{PACKNUM(-1,67,67,AARCH64_ni),0},"sigaction", OK, RLONG, 3,/*FIXME type: {{1, sizeof(struct old_sigaction), W}, {2, sizeof(struct old_sigaction), R},}*/},
    {{PACKNUM(-1,68,68,AARCH64_ni),0},"sgetmask", OK, RLONG, 0,},
    {{PACKNUM(-1,69,69,AARCH64_ni),0},"ssetmask", OK, RLONG, 1,},
    {{PACKNUM(-1,70,70,AARCH64_ni),0},"setreuid16", OK, RLONG, 2,},
    {{PACKNUM(-1,71,71,AARCH64_ni),0},"setregid16", OK, RLONG, 2,},
    {{PACKNUM(-1,72,72,AARCH64_ni),0},"sigsuspend", OK, RLONG, 3,},
    {{PACKNUM(-1,73,73,AARCH64_ni),0},"sigpending", OK, RLONG, 1,/*FIXME type: {{0, sizeof(old_sigset_t), W},}*/},
    {{PACKNUM(170,74,74,AARCH64_sethostname),0},"sethostname", OK, RLONG, 2,
     {
         {0, -1, R},
     }
    },
    {{PACKNUM(160,75,75,AARCH64_setrlimit),0},"setrlimit", OK, RLONG, 2,
     {
         {1, sizeof(struct rlimit), R},
     }
    },
    {{PACKNUM(-1,76,76,AARCH64_ni),0},"old_getrlimit", OK, RLONG, 2,
     {
         {1, sizeof(struct rlimit), W},
     }
    },
    {{PACKNUM(98,77,77,AARCH64_getrusage),0},"getrusage", OK, RLONG, 2,
     {
         {1, sizeof(struct rusage), W},
     }
    },
    {{PACKNUM(96,78,78,AARCH64_gettimeofday),0},"gettimeofday", OK, RLONG, 2,
     {
         {0, sizeof(struct timeval), W},
         {1, sizeof(struct timezone), W},
     }
    },
    {{PACKNUM(164,79,79,AARCH64_settimeofday),0},"settimeofday", OK, RLONG, 2,
     {
         {0, sizeof(struct timeval), R},
         {1, sizeof(struct timezone), R},
     }
    },
    {{PACKNUM(-1,80,80,AARCH64_ni),0},"getgroups16", OK, RLONG, 2,/* FIXME how encode these: {{1, ARG1 * sizeof(vki_old_gid_t), W}, {1, RES * sizeof(vki_old_gid_t), W},}*/},
    {{PACKNUM(-1,81,81,AARCH64_ni),0},"setgroups16", OK, RLONG, 2,/* FIXME how encode these:{{1, ARG1 * sizeof(vki_old_gid_t), R},}*/},
    {{PACKNUM(-1,82,82,AARCH64_ni),0},"old_select", OK, RLONG, /*FIXME*/},
    {{PACKNUM(88,83,83,AARCH64_symlink),0},"symlink", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
     }
    },
#ifndef AARCH64
    {{PACKNUM(-1,84,84,AARCH64_lstat),0},"lstat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct __old_kernel_stat), W},
     }
    },
#endif
    {{PACKNUM(6,-1,-1,AARCH64_lstat),0},"lstat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(89,85,85,AARCH64_readlink),0},"readlink", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(134,86,86,AARCH64_uselib),0},"uselib", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(167,87,87,AARCH64_swapon),0},"swapon", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(169,88,88,AARCH64_reboot),0},"reboot", OK, RLONG, 4, /*FIXME: 3 is optional*/},
    {{PACKNUM(-1,89,89,AARCH64_ni),0},"old_readdir", OK, RLONG, 3,/*FIXME type: {{1, sizeof(struct old_linux_dirent), W},}*/},
    {{PACKNUM(-1,90,90,AARCH64_ni),0},"mmap", OK, RLONG, /*FIXME*/},
    {{PACKNUM(11,91,91,AARCH64_munmap),0},"munmap", OK, RLONG, 2,},
    /* XXX i#822: for framework w/ inlined types we'll need separate x64 entries */
    {{PACKNUM(76,92,92,AARCH64_truncate),0},"truncate", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    /* XXX i#822: for framework w/ inlined types we'll need separate x64 entries */
    {{PACKNUM(77,93,93,AARCH64_ftruncate),0},"ftruncate", OK, RLONG, 2,},
    {{PACKNUM(91,94,94,AARCH64_fchmod),0},"fchmod", OK, RLONG, 2,},
    {{PACKNUM(-1,95,95,AARCH64_ni),0},"fchown16", OK, RLONG, 3,},
    {{PACKNUM(140,96,96,AARCH64_getpriority),0},"getpriority", OK, RLONG, 2,},
    {{PACKNUM(141,97,97,AARCH64_setpriority),0},"setpriority", OK, RLONG, 3,},
    {{PACKNUM(-1,98,98,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(137,99,99,AARCH64_statfs),0},"statfs", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct statfs), W},
     }
    },
    {{PACKNUM(138,100,100,AARCH64_fstatfs),0},"fstatfs", OK, RLONG, 2,
     {
         {1, sizeof(struct statfs), W},
     }
    },
    {{PACKNUM(173,101,101,AARCH64_ni),0},"ioperm", OK, RLONG, 3,},
    {{PACKNUM(-1,102,102,AARCH64_ni),0},"socketcall", OK, RLONG, 2, /* special-cased below */},
    {{PACKNUM(103,103,103,AARCH64_syslog),0},"syslog", OK, RLONG, 3,
     {
         {1, -2, W},
     }
    },
    {{PACKNUM(38,104,104,AARCH64_setitimer),0},"setitimer", OK, RLONG, 3,
     {
         {1, sizeof(struct itimerval), R},
         {2, sizeof(struct itimerval), W},
     }
    },
    {{PACKNUM(36,105,105,AARCH64_getitimer),0},"getitimer", OK, RLONG, 2,
     {
         {1, sizeof(struct itimerval), W},
     }
    },
    {{PACKNUM(-1,106,106,AARCH64_ni),0},"newstat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(-1,107,107,AARCH64_ni),0},"newlstat", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(-1,108,108,AARCH64_ni),0},"newfstat", OK, RLONG, 2,
     {
         {1, sizeof(struct stat), W},
     }
    },
    {{PACKNUM(63,109,109,AARCH64_uname),0},"uname", OK, RLONG, 1, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(172,110,110,AARCH64_ni),0},"iopl", OK, RLONG, 1,},
    {{PACKNUM(153,111,111,AARCH64_vhangup),0},"vhangup", OK, RLONG, 0,},
    {{PACKNUM(-1,112,112,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,113,113,AARCH64_ni),0},"vm86old", OK, RLONG, 1, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(61,114,114,AARCH64_wait4),0},"wait4", OK, RLONG, 4,
     {
         {1, sizeof(int), W},
         {3, sizeof(struct rusage), W},
     }
    },
    {{PACKNUM(168,115,115,AARCH64_swapoff),0},"swapoff", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(99,116,116,AARCH64_sysinfo),0},"sysinfo", OK, RLONG, 1,
     {
         {0, sizeof(struct sysinfo), W},
     }
    },
    {{PACKNUM(-1,117,117,AARCH64_ni),0},"ipc", OK, RLONG, 1, /* special-cased below */ },
    {{PACKNUM(74,118,118,AARCH64_fsync),0},"fsync", OK, RLONG, 1,},
    {{PACKNUM(-1,119,119,AARCH64_ni),0},"sigreturn", OK, RLONG, 0},
    {{PACKNUM(56,120,120,AARCH64_clone),0},"clone", OK, RLONG, 2,}, /* 3 params added in later kernels special-cased */
    {{PACKNUM(171,121,121,AARCH64_setdomainname),0},"setdomainname", OK, RLONG, 2,
     {
         {0, -1, R},
     }
    },
    {{PACKNUM(-1,122,122,AARCH64_ni),0},"newuname", OK, RLONG, 1,
     {
         {0, sizeof(struct new_utsname), W},
     }
    },
    {{PACKNUM(154,123,123,AARCH64_ni),0},"modify_ldt", OK, RLONG, 3, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(159,124,124,AARCH64_adjtimex),0},"adjtimex", OK, RLONG, 1,
     {
         {0, sizeof(struct timex), R},
     }
    },
    {{PACKNUM(10,125,125,AARCH64_mprotect),0},"mprotect", OK, RLONG, 3,},
    {{PACKNUM(-1,126,126,AARCH64_ni),0},"sigprocmask", OK, RLONG, 3,/*FIXME type: {{1, sizeof(old_sigset_t), R}, {2, sizeof(old_sigset_t), W},}*/},
    {{PACKNUM(-1,127,127,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(175,128,128,AARCH64_init_module),0},"init_module", OK, RLONG, 3,
     {
         {0, -1, R},
         {2,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(176,129,129,AARCH64_delete_module),0},"delete_module", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,130,130,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(179,131,131,AARCH64_quotactl),0},"quotactl", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING}, /* FIXME: #3 varies */
     }
    },
    {{PACKNUM(121,132,132,AARCH64_getpgid),0},"getpgid", OK, RLONG, 1,},
    {{PACKNUM(81,133,133,AARCH64_fchdir),0},"fchdir", OK, RLONG, 1,},
    {{PACKNUM(-1,134,134,AARCH64_bdflush),0},"bdflush", OK, RLONG, 2,},
    {{PACKNUM(139,135,135,AARCH64_ni),0},"sysfs", OK, RLONG, 3,},
    {{PACKNUM(135,136,136,AARCH64_personality),0},"personality", OK, RLONG, 1,},
    {{PACKNUM(-1,137,137,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,138,138,AARCH64_ni),0},"setfsuid16", OK, RLONG, 1,},
    {{PACKNUM(-1,139,139,AARCH64_ni),0},"setfsgid16", OK, RLONG, 1,},
    {{PACKNUM(-1,140,140,AARCH64_llseek),0},"llseek", OK, RLONG, 5,
     {
         {3, sizeof(loff_t), W},
     }
    },
    {{PACKNUM(78,141,141,AARCH64_getdents),0},"getdents", OK, RLONG, 3,
     {
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(23,142,142,AARCH64_select),0},"select", OK, RLONG, 5,/* special-cased below */},
    {{PACKNUM(73,143,143,AARCH64_flock),0},"flock", OK, RLONG, 2,},
    {{PACKNUM(26,144,144,AARCH64_msync),0},"msync", OK, RLONG, 3,
     {
         {0, -1, R},
     }
    },
    {{PACKNUM(19,145,145,AARCH64_readv),0},"readv", OK, RLONG, 3, /* FIXME 1, ARG3 * sizeof(struct vki_iovec), R, 1,****** special-case:  (Addr)vec[i].iov_base, nReadThisBuf, R, */},
    {{PACKNUM(20,146,146,AARCH64_writev),0},"writev", OK, RLONG, 3, /* FIXME 1, ARG3 * sizeof(struct vki_iovec), R, 1,****** special-case:  "writev(vector[...])", OK, RLONG, (Addr)vec[i].iov_base, vec[i].iov_len, R, */},
    {{PACKNUM(124,147,147,AARCH64_getsid),0},"getsid", OK, RLONG, 1,},
    {{PACKNUM(75,148,148,AARCH64_fdatasync),0},"fdatasync", OK, RLONG, 1,},
    {{PACKNUM(156,149,149,AARCH64_ni),0},"_sysctl", OK, RLONG, 1,
     {
         {0, sizeof(struct __sysctl_args), R},
     }
    },/*special-cased*/
    {{PACKNUM(149,150,150,AARCH64_mlock),0},"mlock", OK, RLONG, 2,},
    {{PACKNUM(150,151,151,AARCH64_munlock),0},"munlock", OK, RLONG, 2,},
    {{PACKNUM(151,152,152,AARCH64_mlockall),0},"mlockall", OK, RLONG, 1,},
    {{PACKNUM(152,153,153,AARCH64_munlockall),0},"munlockall", OK, RLONG, 0,},
    {{PACKNUM(142,154,154,AARCH64_sched_setparam),0},"sched_setparam", OK, RLONG, 2,
     {
         {1, sizeof(struct sched_param), R},
     }
    },
    {{PACKNUM(143,155,155,AARCH64_sched_getparam),0},"sched_getparam", OK, RLONG, 2,
     {
         {1, sizeof(struct sched_param), W},
     }
    },
    {{PACKNUM(144,156,156,AARCH64_sched_setscheduler),0},"sched_setscheduler", OK, RLONG, 3,
     {
         {2, sizeof(struct sched_param), R},
     }
    },
    {{PACKNUM(145,157,157,AARCH64_sched_getscheduler),0},"sched_getscheduler", OK, RLONG, 1,},
    {{PACKNUM(24,158,158,AARCH64_sched_yield),0},"sched_yield", OK, RLONG, 0,},
    {{PACKNUM(146,159,159,AARCH64_sched_get_priority_max),0},"sched_get_priority_max", OK, RLONG, 1,},
    {{PACKNUM(147,160,160,AARCH64_sched_get_priority_min),0},"sched_get_priority_min", OK, RLONG, 1,},
    {{PACKNUM(148,161,161,AARCH64_sched_rr_get_interval),0},"sched_rr_get_interval", OK, RLONG, 2, /* FIXME  1, sizeof(struct timespec), U, */},
    {{PACKNUM(35,162,162,AARCH64_nanosleep),0},"nanosleep", OK, RLONG, 2,
     {
         {0, sizeof(struct timespec), R},
         {1, sizeof(struct timespec), W},
     }
    },
    {{PACKNUM(25,163,163,AARCH64_mremap),0},"mremap", OK, RLONG, 4,}, /* 5th arg is conditional so special-cased below */
    {{PACKNUM(-1,164,164,AARCH64_ni),0},"setresuid16", OK, RLONG, 3,},
    {{PACKNUM(-1,165,165,AARCH64_ni),0},"getresuid16", OK, RLONG, 3,/*FIXME type: {{0, sizeof(old_uid_t), W}, {1, sizeof(old_uid_t), W}, {2, sizeof(old_uid_t), W},}*/},
    {{PACKNUM(-1,166,166,AARCH64_ni),0},"vm86", OK, RLONG, 2, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(-1,167,167,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(7,168,168,AARCH64_poll),0},"poll", OK, RLONG, 3, /* special-cased below */},
    {{PACKNUM(180,169,169,AARCH64_nfsservctl),0},"nfsservctl", OK, RLONG, 3, /* FIXME 1, sizeof(struct nfsctl_arg), U, 2, sizeof(void), U, */},
    {{PACKNUM(-1,170,170,AARCH64_ni),0},"setresgid16", OK, RLONG, 3,},
    {{PACKNUM(-1,171,171,AARCH64_ni),0},"getresgid16", OK, RLONG, 3,/*FIXME type: {{0, sizeof(old_gid_t), W}, {1, sizeof(old_gid_t), W}, {2, sizeof(old_gid_t), W},}*/},
    {{PACKNUM(157,172,172,AARCH64_prctl),0},"prctl", OK, RLONG, 1, /* special-cased below */},
    {{PACKNUM(15,173,173,AARCH64_rt_sigreturn),0},"rt_sigreturn", OK, RLONG, 0},
    {{PACKNUM(13,174,174,AARCH64_rt_sigaction),0},"rt_sigaction", OK, RLONG, 4,/*1 is special-cased below*/{{2, sizeof(kernel_sigaction_t), W},
     }
    },
    {{PACKNUM(14,175,175,AARCH64_rt_sigprocmask),0},"rt_sigprocmask", OK, RLONG, 4,
     {
         {1, sizeof(kernel_sigset_t), R},
         {2, sizeof(kernel_sigset_t), W},
     }
    },
    {{PACKNUM(127,176,176,AARCH64_rt_sigpending),0},"rt_sigpending", OK, RLONG, 2,
     {
         {0, sizeof(kernel_sigset_t), W},
     }
    },
    {{PACKNUM(128,177,177,AARCH64_rt_sigtimedwait),0},"rt_sigtimedwait", OK, RLONG, 4,
     {
         {0, sizeof(kernel_sigset_t), R},
         {1, sizeof(siginfo_t), W},
         {2, sizeof(struct timespec), R},
     }
    },
    {{PACKNUM(129,178,178,AARCH64_rt_sigqueueinfo),0},"rt_sigqueueinfo", OK, RLONG, 3,
     {
         {2, sizeof(siginfo_t), R},
     }
    },
    {{PACKNUM(130,179,179,AARCH64_rt_sigsuspend),0},"rt_sigsuspend", OK, RLONG, 2, /* FIXME 0, sizeof(siginfo_t), R, 0,****** special-case:  arg2, sizeof(struct vki_msqid64_ds), R, */},
    {{PACKNUM(17,180,180,AARCH64_pread64),0},"pread64", OK, RLONG, 4,
     {
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(18,181,181,AARCH64_pwrite64),0},"pwrite64", OK, RLONG, 4,
     {
         {1, -2, R},
     }
    },
    {{PACKNUM(-1,182,182,AARCH64_ni),0},"chown16", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(79,183,183,AARCH64_getcwd),0},"getcwd", OK, RLONG, 2,
     {
         {0, -1, W},
         {0, RET, W},
     }
    },
    {{PACKNUM(125,184,184,AARCH64_capget),0},"capget", OK, RLONG, 2,
     {
         {0, sizeof(cap_user_header_t), R},
         {1, sizeof(cap_user_data_t), W},
     }
    },
    {{PACKNUM(126,185,185,AARCH64_capset),0},"capset", OK, RLONG, 2,
     {
         {0, sizeof(cap_user_header_t), R},
         {1, sizeof(cap_user_data_t), R},
     }
    },
    {{PACKNUM(131,186,186,AARCH64_sigaltstack),0},"sigaltstack", OK, RLONG, 2, /* FIXME 0,****** special-case:  "sigaltstack(ss)", OK, RLONG, (Addr)&ss->ss_sp, sizeof(ss->ss_sp), R, 0,****** special-case:  "sigaltstack(ss)", OK, RLONG, (Addr)&ss->ss_size, sizeof(ss->ss_size), R, {1, sizeof(cap_user_data_t data), W}, */},
    {{PACKNUM(40,187,187,AARCH64_sendfile),0},"sendfile", OK, RLONG, 4,
     {
         {2, sizeof(off_t), W},
     }
    },
    {{PACKNUM(-1,188,188,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,189,189,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(58,190,190,AARCH64_vfork),0},"vfork", OK, RLONG, 0,},
    {{PACKNUM(97,191,191,AARCH64_getrlimit),0},"getrlimit", OK, RLONG, 2,
     {
         {1, sizeof(struct rlimit), W},
     }
    },
    {{PACKNUM(-1,192,192,AARCH64_ni),0},"mmap2", OK, RLONG, 6,},
    {{PACKNUM(9,-1,-1,AARCH64_mmap),0},  "mmap",  OK, RLONG, 6,},
    {{PACKNUM(-1,193,193,AARCH64_truncate64),0},"truncate64", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(-1,194,194,AARCH64_ftruncate64),0},"ftruncate64", OK, RLONG, 2,},
#ifndef X64
    /* XXX i#1013: we'll need our own defs of stat64 for mixed-mode */
    {{PACKNUM(-1,195,195,AARCH64_stat64),0},"stat64", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat64), W},
     }
    },
    {{PACKNUM(-1,196,196,AARCH64_lstat64),0},"lstat64", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1, sizeof(struct stat64), W},
     }
    },
    {{PACKNUM(-1,197,197,AARCH64_fstat64),0},"fstat64", OK, RLONG, 2,
     {
         {1, sizeof(struct stat64), W,}
     }
    },
#endif
    {{PACKNUM(94,198,198,AARCH64_lchown),0},"lchown", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(102,199,199,AARCH64_getuid),0},"getuid", OK, RLONG, 0,},
    {{PACKNUM(104,200,200,AARCH64_getgid),0},"getgid", OK, RLONG, 0,},
    {{PACKNUM(107,201,201,AARCH64_geteuid),0},"geteuid", OK, RLONG, 0,},
    {{PACKNUM(108,202,202,AARCH64_getegid),0},"getegid", OK, RLONG, 0,},
    {{PACKNUM(113,203,203,AARCH64_setreuid),0},"setreuid", OK, RLONG, 2,},
    {{PACKNUM(114,204,204,AARCH64_setregid),0},"setregid", OK, RLONG, 2,},
    {{PACKNUM(115,205,205,AARCH64_getgroups),0},"getgroups", OK, RLONG, 2,/*FIXME{{1, ARG1 * sizeof(vki_gid_t), W}, {1, RES * sizeof(vki_gid_t), W},}*/},
    {{PACKNUM(116,206,206,AARCH64_setgroups),0},"setgroups", OK, RLONG, 2,/*FIXME{{1, ARG1 * sizeof(vki_gid_t), R},}*/},
    {{PACKNUM(93,207,207,AARCH64_fchown),0},"fchown", OK, RLONG, 3,},
    {{PACKNUM(117,208,208,AARCH64_setresuid),0},"setresuid", OK, RLONG, 3,},
    {{PACKNUM(118,209,209,AARCH64_getresuid),0},"getresuid", OK, RLONG, 3,
     {
         {0, sizeof(uid_t), W},
         {1, sizeof(uid_t), W},
         {2, sizeof(uid_t), W},
     }
    },
    {{PACKNUM(119,210,210,AARCH64_setresgid),0},"setresgid", OK, RLONG, 3,},
    {{PACKNUM(120,211,211,AARCH64_getresgid),0},"getresgid", OK, RLONG, 3,
     {
         {0, sizeof(gid_t), W},
         {1, sizeof(gid_t), W},
         {2, sizeof(gid_t), W},
     }
    },
    {{PACKNUM(92,212,212,AARCH64_chown),0},"chown", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(105,213,213,AARCH64_setuid),0},"setuid", OK, RLONG, 1,},
    {{PACKNUM(106,214,214,AARCH64_setgid),0},"setgid", OK, RLONG, 1,},
    {{PACKNUM(122,215,215,AARCH64_setfsuid),0},"setfsuid", OK, RLONG, 1,},
    {{PACKNUM(123,216,216,AARCH64_setfsgid),0},"setfsgid", OK, RLONG, 1,},

    /* Slight divergence in ARM vs x86 for these 4 */
    {{PACKNUM(155,217,218,AARCH64_pivot_root),0},"pivot_root", OK, RLONG, 2, /* FIXME 0, sizeof(char), U, 1, sizeof(char), U, */},
    {{PACKNUM(27,218,219,AARCH64_mincore),0},"mincore", OK, RLONG, 3,
     {
         {2,/*FIXME: round up to next page size*/-1, W},
     }
    },
    {{PACKNUM(28,219,220,AARCH64_madvise),0},"madvise", OK, RLONG, 3,},
    {{PACKNUM(217,220,217,AARCH64_getdents64),0},"getdents64", OK, RLONG, 3,
     {
         {1, -2, W},
         {1, RET, W},
     }
    },

    {{PACKNUM(-1,221,221,AARCH64_fcntl64),0},"fcntl64", OK, RLONG, 2,}, /*special-cased: 3rd arg not always required*/
    {{PACKNUM(-1,222,222,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(-1,223,223,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(186,224,224,AARCH64_gettid),0},"gettid", OK, RLONG, 0,},
    {{PACKNUM(187,225,225,AARCH64_readahead),0},"readahead", OK, RLONG, 3,},
    {{PACKNUM(188,226,226,AARCH64_setxattr),0},"setxattr", OK, RLONG, 5,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2, -3, R},
     }
    },
    {{PACKNUM(189,227,227,AARCH64_lsetxattr),0},"lsetxattr", OK, RLONG, 5,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2, -3, R},
     }
    },
    {{PACKNUM(190,228,228,AARCH64_fsetxattr),0},"fsetxattr", OK, RLONG, 5,
     {
         {1,0, R|CT, CSTRING},
         {2, -3, R},
     }
    },
    {{PACKNUM(191,229,229,AARCH64_getxattr),0},"getxattr", OK, RLONG, 4,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2, -3, W},
         {2, RET, W},
     }
    },
    {{PACKNUM(192,230,230,AARCH64_lgetxattr),0},"lgetxattr", OK, RLONG, 4,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2, -3, W},
         {2, RET, W},
     }
    },
    {{PACKNUM(193,231,231,AARCH64_fgetxattr),0},"fgetxattr", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {2, -3, W},
         {2, RET, W},
     }
    },
    {{PACKNUM(194,232,232,AARCH64_listxattr),0},"listxattr", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(195,233,233,AARCH64_llistxattr),0},"llistxattr", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(196,234,234,AARCH64_flistxattr),0},"flistxattr", OK, RLONG, 3,
     {
         {1, -2, W},
         {1, RET, W},
     }
    },
    {{PACKNUM(197,235,235,AARCH64_removexattr),0},"removexattr", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(198,236,236,AARCH64_lremovexattr),0},"lremovexattr", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(199,237,237,AARCH64_fremovexattr),0},"fremovexattr", OK, RLONG, 2,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(200,238,238,AARCH64_tkill),0},"tkill", OK, RLONG, 2,},
    {{PACKNUM(-1,239,239,AARCH64_sendfile64),0},"sendfile64", OK, RLONG, 4,
     {
         {2, sizeof(loff_t), W},
     }
    },
    {{PACKNUM(202,240,240,AARCH64_futex),0},"futex", OK, RLONG, 3,
     {
         {0, sizeof(uint), R},
     }
    },/*rest are special-cased*/
    {{PACKNUM(203,241,241,AARCH64_sched_setaffinity),0},"sched_setaffinity", OK, RLONG, 3,
     {
         {2, -1, R},
     }
    },
    {{PACKNUM(204,242,242,AARCH64_sched_getaffinity),0},"sched_getaffinity", OK, RLONG, 3,
     {
         {2, -1, W},
     }
    },

    /* ARM numbers are off after skipping these 2 x86-only syscalls */
    {{PACKNUM(205,243,-1,AARCH64_ni),0},"set_thread_area", OK, RLONG, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(211,244,-1,AARCH64_ni),0},"get_thread_area", OK, RLONG, /* FIXME: ***Missing prototype*** */ },
    {{PACKNUM(206,245,243,AARCH64_io_setup),0},"io_setup", OK, RLONG, 2,/*FIXME type: {{1, sizeof(aio_context_t), W},}*/},
    {{PACKNUM(207,246,244,AARCH64_io_destroy),0},"io_destroy", OK, RLONG, 1,},
    {{PACKNUM(208,247,245,AARCH64_io_getevents),0},"io_getevents", OK, RLONG, 5, /* FIXME 3, sizeof(struct io_event), W, 3,****** special-case:  cb->aio_buf, vev->result, W,{4, sizeof(struct timespec), R}, */},
    {{PACKNUM(209,248,246,AARCH64_io_submit),0},"io_submit", OK, RLONG, 3, /* FIXME 2, ARG2*sizeof(struct vki_iocb *), R, 2,****** special-case:  "io_submit(PWRITE)", OK, RLONG, cb->aio_buf, cb->aio_nbytes, R, */},
    {{PACKNUM(210,249,247,AARCH64_io_cancel),0},"io_cancel", OK, RLONG, 3,/* FIXME type: {{1, sizeof(struct iocb), R},{2, sizeof(struct io_event), W},}*/},
    {{PACKNUM(221,250,-1,AARCH64_fadvise64),0},"fadvise64", OK, RLONG, 4,},
    {{PACKNUM(-1,251,-1,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(231,252,248,AARCH64_exit_group),0},"exit_group", OK, RLONG, 1,},
    {{PACKNUM(212,253,249,AARCH64_lookup_dcookie),0},"lookup_dcookie", OK, RLONG, 3, /* FIXME 1, sizeof(char), U,{2, -3, W},{2, RET, W}, */},
    {{PACKNUM(213,254,250,AARCH64_epoll_create),0},"epoll_create", OK, RLONG, 1,},
    {{PACKNUM(233,255,251,AARCH64_epoll_ctl),0},"epoll_ctl", OK, RLONG, 4,
     {
         {3, sizeof(struct epoll_event), R},
     }
    },
    {{PACKNUM(232,256,252,AARCH64_epoll_wait),0},"epoll_wait", OK, RLONG, 4,
     {
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct epoll_event)},
         {1, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct epoll_event)},
     }
    },
    {{PACKNUM(216,257,253,AARCH64_remap_file_pages),0},"remap_file_pages", OK, RLONG, 5,},
    {{PACKNUM(218,258,256,AARCH64_set_tid_address),0},"set_tid_address", OK, RLONG, 1, /* FIXME 0, sizeof(int), U, */},
    {{PACKNUM(222,259,257,AARCH64_timer_create),0},"timer_create", OK, RLONG, 3,
     {
         {1, sizeof(struct sigevent), R},
         {2, sizeof(timer_t), W},
     }
    },
    {{PACKNUM(223,260,258,AARCH64_timer_settime),0},"timer_settime", OK, RLONG, 4,
     {
         {2, sizeof(struct itimerspec), R},
         {3, sizeof(struct itimerspec), W},
     }
    },
    {{PACKNUM(224,261,259,AARCH64_timer_gettime),0},"timer_gettime", OK, RLONG, 2,
     {
         {1, sizeof(struct itimerspec), W},
     }
    },
    {{PACKNUM(225,262,260,AARCH64_timer_getoverrun),0},"timer_getoverrun", OK, RLONG, 1,},
    {{PACKNUM(226,263,261,AARCH64_timer_delete),0},"timer_delete", OK, RLONG, 1,},
    {{PACKNUM(227,264,262,AARCH64_clock_settime),0},"clock_settime", OK, RLONG, 2,
     {
         {1, sizeof(struct timespec), R},
     }
    },
    {{PACKNUM(228,265,263,AARCH64_clock_gettime),0},"clock_gettime", OK, RLONG, 2,
     {
         {1, sizeof(struct timespec), W},
     }
    },
    {{PACKNUM(229,266,264,AARCH64_clock_getres),0},"clock_getres", OK, RLONG, 2,
     {
         {1, sizeof(struct timespec), W},
     }
    },
    {{PACKNUM(230,267,265,AARCH64_clock_nanosleep),0},"clock_nanosleep", OK, RLONG, 4,
     {
         {2, sizeof(struct timespec), R},
         {3, sizeof(struct timespec), W},
     }
    },
    {{PACKNUM(-1,268,266,AARCH64_statfs64),0},"statfs64", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
         {2, -1, W},
     }
    },
    {{PACKNUM(-1,269,267,AARCH64_fstatfs64),0},"fstatfs64", OK, RLONG, 3,
     {
         {2, -1, W},
     }
    },
    {{PACKNUM(234,270,268,AARCH64_tgkill),0},"tgkill", OK, RLONG, 3,},
    {{PACKNUM(235,271,269,AARCH64_utimes),0},"utimes", OK, RLONG, 2,
     {
         {0,0, R|CT, CSTRING},
         {1,2 * sizeof(struct timeval), R},
     }
    },
    {{PACKNUM(-1,272,270,AARCH64_fadvise64_64),0},"fadvise64_64", OK, RLONG, 4,},
    {{PACKNUM(-1,273,-1,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(237,274,319,AARCH64_mbind),0},"mbind", OK, RLONG, 6, /*FIXME {{3, VG_ROUNDUP(ARG5, sizeof(UWord))/sizeof(UWord), R,},}*/},
    {{PACKNUM(239,275,320,AARCH64_get_mempolicy),0},"get_mempolicy", OK, RLONG, 5,/*FIXME {{0, sizeof(int), W}, {1, VG_ROUNDUP(ARG3, sizeof(UWord)*8)/sizeof(UWord), W},}*/},
    {{PACKNUM(238,276,321,AARCH64_set_mempolicy),0},"set_mempolicy", OK, RLONG, 3, /*FIXME {{1, VG_ROUNDUP(ARG3, sizeof(UWord))/sizeof(UWord), R},}*/},
    {{PACKNUM(240,277,274,AARCH64_mq_open),0},"mq_open", OK, RLONG, 4, /* FIXME 0, CSTRING, R, 0,****** special-case:  "mq_open(attr->mq_msgsize)", OK, RLONG, (Addr)&attr->mq_msgsize, sizeof(attr->mq_msgsize), R, 3, sizeof(struct mq_attr), U, */},
    {{PACKNUM(241,278,275,AARCH64_mq_unlink),0},"mq_unlink", OK, RLONG, 1,
     {
         {0,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(242,279,276,AARCH64_mq_timedsend),0},"mq_timedsend", OK, RLONG, 5,
     {
         {1, -2, R},
         {4, sizeof(struct timespec), R
     }
    },},
    {{PACKNUM(243,280,277,AARCH64_mq_timedreceive),0},"mq_timedreceive", OK, RLONG, 5,
     {
         {1, -2, W},
         {3, sizeof(unsigned int), W},
         {4, sizeof(struct timespec), R
     }
    },},
    {{PACKNUM(244,281,278,AARCH64_mq_notify),0},"mq_notify", OK, RLONG, 2,
     {
         {1, sizeof(struct sigevent), R},
     }
    },
    {{PACKNUM(245,282,279,AARCH64_mq_getsetattr),0},"mq_getsetattr", OK, RLONG, 3, /* FIXME 1,****** special-case:  "mq_getsetattr(mqstat->mq_flags)", OK, RLONG, (Addr)&attr->mq_flags, sizeof(attr->mq_flags), R,{2, sizeof(struct mq_attr), W}, */},
    {{PACKNUM(246,283,347,AARCH64_kexec_load),0},"kexec_load", OK, RLONG, 4, /* FIXME 2, sizeof(struct kexec_segment), U, */},
    {{PACKNUM(247,284,280,AARCH64_waitid),0},"waitid", OK, RLONG, 5,
     {
         {2, sizeof(siginfo_t), W},
         {4, sizeof(struct rusage), W},
     }
    },
    {{PACKNUM(-1,285,-1,AARCH64_ni),0},"ni_syscall", OK, RLONG, 0,},
    {{PACKNUM(248,286,309,AARCH64_add_key),0},"add_key", OK, RLONG, 5,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2, -3, R},
     }
    },
    {{PACKNUM(249,287,310,AARCH64_request_key),0},"request_key", OK, RLONG, 4,
     {
         {0,0, R|CT, CSTRING},
         {1,0, R|CT, CSTRING},
         {2,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(250,288,311,AARCH64_keyctl),0},"keyctl", OK, RLONG, 5,
     {
         {1,0, R|CT, CSTRING},
         {2, -3, R},
         {2, RET, R},
         {3,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(251,289,314,AARCH64_ioprio_set),0},"ioprio_set", OK, RLONG, 3,},
    {{PACKNUM(252,290,315,AARCH64_ioprio_get),0},"ioprio_get", OK, RLONG, 2,},
    {{PACKNUM(253,291,316,AARCH64_inotify_init),0},"inotify_init", OK, RLONG, 0,},
    {{PACKNUM(254,292,317,AARCH64_inotify_add_watch),0},"inotify_add_watch", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(255,293,318,AARCH64_inotify_rm_watch),0},"inotify_rm_watch", OK, RLONG, 2,},
    {{PACKNUM(256,294,-1,AARCH64_migrate_pages),0},"migrate_pages", OK, RLONG, 4, /* FIXME 2, sizeof(unsigned long), U, 3, sizeof(unsigned long), U, */},
    {{PACKNUM(257,295,322,AARCH64_openat),0},"openat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(258,296,323,AARCH64_mkdirat),0},"mkdirat", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(259,297,324,AARCH64_mknodat),0},"mknodat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(260,298,325,AARCH64_fchownat),0},"fchownat", OK, RLONG, 5,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(261,299,326,AARCH64_futimesat),0},"futimesat", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
         {2,2 * sizeof(struct timeval), R},
     }
    },
#ifndef X64
    /* XXX i#1013: we'll need our own defs of stat64 for mixed-mode */
    {{PACKNUM(-1,300,327,AARCH64_fstatat64),0},"fstatat64", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {2,sizeof(struct stat64),W},
     }
    },
#endif
    {{PACKNUM(263,301,328,AARCH64_unlinkat),0},"unlinkat", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(264,302,329,AARCH64_renameat),0},"renameat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {3,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(265,303,330,AARCH64_linkat),0},"linkat", OK, RLONG, 5,
     {
         {1,0, R|CT, CSTRING},
         {3,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(266,304,331,AARCH64_symlinkat),0},"symlinkat", OK, RLONG, 3,
     {
         {0,0, R|CT, CSTRING},
         {2,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(267,305,332,AARCH64_readlinkat),0},"readlinkat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {2, -3, W},
         {2, RET, W},
     }
    },
    {{PACKNUM(268,306,333,AARCH64_fchmodat),0},"fchmodat", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(269,307,334,AARCH64_faccessat),0},"faccessat", OK, RLONG, 3,
     {
         {1,0, R|CT, CSTRING},
     }
    },
    {{PACKNUM(270,308,335,AARCH64_pselect6),0},"pselect6", OK, RLONG, 6, /* special-cased below */},
    {{PACKNUM(271,309,336,AARCH64_ppoll),0},"ppoll", OK, RLONG, 5, /* FIXME 0, sizeof(struct pollfd), U,{2, sizeof(struct timespec), R},{3, sizeof(kernel_sigset_t), R}, 3,****** special-case:  (Addr)(&ufds[i].revents), sizeof(ufds[i].revents), R, */},
    {{PACKNUM(272,310,337,AARCH64_unshare),0},"unshare", OK, RLONG, 1,},
    {{PACKNUM(273,311,338,AARCH64_set_robust_list),0},"set_robust_list", OK, RLONG, 2,
     {
         {0, -1, R},
     }
    },
    {{PACKNUM(274,312,339,AARCH64_get_robust_list),0},"get_robust_list", OK, RLONG, 3,/*FIXME type: {{1, sizeof(struct robust_list_head), W},{2, sizeof(size_t), W},}*/},
    {{PACKNUM(275,313,340,AARCH64_splice),0},"splice", OK, RLONG, 6, /* FIXME 1, sizeof(loff_t), U, 3, sizeof(loff_t), U, */},
    {{PACKNUM(277,314,341,AARCH64_sync_file_range),0},"sync_file_range", OK, RLONG, 4,},
    {{PACKNUM(276,315,342,AARCH64_tee),0},"tee", OK, RLONG, 4,},
    {{PACKNUM(278,316,343,AARCH64_vmsplice),0},"vmsplice", OK, RLONG, 4, /* FIXME 1, sizeof(struct iovec), U, */},
    {{PACKNUM(279,317,344,AARCH64_move_pages),0},"move_pages", OK, RLONG, 6, /* FIXME 2, sizeof(void), U, 3, sizeof(int), U, 4, sizeof(int), U, */},
    {{PACKNUM(309,318,345,AARCH64_getcpu),0},"getcpu", OK, RLONG, 3, /* FIXME 0, sizeof(unsigned), U, 1, sizeof(unsigned), U, 2, sizeof(struct getcpu_cache), U, */},
    {{PACKNUM(281,319,346,AARCH64_epoll_pwait),0},"epoll_pwait", OK, RLONG, 6,
     {
         {1, -2, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct epoll_event)},
         {1, RET, W|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct epoll_event)},
         {4, sizeof(kernel_sigset_t), R},
     }
    },
    {{PACKNUM(280,320,348,AARCH64_utimensat),0},"utimensat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {2,2 * sizeof(struct timespec), R},
     }
    },
    {{PACKNUM(282,321,349,AARCH64_signalfd),0},"signalfd", OK, RLONG, 3,
     {
         {1, sizeof(kernel_sigset_t), R},
     }
    },
    {{PACKNUM(283,322,350,AARCH64_timerfd_create),0},"timerfd_create", OK, RLONG, 2,},
    {{PACKNUM(284,323,351,AARCH64_eventfd),0},"eventfd", OK, RLONG, 1,},
    {{PACKNUM(285,324,352,AARCH64_fallocate),0},"fallocate", OK, RLONG, 4,},
    {{PACKNUM(286,325,353,AARCH64_timerfd_settime),0},"timerfd_settime", OK, RLONG, 4,
     {
         {2, sizeof(struct itimerspec), R},
         {3, sizeof(struct itimerspec), W},
     }
    },
    {{PACKNUM(287,326,354,AARCH64_timerfd_gettime),0},"timerfd_gettime", OK, RLONG, 2,
     {
         {1, sizeof(struct itimerspec), W},
     }
    },
    {{PACKNUM(289,327,355,AARCH64_signalfd4),0},"signalfd4", OK, RLONG, 4, /* FIXME 1, sizeof(kernel_sigset_t), U, */},
    {{PACKNUM(290,328,356,AARCH64_eventfd2),0},"eventfd2", OK, RLONG, 2,},
    {{PACKNUM(291,329,357,AARCH64_epoll_create1),0},"epoll_create1", OK, RLONG, 1,},
    {{PACKNUM(292,330,358,AARCH64_dup3),0},"dup3", OK, RLONG, 3,},
    {{PACKNUM(293,331,359,AARCH64_pipe2),0},"pipe2", OK, RLONG, 2,
     {
         {0, sizeof(int)*2, W},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{PACKNUM(294,332,360,AARCH64_inotify_init1),0},"inotify_init1", OK, RLONG, 1,},

    /* FIXME i#1019: fill these new syscalls in */
    {{PACKNUM(295,333,361,AARCH64_preadv),0},"preadv", UNKNOWN, RLONG, 0, },
    {{PACKNUM(296,334,362,AARCH64_pwritev),0},"pwritev", UNKNOWN, RLONG, 0, },
    {{PACKNUM(297,335,363,AARCH64_rt_tgsigqueueinfo),0},"rt_tgsigqueueinfo", UNKNOWN, RLONG, 0, },
    {{PACKNUM(298,336,364,AARCH64_perf_event_open),0},"perf_event_open", UNKNOWN, RLONG, 0, },
    {{PACKNUM(299,337,365,AARCH64_recvmmsg),0},"recvmmsg", UNKNOWN, RLONG, 0, },
    {{PACKNUM(300,338,367,AARCH64_fanotify_init),0},"fanotify_init", UNKNOWN, RLONG, 0, },
    {{PACKNUM(301,339,368,AARCH64_fanotify_mark),0},"fanotify_mark", UNKNOWN, RLONG, 0, },
    {{PACKNUM(302,340,369,AARCH64_prlimit64),0},"prlimit64", OK, RLONG, 4,
     {
         {0, sizeof(pid_t), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(struct rlimit), R},
         {3, sizeof(struct rlimit), W},
     }
    },
    {{PACKNUM(303,341,370,AARCH64_name_to_handle_at),0},"name_to_handle_at", UNKNOWN, RLONG, 0, },
    {{PACKNUM(304,342,371,AARCH64_open_by_handle_at),0},"open_by_handle_at", UNKNOWN, RLONG, 0, },
    {{PACKNUM(305,343,372,AARCH64_clock_adjtime),0},"clock_adjtime", UNKNOWN, RLONG, 0, },
    {{PACKNUM(306,344,373,AARCH64_syncfs),0},"syncfs", OK, RLONG, 1,
     {
         {0, sizeof(int),SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{PACKNUM(307,345,374,AARCH64_sendmmsg),0},"sendmmsg", UNKNOWN, RLONG, 0, },
    {{PACKNUM(308,346,375,AARCH64_setns),0},"setns", UNKNOWN, RLONG, 0, },
    {{PACKNUM(310,347,376,AARCH64_process_vm_readv),0},"process_vm_readv", OK, RLONG, 6,
     {
         {0, sizeof(pid_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct iovec)},
         {2, sizeof(unsigned long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct iovec)},
         {4, sizeof(unsigned long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{PACKNUM(311,348,377,AARCH64_process_vm_writev),0},"process_vm_writev", OK, RLONG, 6,
     {
         {0, sizeof(pid_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct iovec)},
         {2, sizeof(unsigned long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, -4, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct iovec)},
         {4, sizeof(unsigned long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {5, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{PACKNUM(312,349,-1,AARCH64_kcmp),0},"kcmp", UNKNOWN, RLONG, 0, },
    {{PACKNUM(313,350,-1,AARCH64_finit_module),0},"finit_module", OK, RLONG, 3,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, 0, R|CT, CSTRING},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    /* TODO: Fill in the ones in between. */
    {{PACKNUM(318,355,384,AARCH64_getrandom),0},"getrandom", OK, RLONG, 3,
     {
         {0, -1, W},
         {1, sizeof(size_t), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },
    {{PACKNUM(334,386,398,AARCH64_rseq),0},"rseq", OK, RLONG, 4,
     {
         {0, -1, R},
         {1, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {3, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
     }
    },

    /**************************************************/
    /* 64-bit only (some are on ARM too) */
    {{PACKNUM(29,-1,307,AARCH64_shmget),0},"shmget", OK, RLONG, 3, },
    {{PACKNUM(30,-1,305,AARCH64_shmat),0},"shmat", OK, RLONG, 3, /*FIXME i#1018: mark the shared mem as defined*/ },
    {{PACKNUM(31,-1,308,AARCH64_shmctl),0},"shmctl", OK, RLONG, 3, /*special-cased*/},
    {{PACKNUM(41,-1,281,AARCH64_socket),0},"socket", OK, RLONG, 3, },
    {{PACKNUM(42,-1,283,AARCH64_connect),0},"connect", OK, RLONG, 3,
     {
         {1, -2, R|CT, SYSARG_TYPE_SOCKADDR},
     }
    },
    {{PACKNUM(43,-1,285,AARCH64_accept),0},"accept", OK, RLONG, 3,
     {
         {1, -2, WI|CT, SYSARG_TYPE_SOCKADDR},
         {2, sizeof(socklen_t), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(44,-1,290,AARCH64_sendto),0},"sendto", OK, RLONG, 6,
     {
         {1, -2, R},
         {4, -5, R|CT, SYSARG_TYPE_SOCKADDR},
         {5, sizeof(socklen_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(45,-1,292,AARCH64_recvfrom),0},"recvfrom", OK, RLONG, 6,
     {
         {1, -2, W},
         {4, -5, WI|CT, SYSARG_TYPE_SOCKADDR},
         {5, sizeof(socklen_t), R|W|HT|SYSARG_IGNORE_IF_PREV_NULL, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(46,-1,296,AARCH64_sendmsg),0},"sendmsg", OK, RLONG, 3,
     {
         {1, sizeof(struct msghdr), R|CT, SYSARG_TYPE_MSGHDR},
     }
    },
    {{PACKNUM(47,-1,297,AARCH64_recvmsg),0},"recvmsg", OK, RLONG, 3,
     {
         {1, sizeof(struct msghdr), W|CT, SYSARG_TYPE_MSGHDR},
     }
    },
    {{PACKNUM(48,-1,293,AARCH64_shutdown),0},"shutdown", OK, RLONG, 2, },
    {{PACKNUM(49,-1,282,AARCH64_bind),0},"bind", OK, RLONG, 3,
     {
         {1, -2, R|CT, SYSARG_TYPE_SOCKADDR},
     }
    },
    {{PACKNUM(50,-1,284,AARCH64_listen),0},"listen", OK, RLONG, 2, },
    {{PACKNUM(51,-1,286,AARCH64_getsockname),0},"getsockname", OK, RLONG, 3,
     {
         {1, -2, WI|CT, SYSARG_TYPE_SOCKADDR},
         {2, sizeof(socklen_t), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(52,-1,287,AARCH64_getpeername),0},"getpeername", OK, RLONG, 3,
     {
         {1, -2, WI|CT, SYSARG_TYPE_SOCKADDR},
         {2, sizeof(socklen_t), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(53,-1,288,AARCH64_socketpair),0},"socketpair", OK, RLONG, 4,
     {
         {3,2*sizeof(int), W},
     }
    },
    {{PACKNUM(54,-1,294,AARCH64_setsockopt),0},"setsockopt", OK, RLONG, 5,
     {
         {3, -4, R},
     }
    },
    {{PACKNUM(55,-1,295,AARCH64_getsockopt),0},"getsockopt", OK, RLONG, 5,
     {
         {3, -4, WI},
         {4, sizeof(socklen_t), W|HT, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(64,-1,299,AARCH64_semget),0},"semget", OK, RLONG, 3, },
    {{PACKNUM(65,-1,298,AARCH64_semop),0},"semop", OK, RLONG, 3,
     {
         {1, -2, R|SYSARG_SIZE_IN_ELEMENTS, sizeof(struct sembuf)},
     }
    },
    {{PACKNUM(66,-1,300,AARCH64_semctl),0},"semctl", OK, RLONG, 4, /*special-cased*/},
    {{PACKNUM(67,-1,306,AARCH64_shmdt),0},"shmdt", OK, RLONG, 1, /*FIXME i#1018: mark the un-shared mem as unaddr*/  },
    {{PACKNUM(68,-1,303,AARCH64_msgget),0},"msgget", OK, RLONG, 2, },
    {{PACKNUM(69,-1,301,AARCH64_msgsnd),0},"msgsnd", OK, RLONG, 4,
     {
         {1, -2, R|CT, SYSARG_TYPE_MSGBUF},
     }
    },
    {{PACKNUM(70,-1,302,AARCH64_msgrcv),0},"msgrcv", OK, RLONG, 5,
     {
         {1, -2, W|CT, SYSARG_TYPE_MSGBUF},
     }
    },
    {{PACKNUM(71,-1,304,AARCH64_msgctl),0},"msgctl", OK, RLONG, 3, /*special-cased*/},
    {{PACKNUM(158,384,-1,AARCH64_ni),0},"arch_prctl", OK, RLONG, 2,
     {
         {0, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         /* 2nd arg is special-cased */
     }
    },
    /* FIXME i#1019: fill these in (merge w/ 32-bit parallel entries above if nec) */
    {{PACKNUM(166,-1,-1,AARCH64_umount2),0},"umount2", UNKNOWN, RLONG, 0, },
    {{PACKNUM(174,-1,-1,AARCH64_ni),0},"create_module", UNKNOWN, RLONG, 0, },
    {{PACKNUM(177,-1,-1,AARCH64_ni),0},"get_kernel_syms", UNKNOWN, RLONG, 0, },
    {{PACKNUM(178,-1,-1,AARCH64_ni),0},"query_module", UNKNOWN, RLONG, 0, },
    {{PACKNUM(181,-1,-1,AARCH64_ni),0},"getpmsg", UNKNOWN, RLONG, 0, },
    {{PACKNUM(182,-1,-1,AARCH64_ni),0},"putpmsg", UNKNOWN, RLONG, 0, },
    {{PACKNUM(183,-1,-1,AARCH64_ni),0},"afs_syscall", UNKNOWN, RLONG, 0, },
    {{PACKNUM(184,-1,-1,AARCH64_ni),0},"tuxcall", UNKNOWN, RLONG, 0, },
    {{PACKNUM(185,-1,-1,AARCH64_ni),0},"security", UNKNOWN, RLONG, 0, },
    {{PACKNUM(214,-1,-1,AARCH64_ni),0},"epoll_ctl_old", UNKNOWN, RLONG, 0, },
    {{PACKNUM(215,-1,-1,AARCH64_ni),0},"epoll_wait_old", UNKNOWN, RLONG, 0, },
    {{PACKNUM(220,-1,-1,AARCH64_semtimedop),0},"semtimedop", OK, RLONG, 4,
     {
         {1,-2,R|SYSARG_SIZE_IN_ELEMENTS,sizeof(struct sembuf)},
         {3,sizeof(struct timespec),R},
     }
    },
    {{PACKNUM(236,-1,-1,AARCH64_ni),0},"vserver", UNKNOWN, RLONG, 0, },
    {{PACKNUM(262,-1,-1,AARCH64_newfstatat),0},"newfstatat", OK, RLONG, 4,
     {
         {1,0, R|CT, CSTRING},
         {2,sizeof(struct stat),W},
     }
    },
    {{PACKNUM(288,-1,366,AARCH64_ni),0},"paccept", OK, RLONG, 4,
     {
         {1,-2,WI|CT,SYSARG_TYPE_SOCKADDR},
         {2,sizeof(int),W, DRSYS_TYPE_SIGNED_INT},
     }
    }, /* == accept4 */

#ifdef AARCHXX
    /**************************************************/
    /* ARM-only */
    {{PACKNUM(-1,-1,271,AARCH64_ni),0},"pciconfig_iobase", OK, RLONG, 3,
     {
         {0, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_SIGNED_INT},
         {1, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(-1,-1,272,AARCH64_ni),0},"pciconfig_read", OK, RLONG, 5,
     {
         {0, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, W},
     }
    },
    {{PACKNUM(-1,-1,273,AARCH64_ni),0},"pciconfig_write", OK, RLONG, 5,
     {
         {0, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {1, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {3, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {4, -3, R},
     }
    },
    {{PACKNUM(-1,-1,289,AARCH64_send),0},"send", OK, RLONG, 4,
     {
         /* Seems to have same 1st 4 args of sendto */
         {1, -2, R},
     }
    },
    {{PACKNUM(-1,-1,291,AARCH64_recv),0},"recv", OK, RLONG, 4,
     {
         /* Seems to have same 1st 4 args of recvfrom */
         {1, -2, W},
     }
    },
    {{PACKNUM(-1,-1,0x0f0001,AARCH64_ni),0},"breakpoint", OK, RLONG, 1,
     {
         /* Return value is this param: */
         {0, sizeof(reg_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(-1,-1,0x0f0002,AARCH64_ni),0},"cacheflush", OK, RLONG, 3,
     {
         {0, sizeof(void *), SYSARG_INLINED, DRSYS_TYPE_POINTER},
         {1, sizeof(long), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
         {2, sizeof(int), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(-1,-1,0x0f0003,AARCH64_ni),0},"usr26", OK, RLONG, 1,
     {
         /* Return value is this param: */
         {0, sizeof(reg_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(-1,-1,0x0f0004,AARCH64_ni),0},"usr32", OK, RLONG, 1,
     {
         /* Return value is this param: */
         {0, sizeof(reg_t), SYSARG_INLINED, DRSYS_TYPE_UNSIGNED_INT},
     }
    },
    {{PACKNUM(-1,-1,0x0f0005,AARCH64_ni),0},"settls", OK, RLONG, 1,
     {
         {0, sizeof(void *), SYSARG_INLINED, DRSYS_TYPE_POINTER},
     }
    },
#endif

    /* XXX i#1019: add newly added linux syscalls */
};

size_t count_syscall_info = sizeof(syscall_info)/sizeof(syscall_info[0]);
