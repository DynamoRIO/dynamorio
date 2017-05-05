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

#ifndef _SIGCONTEXT_H_
#define _SIGCONTEXT_H_

/* Avoid <bits/sigcontext.h> from <signal.h> */
#define _BITS_SIGCONTEXT_H  1
/* Avoid <asm/sigcontext.h> from <signal.h> on 32-bit Ubuntu (i#648)
 * and anywhere else it manages to sneak in, just in case
 */
#define _ASM_X86_SIGCONTEXT_H 1
#define _ASMi386_SIGCONTEXT_H 1
#define _ASM_X86_64_SIGCONTEXT_H 1
#define _ASM_X86_SIGCONTEXT32_H 1
/* Avoid <asm/sigcontext.h> from <signal.h> on 32-bit ARM. */
#define _ASMARM_SIGCONTEXT_H

#include <linux/types.h>

#define FP_XSTATE_MAGIC1        0x46505853U
#define FP_XSTATE_MAGIC2        0x46505845U
#define FP_XSTATE_MAGIC2_SIZE   sizeof(FP_XSTATE_MAGIC2)

/*
 * bytes 464..511 in the current 512byte layout of fxsave/fxrstor frame
 * are reserved for SW usage. On cpu's supporting xsave/xrstor, these bytes
 * are used to extended the fpstate pointer in the sigcontext, which now
 * includes the extended state information along with fpstate information.
 *
 * Presence of FP_XSTATE_MAGIC1 at the beginning of this SW reserved
 * area and FP_XSTATE_MAGIC2 at the end of memory layout
 * (extended_size - FP_XSTATE_MAGIC2_SIZE) indicates the presence of the
 * extended state information in the memory layout pointed by the fpstate
 * pointer in sigcontext.
 */
struct _fpx_sw_bytes {
        __u32 magic1;           /* FP_XSTATE_MAGIC1 */
        __u32 extended_size;    /* total size of the layout referred by
                                 * fpstate pointer in the sigcontext.
                                 */
        __u64 xstate_bv;
                                /* feature bit mask (including fp/sse/extended
                                 * state) that is present in the memory
                                 * layout.
                                 */
        __u32 xstate_size;      /* actual xsave state size, based on the
                                 * features saved in the layout.
                                 * 'extended_size' will be greater than
                                 * 'xstate_size'.
                                 */
        __u32 padding[7];       /*  for future use. */
};

#ifdef __i386__
/*
 * As documented in the iBCS2 standard..
 *
 * The first part of "struct _fpstate" is just the normal i387
 * hardware setup, the extra "status" word is used to save the
 * coprocessor status word before entering the handler.
 *
 * Pentium III FXSR, SSE support
 *      Gareth Hughes <gareth@valinux.com>, May 2000
 *
 * The FPU state data structure has had to grow to accommodate the
 * extended FPU state required by the Streaming SIMD Extensions.
 * There is no documented standard to accomplish this at the moment.
 */
struct _fpreg {
        unsigned short significand[4];
        unsigned short exponent;
};

struct _fpxreg {
        unsigned short significand[4];
        unsigned short exponent;
        unsigned short padding[3];
};

struct _xmmreg {
        unsigned long element[4];
};

struct _fpstate {
        /* Regular FPU environment */
        unsigned long   cw;
        unsigned long   sw;
        unsigned long   tag;
        unsigned long   ipoff;
        unsigned long   cssel;
        unsigned long   dataoff;
        unsigned long   datasel;
        struct _fpreg   _st[8];
        unsigned short  status;
        unsigned short  magic;          /* 0xffff = regular FPU data only */

        /* FXSR FPU environment */
        unsigned long   _fxsr_env[6];   /* FXSR FPU env is ignored */
        unsigned long   mxcsr;
        unsigned long   reserved;
        struct _fpxreg  _fxsr_st[8];    /* FXSR FPU reg data is ignored */
        struct _xmmreg  _xmm[8];
        unsigned long   padding1[44];

        union {
                unsigned long   padding2[12];
                struct _fpx_sw_bytes sw_reserved; /* represents the extended
                                                   * state info */
        };
};

#define X86_FXSR_MAGIC          0x0000

/*
 * User-space might still rely on the old definition:
 */
