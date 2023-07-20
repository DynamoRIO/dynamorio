/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

/*
 * signal_private.h - declarations shared among signal handling files, but not
 * exported to the rest of the code
 */

#ifndef _SIGNAL_PRIVATE_H_
#define _SIGNAL_PRIVATE_H_ 1

/* We have an ordering issue so we split out LINUX from globals.h */
#include "configure.h"

/* We want to build on older toolchains so we have our own copy of signal
 * data structures.
 */
#include "include/siginfo.h"
#ifdef LINUX
#    include "include/sigcontext.h"
#    include "include/signalfd.h"
#    include "../globals.h" /* after our sigcontext.h, to preclude bits/sigcontext.h */
#elif defined(MACOS)
#    include "../globals.h" /* this defines _XOPEN_SOURCE for Mac */
#    include <signal.h>     /* after globals.h, for _XOPEN_SOURCE from os_exports.h */
#    ifdef X64
#        include <sys/_types/_ucontext64.h> /* for _STRUCT_UCONTEXT64 */
#    endif
#endif

#include "os_private.h"

/***************************************************************************
 * MISC DEFINITIONS
 */

/* handler with SA_SIGINFO flag set gets three arguments: */
typedef void (*handler_t)(int, kernel_siginfo_t *, void *);

#ifdef MACOS
typedef void (*tramp_t)(handler_t, int, int, kernel_siginfo_t *, void *);
#    define SIGHAND_STYLE_UC_TRAD 1
#    define SIGHAND_STYLE_UC_FLAVOR 30
#endif

/* default actions */
enum {
    DEFAULT_TERMINATE,
    DEFAULT_TERMINATE_CORE,
    DEFAULT_IGNORE,
    DEFAULT_STOP,
    DEFAULT_CONTINUE,
};

#ifdef X86
/* Even though we don't always execute xsave ourselves, kernel will do
 * xrestore on sigreturn so we have to obey alignment for avx.
 */
#    define AVX_ALIGNMENT 64
#    define FPSTATE_ALIGNMENT 16
#    define XSTATE_ALIGNMENT (YMM_ENABLED() ? AVX_ALIGNMENT : FPSTATE_ALIGNMENT)
#else
#    define XSTATE_ALIGNMENT REGPARM_END_ALIGN /* actually 4 is prob enough */
#endif

/***************************************************************************
 * FRAMES
 */

/* kernel's notion of sigaction has fields in different order from that
 * used in glibc (I looked at glibc-2.2.4/sysdeps/unix/sysv/linux/i386/sigaction.c)
 * also, /usr/include/asm/signal.h lists both versions
 * I deliberately give kernel_sigaction_t's fields different names to help
 * avoid confusion.
 * (2.1.20 kernel has mask as 2nd field instead, and is expected to be passed
 * in to the non-rt sigaction() call, which we do not yet support)
 */
struct _kernel_sigaction_t {
    handler_t handler;
#ifdef LINUX
    unsigned long flags;
    void (*restorer)(void);
    kernel_sigset_t mask;
#elif defined(MACOS)
    /* this is struct __sigaction in sys/signal.h */
    tramp_t tramp;
    kernel_sigset_t mask;
    int flags;
#endif
}; /* typedef in os_private.h */

#ifdef MACOS
/* i#2105: amazingly, the kernel uses a different layout for returning the prior action.
 * For simplicity we don't change the signature for the handle_sigaction routines.
 */
struct _prev_sigaction_t {
    handler_t handler;
    kernel_sigset_t mask;
    int flags;
}; /* typedef in os_private.h */
#endif

#ifdef LINUX
#    define SIGACT_PRIMARY_HANDLER(sigact) (sigact)->handler
#elif defined(MACOS)
#    define SIGACT_PRIMARY_HANDLER(sigact) (sigact)->tramp
#endif

#ifdef LINUX
typedef unsigned int old_sigset_t;

struct _old_sigaction_t {
    handler_t handler;
    old_sigset_t mask;
    unsigned long flags;
    void (*restorer)(void);
}; /* typedef in os_private.h */

