/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from kernel headers to make
 ***   information necessary for userspace to call into the Linux
 ***   kernel available to DynamoRIO.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _CLONE3_H
#define _CLONE3_H

#include <linux/types.h>

/* Derived from struct clone_args defined in /usr/include/linux/sched.h */
typedef struct {
    __aligned_u64 flags;
    __aligned_u64 pidfd;
    __aligned_u64 child_tid;
    __aligned_u64 parent_tid;
    __aligned_u64 exit_signal;
    __aligned_u64 stack;
    __aligned_u64 stack_size;
    __aligned_u64 tls;
    __aligned_u64 set_tid;
    __aligned_u64 set_tid_size;
    __aligned_u64 cgroup;
} clone3_syscall_args_t;

#define CLONE_ARGS_SIZE_VER0 64

#endif /* _CLONE3_H */