struct sigcontext {
        unsigned short gs, __gsh;
        unsigned short fs, __fsh;
        unsigned short es, __esh;
        unsigned short ds, __dsh;
        unsigned long edi;
        unsigned long esi;
        unsigned long ebp;
        unsigned long esp;
        unsigned long ebx;
        unsigned long edx;
        unsigned long ecx;
        unsigned long eax;
        unsigned long trapno;
        unsigned long err;
        unsigned long eip;
        unsigned short cs, __csh;
        unsigned long eflags;
        unsigned long esp_at_signal;
        unsigned short ss, __ssh;
        struct _fpstate *fpstate;
        unsigned long oldmask;
        unsigned long cr2;
};

#elif defined(__amd64__)

/* FXSAVE frame */
/* Note: reserved1/2 may someday contain valuable data. Always save/restore
   them when you change signal frames. */
struct _fpstate {
        __u16   cwd;
        __u16   swd;
        __u16   twd;            /* Note this is not the same as the
                                   32bit/x87/FSAVE twd */
        __u16   fop;
        __u64   rip;
        __u64   rdp;
        __u32   mxcsr;
        __u32   mxcsr_mask;
        __u32   st_space[32];   /* 8*16 bytes for each FP-reg */
        __u32   xmm_space[64];  /* 16*16 bytes for each XMM-reg  */
        __u32   reserved2[12];
        union {
                __u32   reserved3[12];
                struct _fpx_sw_bytes sw_reserved; /* represents the extended
                                                   * state information */
        };
};

/*
 * User-space might still rely on the old definition:
 */
struct sigcontext {
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
    unsigned long rdi;
    unsigned long rsi;
    unsigned long rbp;
    unsigned long rbx;
    unsigned long rdx;
    unsigned long rax;
    unsigned long rcx;
    unsigned long rsp;
    unsigned long rip;
    unsigned long eflags;    /* RFLAGS */
    unsigned short cs;
    unsigned short gs;
    unsigned short fs;
    unsigned short __pad0;
    unsigned long err;
    unsigned long trapno;
    unsigned long oldmask;
    unsigned long cr2;
    struct _fpstate *fpstate;    /* zero when no FPU context */
    unsigned long reserved1[8];
};

#endif /* !__i386__ */

#if defined(__i386__) || defined(__amd64__)
struct _xsave_hdr {
    __u64 xstate_bv;
    __u64 reserved1[2];
    __u64 reserved2[5];
};

struct _ymmh_state {
    /* 16 * 16 bytes for each YMMH-reg */
    __u32 ymmh_space[64];
};

/*
 * Extended state pointed by the fpstate pointer in the sigcontext.
 * In addition to the fpstate, information encoded in the xstate_hdr
 * indicates the presence of other extended state information
 * supported by the processor and OS.
 */
struct _xstate {
    struct _fpstate fpstate;
    struct _xsave_hdr xstate_hdr;
    struct _ymmh_state ymmh;
    /* new processor state extensions go here */
};
#endif /* __i386__ || __amd64__ */

#ifdef __arm__
/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.  Note: only add new entries
 * to the end of the structure.
 */
struct sigcontext {
    unsigned long trap_no;
    unsigned long error_code;
    unsigned long oldmask;
    unsigned long arm_r0;
    unsigned long arm_r1;
    unsigned long arm_r2;
    unsigned long arm_r3;
    unsigned long arm_r4;
    unsigned long arm_r5;
    unsigned long arm_r6;
    unsigned long arm_r7;
    unsigned long arm_r8;
    unsigned long arm_r9;
    unsigned long arm_r10;
    unsigned long arm_fp;
    unsigned long arm_ip;
    unsigned long arm_sp;
    unsigned long arm_lr;
    unsigned long arm_pc;
    unsigned long arm_cpsr;
    unsigned long fault_address;
};

/* user_vfp is defined in <sys/user.h> on Android, so we use sys_user_vfp instead */
struct sys_user_vfp {
    unsigned long long fpregs[32]; /* 16-31 ignored for VFPv2 and below */
    unsigned long fpscr;
};

struct sys_user_vfp_exc {
    unsigned long fpexc;
    unsigned long fpinst;
    unsigned long fpinst2;
};

#define VFP_MAGIC 0x56465001

struct vfp_sigframe {
    unsigned long magic;
    unsigned long size;
    struct sys_user_vfp ufp;
    struct sys_user_vfp_exc ufp_exc;
} __attribute__((__aligned__(8)));
#endif /* __arm__ */


#endif /* _SIGCONTEXT_H_ */