/* kernel's notion of ucontext is different from glibc's!
 * this is adapted from asm/ucontext.h:
 */
typedef struct {
#    if defined(X86)
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    sigcontext_t uc_mcontext;
    kernel_sigset_t uc_sigmask; /* mask last for extensibility */
#    elif defined(AARCH64)
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    kernel_sigset_t uc_sigmask;
    unsigned char sigset_ex[1024 / 8 - sizeof(kernel_sigset_t)];
    sigcontext_t uc_mcontext; /* last for future expansion */
#    elif defined(ARM)
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    sigcontext_t uc_mcontext;
    kernel_sigset_t uc_sigmask;
    int sigset_ex[32 - (sizeof(kernel_sigset_t) / sizeof(int))];
    /* coprocessor state is here */
    union {
        unsigned long uc_regspace[128] __attribute__((__aligned__(8)));
        kernel_vfp_sigframe_t uc_vfp;
    } coproc;
#    elif defined(RISCV64)
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    kernel_sigset_t uc_sigmask;
    unsigned char sigset_ex[1024 / 8 - sizeof(kernel_sigset_t)];
    sigcontext_t uc_mcontext;
#    else
#        error NYI
#    endif
} kernel_ucontext_t;

/* SIGCXT_FROM_UCXT is in os_public.h */
#    define SIGMASK_FROM_UCXT(ucxt) (&((ucxt)->uc_sigmask))

#elif defined(MACOS)
#    ifdef X64
typedef _STRUCT_UCONTEXT64 /* == __darwin_ucontext64 */ kernel_ucontext_t;
#    else
typedef _STRUCT_UCONTEXT /* == __darwin_ucontext */ kernel_ucontext_t;
#    endif
#    define SIGMASK_FROM_UCXT(ucxt) ((kernel_sigset_t *)&((ucxt)->uc_sigmask))
#endif

#if defined(LINUX) || defined(X64)
#    define SIGINFO_FROM_RT_FRAME(frame) (&(frame)->info)
#elif defined(MACOS)
/* Make sure to access through pinfo rather than info as on Mac the info
 * location in our frame struct doesn't exactly match the kernel due to
 * the mid padding.
 */
#    define SIGINFO_FROM_RT_FRAME(frame) ((frame)->pinfo)
#endif

#ifdef LINUX
/* we assume frames look like this, with rt_sigframe used if SA_SIGINFO is set
 * (these are from /usr/src/linux/arch/i386/kernel/signal.c for kernel 2.4.17)
 */

#    define RETCODE_SIZE 8

typedef struct sigframe {
#    ifdef X86
    char *pretcode;
    int sig;
    sigcontext_t sc;
    /* Since 2.6.28, this fpstate has been unused and the real fpstate
     * is at the end of the struct so it can include xstate
     */
    kernel_fpstate_t fpstate;
    unsigned long extramask[_NSIG_WORDS - 1];
    char retcode[RETCODE_SIZE];
#    elif defined(ARM)
    kernel_ucontext_t uc;
    char retcode[RETCODE_SIZE];
#    endif
    /* FIXME: this is a field I added, so our frame looks different from
     * the kernel's...but where else can I store sig where the app won't
     * clobber it?
     * WARNING: our handler receives only rt frames, and we construct
     * plain frames but never pass them to the kernel (on sigreturn() we
     * just go to new context and interpret from there), so the only
     * transparency problem here is if app tries to build its own plain
     * frame and call sigreturn() unrelated to signal delivery.
     * UPDATE: actually we do invoke SYS_*sigreturn
     */
    int sig_noclobber;
    /* In 2.6.28+, fpstate/xstate goes here */
} sigframe_plain_t;
#else
/* Mac only has one frame type, with a libc stub that calls 1-arg or 3-arg handler */
#endif

