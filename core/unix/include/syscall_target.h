#ifndef _SYSCALL_TARGET_H_
#define _SYSCALL_TARGET_H_ 1

/* Do not include this on MacOS as you'll get the wrong numbers! */
#ifndef LINUX
#    error Only use this file on Linux
#endif

#ifdef X86
#    include "../../core/unix/include/syscall_linux_x86.h"
#elif defined(ARM)
#    include "../../core/unix/include/syscall_linux_arm.h"
#elif defined(AARCH64)
#    include "../../core/unix/include/syscall_linux_uapi.h"
#elif defined(RISCV64)
#    include "../../core/unix/include/syscall_linux_riscv64.h"
#else
#    error Unknown platform.
#endif

#endif /* _SYSCALL_TARGET_H_ */
