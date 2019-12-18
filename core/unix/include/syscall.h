/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was generated from glibc headers to make
 ***   information necessary for userspace to call into the Linux
 ***   kernel available to DynamoRIO.  It contains only constants,
 ***   structures, and macros generated from the original header, and
 ***   thus, contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/

#ifndef _SYSCALL_H_
#define _SYSCALL_H_ 1

/* Do not include this on MacOS as you'll get the wrong numbers! */
#ifndef LINUX
#    error Only use this file on Linux
#endif

#ifdef X86
#    include "syscall_linux_x86.h"
#elif defined(ARM)
#    include "syscall_linux_arm.h"
#elif defined(AARCH64)
#    include "syscall_linux_uapi.h"
#endif

#endif /* _SYSCALL_H_ */