/* the rt frame is used for SA_SIGINFO signals */
typedef struct rt_sigframe {
#ifdef LINUX
#    ifdef X86
    char *pretcode;
#        ifdef X64
#            ifdef VMX86_SERVER
    kernel_siginfo_t info;
    kernel_ucontext_t uc;
#            else
    kernel_ucontext_t uc;
    kernel_siginfo_t info;
#            endif
#        else
    int sig;
    kernel_siginfo_t *pinfo;
    void *puc;
    kernel_siginfo_t info;
    kernel_ucontext_t uc;
    /* Prior to 2.6.28, "kernel_fpstate_t fpstate" was here.  Rather than
     * try to reproduce that exact layout and detect the underlying kernel
     * (the safest way would be to send ourselves a signal and examine the
     * frame, rather than relying on uname, to handle backports), we use
     * the new layout even on old kernels.  The app should use the fpstate
     * pointer in the sigcontext anyway.
     */
    char retcode[RETCODE_SIZE];
#        endif
    /* In 2.6.28+, fpstate/xstate goes here */
#    elif defined(AARCHXX)
    kernel_siginfo_t info;
    kernel_ucontext_t uc;
    char retcode[RETCODE_SIZE];
#    elif defined(RISCV64)
    kernel_siginfo_t info;
    kernel_ucontext_t uc;
    char retcode[RETCODE_SIZE];
#    endif

#elif defined(MACOS)
#    ifdef AARCH64
    kernel_siginfo_t info;
    struct __darwin_ucontext64 uc;
    struct __darwin_mcontext64 mc;
#    elif defined(X64)
    /* Kernel places padding to align to 16 (via an inefficient alignment macro!),
     * and then skips the retaddr slot to align to 8.
     */
    /* TODO i#1979/i#1312: This will be __darwin_mcontext_avx512_64 if AVX512 is
     * enabled.  Given that it's inlined here *first*, though, we need to figure
     * out how best to handle this variability.  Multiple sigframe_rt_t struct
     * definitions?  Do we want a discovery signal to find the size at init time
     * like on Linux?  We would get the size by counting from "info".
     * Also, should we change this to sigcontext_t.
     */
    struct __darwin_mcontext_avx64 mc; /* sigcontext, "struct mcontext_avx64" to kernel */
    kernel_siginfo_t info;             /* matches user-mode sys/signal.h struct */
    struct __darwin_ucontext64 uc;     /* "struct user_ucontext64" to kernel */
#    else
    app_pc retaddr;
    app_pc handler;
    int sigstyle; /* UC_TRAD = 1-arg, UC_FLAVOR = 3-arg handler */
    int sig;
    kernel_siginfo_t *pinfo;
    struct __darwin_ucontext *puc; /* "struct user_ucontext32 *" to kernel */
    /* The kernel places padding here to align to 16 and then subtract one slot
     * for retaddr post-call alignment, so don't access these subsequent fields
     * directly if given a frame from the kernel!
     */
    struct __darwin_mcontext_avx32 mc; /* sigcontext, "struct mcontext_avx32" to kernel */
    kernel_siginfo_t info;             /* matches user-mode sys/signal.h struct */
    struct __darwin_ucontext uc;       /* "struct user_ucontext32" to kernel */
#    endif
#endif
} sigframe_rt_t;

/* we have to queue up both rt and non-rt signals because we delay
 * their delivery.
 * PR 304708: we now leave in rt form right up until we copy to the
 * app stack, so that we can deliver to a client at a safe spot
 * in rt form.
 */
typedef struct _sigpending_t {
    sigframe_rt_t rt_frame;
    /* i#182/PR 449996: we provide the faulting access address for SIGSEGV, etc. */
    byte *access_address;
    /* use the sigcontext, not the mcontext (used to restart syscalls for i#1145) */
    bool use_sigcontext;
    /* Was this unblocked at receive time? */
    bool unblocked_at_receipt;
    struct _sigpending_t *next;
#if defined(LINUX) && defined(X86)
    /* fpstate is no longer kept inside the frame, and is not always present.
     * if we delay we need to ensure we have room for it.
     * we statically keep room for full xstate in case we need it.
     */
    kernel_xstate_t __attribute__((aligned(AVX_ALIGNMENT))) xstate;
    /* The xstate struct grows and we have to allow for variable sizing,
     * which we handle here by placing it last.
     */
#endif /* LINUX && X86 */
} sigpending_t;

