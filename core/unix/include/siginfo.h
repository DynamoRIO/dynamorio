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

#ifndef _SIGINFO_H_
#define _SIGINFO_H_

/* We make sure to pull in the system #defines first so we can
 * undef them here.
 */
#include <signal.h>

#ifdef MACOS
/* For now we just use the system header. */
typedef siginfo_t kernel_siginfo_t;

#else

/* To avoid conflicts with the system header, as we're still including <signal.h>,
 * all types here have kernel_ (or KERNEL_ for enums) prefixed, and all #defines are
 * first #undef-ed.
 */
#    ifdef ANDROID
#        define __WORDSIZE 32
typedef clock_t __clock_t;
#    else
#        include <bits/wordsize.h>
#    endif

/* Type for data associated with a signal.  */
typedef union kernel_sigval {
    int sival_int;
    void *sival_ptr;
} kernel_sigval_t;

#    undef __SI_MAX_SIZE
#    undef __SI_PAD_SIZE

#    define __SI_MAX_SIZE 128
#    if __WORDSIZE == 64
#        define __SI_PAD_SIZE ((__SI_MAX_SIZE / sizeof(int)) - 4)
#    else
#        define __SI_PAD_SIZE ((__SI_MAX_SIZE / sizeof(int)) - 3)
#    endif

#    undef __SI_ALIGNMENT

#    if defined __x86_64__ && __WORDSIZE == 32
/* si_utime and si_stime must be 4 byte aligned for x32 to match the
   kernel.  We align siginfo_t to 8 bytes so that si_utime and si_stime
   are actually aligned to 8 bytes since their offsets are multiple of
   8 bytes.  */
typedef __clock_t __attribute__((__aligned__(4))) __kernel_sigchld_clock_t;
#        define __SI_ALIGNMENT __attribute__((__aligned__(8)))
#    else
typedef __clock_t __kernel_sigchld_clock_t;
#        define __SI_ALIGNMENT
#    endif

#    undef si_pid
#    undef si_uid
#    undef si_tid
#    undef si_timerid
#    undef si_overrun
#    undef si_status
#    undef si_utime
#    undef si_stime
#    undef si_value
#    undef si_int
#    undef si_ptr
#    undef si_addr
#    undef si_addr_lsb
#    undef si_lower
#    undef si_upper
#    undef si_band
#    undef si_fd
#    undef si_call_addr
#    undef si_syscall
#    undef si_arch

/* New fields are appended, and there's padding to cover them, so DR code can
 * blindly write to the latest fields and still work on older kernels.
 */
typedef struct {
    int si_signo; /* Signal number.  */
    int si_errno; /* If non-zero, an errno value associated with
                     this signal, as defined in <errno.h>.  */
    int si_code;  /* Signal code.  */

    union {
        int _pad[__SI_PAD_SIZE];

        /* kill().  */
        struct {
            __pid_t si_pid; /* Sending process ID.  */
            __uid_t si_uid; /* Real user ID of sending process.  */
        } _kill;

        /* POSIX.1b timers.  */
        struct {
            int si_tid;                /* Timer ID.  */
            int si_overrun;            /* Overrun count.  */
            kernel_sigval_t si_sigval; /* Signal value.  */
        } _timer;

        /* POSIX.1b signals.  */
        struct {
            __pid_t si_pid;            /* Sending process ID.  */
            __uid_t si_uid;            /* Real user ID of sending process.  */
            kernel_sigval_t si_sigval; /* Signal value.  */
        } _rt;

        /* SIGCHLD.  */
        struct {
            __pid_t si_pid; /* Which child.  */
            __uid_t si_uid; /* Real user ID of sending process.  */
            int si_status;  /* Exit value or signal.  */
            __kernel_sigchld_clock_t si_utime;
            __kernel_sigchld_clock_t si_stime;
        } _sigchld;

        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS.  */
        struct {
            void *si_addr;         /* Faulting insn/memory ref.  */
            short int si_addr_lsb; /* Valid LSB of the reported address.  */
            struct {
                void *_lower;
                void *_upper;
            } si_addr_bnd;
        } _sigfault;

        /* SIGPOLL.  */
        struct {
            long int si_band; /* Band event for SIGPOLL.  */
            int si_fd;
        } _sigpoll;

        /* SIGSYS.  */
        struct {
            void *_call_addr;   /* Calling user insn.  */
            int _syscall;       /* Triggering system call number.  */
            unsigned int _arch; /* AUDIT_ARCH_* of syscall.  */
        } _sigsys;
    } _sifields;
} kernel_siginfo_t __SI_ALIGNMENT;

