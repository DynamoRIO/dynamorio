/* UAPI system call numbers derived from Linux's "unistd.h"; see exact
 * sources below. This information is necessary for user space to call
 * into the Linux kernel. These UAPI syscall numbers are shared by several
 * current architectures.
 */

#ifndef SYSCALL_LINUX_RISCV64_H
#define SYSCALL_LINUX_RISCV64_H 1

#ifndef LINUX
#    error Only use this file on Linux
#endif

#ifndef __NR_riscv_flush_icache
#    define __NR_riscv_flush_icache (__NR_arch_specific_syscall + 15)
#endif

/* Derived from /usr/riscv64-linux-gnu/include/asm/unistd.h
 * on Ubuntu GLIBC 2.36.1-6ubuntu on Linux 5.15.0-1008-generic
 */
#include "syscall_linux_uapi.h"

#endif /* SYSCALL_LINUX_RISCV64_H */