size_t
signal_frame_extra_size(bool include_alignment);

/***************************************************************************
 * PER-THREAD DATA
 */

/* PR 204556: DR/clients use itimers so we need to emulate the app's usage */
typedef struct _itimer_info_t {
    /* easier to manipulate a single value than the two-field struct timeval */
    uint64 interval;
    uint64 value;
} itimer_info_t;

typedef struct _thread_itimer_info_t {
    /* We use per-itimer-signal-type locks to avoid races with alarms arriving
     * in separate threads simultaneously (we don't want to block on itimer
     * locks to handle app-syscall-interruption cases).  Xref i#2993.
     * We only need owner info -- xref i#219: we should add a known-owner
     * lock for cases where a full-fledged recursive lock is not needed.
     */
    recursive_lock_t lock;
    itimer_info_t app;
    itimer_info_t app_saved;
    itimer_info_t dr;
    itimer_info_t actual;
    void (*cb)(dcontext_t *, priv_mcontext_t *);
    /* version for clients */
    void (*cb_api)(dcontext_t *, dr_mcontext_t *);
} thread_itimer_info_t;

/* We use all 3: ITIMER_REAL for clients (i#283/PR 368737), ITIMER_VIRTUAL
 * for -prof_pcs, and ITIMER_PROF for PAPI
 */
#define NUM_ITIMERS 3

/* Don't try to translate every alarm if they're piling up (PR 213040) */
#define SKIP_ALARM_XL8_MAX 3

struct _sigfd_pipe_t;
typedef struct _sigfd_pipe_t sigfd_pipe_t;

/* Data that is shared for all threads in a CLONE_SIGHAND group.
 * Typically this is the whole thread group or "process".
 * The thread_sig_info_t for each thread in the group points to a
 * single copy of this structure.
 */
typedef struct _sighand_info_t {
    bool is_shared;
    int refcount;
    mutex_t lock;
    /* We use kernel_sigaction_t so we don't have to translate back and forth
     * between it and the libc version.
     */
    kernel_sigaction_t *action[SIGARRAY_SIZE];
    bool we_intercept[SIGARRAY_SIZE];
    /* For handling masked-for-app-but-not-for-DR signals.  Any time we receive
     * a signal in a thread for which it is blocked, we need to know whether it was
     * a "process"-wide signal and whether some other thread has it unblocked.
     * To avoid heavyweight locks every time, we keep an atomic-access counter of
     * unmasked threads for each signal, for the CLONE_SIGHAND group (typically
     * whole process).
     */
    int threads_unmasked[SIGARRAY_SIZE];
} sighand_info_t;