/* X/Open requires some more fields with fixed names.  */
#    define si_pid _sifields._kill.si_pid
#    define si_uid _sifields._kill.si_uid
#    define si_timerid _sifields._timer.si_tid
#    define si_overrun _sifields._timer.si_overrun
#    define si_status _sifields._sigchld.si_status
#    define si_utime _sifields._sigchld.si_utime
#    define si_stime _sifields._sigchld.si_stime
#    define si_value _sifields._rt.si_sigval
#    define si_int _sifields._rt.si_sigval.sival_int
#    define si_ptr _sifields._rt.si_sigval.sival_ptr
#    define si_addr _sifields._sigfault.si_addr
#    define si_addr_lsb _sifields._sigfault.si_addr_lsb
#    define si_lower _sifields._sigfault.si_addr_bnd._lower
#    define si_upper _sifields._sigfault.si_addr_bnd._upper
#    define si_band _sifields._sigpoll.si_band
#    define si_fd _sifields._sigpoll.si_fd
#    define si_call_addr _sifields._sigsys._call_addr
#    define si_syscall _sifields._sigsys._syscall
#    define si_arch _sifields._sigsys._arch

/* Values for `si_code'.  Positive values are reserved for kernel-generated
   signals.  */
enum {
    KERNEL_SI_ASYNCNL = -60, /* Sent by asynch name lookup completion.  */
#    undef SI_ASYNCNL
#    define SI_ASYNCNL KERNEL_SI_ASYNCNL
    KERNEL_SI_TKILL = -6,    /* Sent by tkill.  */
#    undef SI_TKILL
#    define SI_TKILL KERNEL_SI_TKILL
    KERNEL_SI_SIGIO,         /* Sent by queued SIGIO. */
#    undef SI_SIGIO
#    define SI_SIGIO KERNEL_SI_SIGIO
    KERNEL_SI_ASYNCIO,       /* Sent by AIO completion.  */
#    undef SI_ASYNCIO
#    define SI_ASYNCIO KERNEL_SI_ASYNCIO
    KERNEL_SI_MESGQ,         /* Sent by real time mesq state change.  */
#    undef SI_MESGQ
#    define SI_MESGQ KERNEL_SI_MESGQ
    KERNEL_SI_TIMER,         /* Sent by timer expiration.  */
#    undef SI_TIMER
#    define SI_TIMER KERNEL_SI_TIMER
    KERNEL_SI_QUEUE,         /* Sent by sigqueue.  */
#    undef SI_QUEUE
#    define SI_QUEUE KERNEL_SI_QUEUE
    KERNEL_SI_USER,          /* Sent by kill, sigsend.  */
#    undef SI_USER
#    define SI_USER KERNEL_SI_USER
    KERNEL_SI_KERNEL = 0x80  /* Send by kernel.  */
#    undef SI_KERNEL
#    define SI_KERNEL KERNEL_SI_KERNEL
};

/* `si_code' values for SIGSEGV signal.  */
enum {
    KERNEL_SEGV_MAPERR = 1, /* Address not mapped to object.  */
#    undef SEGV_MAPERR
#    define SEGV_MAPERR KERNEL_SEGV_MAPERR
    KERNEL_SEGV_ACCERR      /* Invalid permissions for mapped object.  */
#    undef SEGV_ACCERR
#    define SEGV_ACCERR KERNEL_SEGV_ACCERR
};

#endif /* !MACOS */

#endif /* _SIGINFO_H_ */