typedef struct _thread_sig_info_t {
    /* A pointer to handler info shared in a CLONG_SIGHAND group. */
    sighand_info_t *sighand;

    /* We save the old sigaction across a sigaction syscall so we can return it
     * in post-syscall handling.
     */
    kernel_sigaction_t prior_app_sigaction;
    bool use_kernel_prior_sigaction;
    /* We pass this to the kernel in lieu of the app's data struct, so we
     * can modify it.
     */
    kernel_sigaction_t our_sigaction;
    /* This is the app's sigaction pointer, for restoring post-syscall. */
    const kernel_sigaction_t *sigaction_param;

    /* True after signal_thread_inherit or signal_fork_init are called.  We
     * squash alarm or profiling signals up until this point.
     */
    bool fully_initialized;

    /* DR and clients use itimers, so we need to emulate the app's itimer
     * usage.  This info is shared across CLONE_THREAD threads only for
     * NPTL in kernel 2.6.12+ so these fields are separately shareable from
     * the CLONE_SIGHAND set of fields above.
     */
    bool shared_itimer;
    /* Because a non-CLONE_THREAD thread can be created we can't just use
     * dynamo_exited and need a refcount here.  This is updated via
     * atomic inc/dec without holding a lock (i#1993).
     */
    int *shared_itimer_refcount;
    /* Indicates the # of threads under DR control.  This is updated via atomic
     * inc/dec without holding a lock (i#1993).
     */
    int *shared_itimer_underDR;
    thread_itimer_info_t (*itimer)[NUM_ITIMERS];

    /* cache restorer validity.  not shared: inheriter will re-populate. */
    int restorer_valid[SIGARRAY_SIZE];

    /* rest of app state */
    stack_t app_sigstack;
    sigpending_t *sigpending[SIGARRAY_SIZE];
    /* count of pending signals */
    int num_pending;
    /* are the pending still on one special heap unit? */
    bool multiple_pending_units;
    /* "lock" to prevent interrupting signal from messing up sigpending array */
    bool accessing_sigpending;
    bool nested_pending_ok;

    /* This thread's application signal mask: the set of blocked signals.
     * We need to keep this in sync with the thread-group-shared
     * sighand->threads_unmasked.
     *
     * reroute_to_unmasked_thread() needs read access to app_sigblocked from other
     * threads.  However, we also need lockless read access from our signal handler.
     * Since all writes are from the owning thread, we read w/o a lock from the owning
     * thread, but use the lock for writes from the owning thread and reads from
     * other threads.  (The bitwise operations make it difficult to use atomic
     * updates instead of a mutex.)
     */
    kernel_sigset_t app_sigblocked;
    mutex_t sigblocked_lock;
    /* This is a not-guaranteed-accurate indicator of whether we're inside an
     * app signal handler.  We can't know for sure when a handler ends if the
     * app exits with a longjmp instead of siglongjmp.
     */
    bool in_app_handler;

    /* for returning the old mask (xref PR 523394) */
    kernel_sigset_t pre_syscall_app_sigblocked;
    /* for preserving the app memory (xref i#1187), and for preserving app
     * mask supporting ppoll, epoll_pwait and pselect
     */
    kernel_sigset_t pre_syscall_app_sigprocmask;
    /* True if pre_syscall_app_sigprocmask holds a pre-syscall sigmask */
    bool pre_syscall_app_sigprocmask_valid;
    /* for alarm signals arriving in coarse units we only attempt to xl8
     * every nth signal since coarse translation is expensive (PR 213040)
     */
    uint skip_alarm_xl8;
    /* signalfd array (lazily initialized) */
    sigfd_pipe_t *signalfd[SIGARRAY_SIZE];

    /* to handle sigsuspend we have to save blocked set */
    bool in_sigsuspend;
    kernel_sigset_t app_sigblocked_save;

    /* to inherit in children must not modify until they're scheduled */
    volatile int num_unstarted_children;
    mutex_t child_lock;

    /* our own structures */
    stack_t sigstack;
    void *sigheap;           /* special heap */
    fragment_t *interrupted; /* frag we unlinked for delaying signal */
    cache_pc interrupted_pc; /* pc within frag we unlinked for delaying signal */

#if defined(X86) && defined(LINUX)
    /* As the xstate buffer varies dynamically and gets large (with avx512
     * it is over 2K) we use a copy on the heap.  There are paths where we
     * can't easily free it locally so we keep a pointer in the TLS.
     */
    byte *xstate_buf;   /* xstate_alloc aligned */
    byte *xstate_alloc; /* unaligned */
#endif

#ifdef RETURN_AFTER_CALL
    app_pc signal_restorer_retaddr; /* last signal restorer, known ret exception */
#endif
} thread_sig_info_t;

/***************************************************************************
 * GENERAL ROUTINES (in signal.c)
 */

sigcontext_t *
get_sigcontext_from_rt_frame(sigframe_rt_t *frame);

/**** kernel_sigset_t ***************************************************/

/* defines and typedefs are exported in os_exports.h for siglongjmp */

/* For MacOS, the type is really __darwin_sigset_t, which is a plain __uint32_t.
 * We stick with the struct-containing-uint to simplify the helpers here.
 */

/* most of these are from /usr/src/linux/include/linux/signal.h */
static inline void
kernel_sigemptyset(kernel_sigset_t *set)
{
    memset(set, 0, sizeof(kernel_sigset_t));
}

static inline void
kernel_sigfillset(kernel_sigset_t *set)
{
    memset(set, -1, sizeof(kernel_sigset_t));
}

static inline void
kernel_sigaddset(kernel_sigset_t *set, int _sig)
{
    uint sig = _sig - 1;
    if (_NSIG_WORDS == 1)
        set->sig[0] |= 1UL << sig;
    else
        set->sig[sig / _NSIG_BPW] |= 1UL << (sig % _NSIG_BPW);
}

static inline void
kernel_sigdelset(kernel_sigset_t *set, int _sig)
{
    uint sig = _sig - 1;
    if (_NSIG_WORDS == 1)
        set->sig[0] &= ~(1UL << sig);
    else
        set->sig[sig / _NSIG_BPW] &= ~(1UL << (sig % _NSIG_BPW));
}

static inline bool
kernel_sigismember(kernel_sigset_t *set, int _sig)
{
    int sig = _sig - 1; /* go to 0-based */
    if (_NSIG_WORDS == 1)
        return CAST_TO_bool(1 & (set->sig[0] >> sig));
    else
        return CAST_TO_bool(1 & (set->sig[sig / _NSIG_BPW] >> (sig % _NSIG_BPW)));
}

/* XXX: how does libc do this? */
static inline void
copy_kernel_sigset_to_sigset(kernel_sigset_t *kset, sigset_t *uset)
{
    int sig;
#ifdef DEBUG
    int rc =
#endif
        sigemptyset(uset);
    ASSERT(rc == 0);
    /* do this the slow way...I don't want to make assumptions about
     * structure of user sigset_t
     */
    for (sig = 1; sig <= MAX_SIGNUM; sig++) {
        if (kernel_sigismember(kset, sig))
            sigaddset(uset, sig); /* inlined, so no libc dep */
    }
}

/* i#1541: unfortunately sigismember now leads to libc imports so we write our own */
static inline bool
libc_sigismember(const sigset_t *set, int _sig)
{
    int sig = _sig - 1; /* go to 0-based */
#if defined(MACOS) || defined(ANDROID)
    /* sigset_t is just a uint32 */
    return TEST(1UL << sig, *set);
#else
    /* "set->__val" would be cleaner, but is glibc specific (e.g. musl libc
     * uses __bits as the field name on sigset_t).
     */
    uint bits_per = 8 * sizeof(ulong);
    return TEST(1UL << (sig % bits_per), ((const ulong *)set)[sig / bits_per]);
#endif
}

/* XXX: how does libc do this? */
static inline void
copy_sigset_to_kernel_sigset(const sigset_t *uset, kernel_sigset_t *kset)
{
    int sig;
    kernel_sigemptyset(kset);
    for (sig = 1; sig <= MAX_SIGNUM; sig++) {
        if (libc_sigismember(uset, sig))
            kernel_sigaddset(kset, sig);
    }
}

int
sigaction_syscall(int sig, kernel_sigaction_t *act, kernel_sigaction_t *oact);

void
set_handler_sigact(kernel_sigaction_t *act, int sig, handler_t handler);

/***************************************************************************
 * OS-SPECIFIC ROUTINES (in signal_<os>.c)
 */

void
signal_arch_init(void);

void
sigcontext_to_mcontext_simd(priv_mcontext_t *mc, sig_full_cxt_t *sc_full);

void
mcontext_to_sigcontext_simd(sig_full_cxt_t *sc_full, priv_mcontext_t *mc);

void
save_fpstate(dcontext_t *dcontext, sigframe_rt_t *frame);

#ifdef DEBUG
void
dump_sigcontext(dcontext_t *dcontext, sigcontext_t *sc);
#endif

#ifdef LINUX
void
signalfd_init(void);

void
signalfd_exit(void);

void
signalfd_thread_exit(dcontext_t *dcontext, thread_sig_info_t *info);

bool
notify_signalfd(dcontext_t *dcontext, thread_sig_info_t *info, int sig,
                sigframe_rt_t *frame);

void
check_signals_pending(dcontext_t *dcontext, thread_sig_info_t *info);
#endif

#endif /* _SIGNAL_PRIVATE_H_ */
