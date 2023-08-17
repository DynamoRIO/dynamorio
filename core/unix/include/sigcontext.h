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

#include <linux/types.h>

#define FP_XSTATE_MAGIC1 0x46505853U
#define FP_XSTATE_MAGIC2 0x46505845U
#ifndef FP_XSTATE_MAGIC2_SIZE
#    define FP_XSTATE_MAGIC2_SIZE sizeof(FP_XSTATE_MAGIC2)
#endif

/*
 * bytes 464..511 in the current 512byte layout of fxsave/fxrstor frame
 * are reserved for SW usage. On cpu's supporting xsave/xrstor, these bytes
 * are used to extended the fpstate pointer in the sigcontext, which now
 * includes the extended state information along with fpstate information.
 *
 * If sw_reserved.magic1 == FP_XSTATE_MAGIC1 then there's a
 * sw_reserved.extended_size bytes large extended context area present. (The
 * last 32-bit word of this extended area (at the
 * fpstate+extended_size-FP_XSTATE_MAGIC2_SIZE address) is set to
 * FP_XSTATE_MAGIC2 so that you can sanity check your size calculations.)
 *
 * This extended area typically grows with newer CPUs that have larger and
 * larger XSAVE areas.
 */
typedef struct _kernel_fpx_sw_bytes_t {
    __u32 magic1; /* FP_XSTATE_MAGIC1 */
    /* Total size of the fpstate area:
     *
     * - if magic1 == 0 then it's sizeof(struct _fpstate)
     * - if magic1 == FP_XSTATE_MAGIC1 then it's sizeof(struct
     *   _xstate) plus extensions (if any).
     *
     * The extensions always include FP_XSTATE_MAGIC2_SIZE.
     * For 32-bit, they also include the FSAVE fields, but those are actually
     * prepended: for DR they're the initial part of the 32-bit kernel_fpstate_t
     * and thus part of kernel_xstate_t already.
     */
    __u32 extended_size;
    __u64 xstate_bv;
    /* feature bit mask (including fp/sse/extended
     * state) that is present in the memory
     * layout.
     */
    /* Actual xsave state size, based on the features saved in the layout.
     * 'extended_size' will be greater than 'xstate_size' (because it includes
     * FP_XSTATE_MAGIC2, plus FSAVE data for 32-bit).
     */
    __u32 xstate_size;
    __u32 padding[7]; /*  for future use. */
} kernel_fpx_sw_bytes_t;

#if defined(X86) && !defined(X64)
/*
 * As documented in the iBCS2 standard..
 *
 * The first part of "kernel_fpstate_t" is just the normal i387
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
typedef struct _kernel_fpreg_t {
    unsigned short significand[4];
    unsigned short exponent;
} kernel_fpreg_t;

typedef struct _kernel_fpxreg_t {
    unsigned short significand[4];
    unsigned short exponent;
    unsigned short padding[3];
} kernel_fpxreg_t;

typedef struct _kernel_xmmreg_t {
    unsigned long element[4];
} kernel_xmmreg_t;

typedef struct _kernel_fpstate_t {
    /* Regular FPU environment.
     * These initial fields are the FSAVE format.  They are followed by the
     * FXSAVE, which matches the 64-bit kernel_fpstate_t.  Note that in the kernel
     * code, it treats this FSAVE prefix as a separate thing and includes it in
     * extra size in kernel_fpx_sw_bytes_t.extended_size, although it is in essence
     * prepended instead of appended.
     */
    unsigned long cw;
    unsigned long sw;
    unsigned long tag;
    unsigned long ipoff;
    unsigned long cssel;
    unsigned long dataoff;
    unsigned long datasel;
    kernel_fpreg_t _st[8];
    unsigned short status;
    unsigned short magic; /* 0xffff = regular FPU data only */

    /* FXSR FPU environment.
     * Note that this is the start of the xsave region. The kernel requires this
     * to be 64-byte aligned. We ensure this alignment in convert_frame_to_nonrt
     * and fixup_rtframe_pointers.
     */
    unsigned long _fxsr_env[6]; /* FXSR FPU env is ignored */
    unsigned long mxcsr;
    unsigned long reserved;
    kernel_fpxreg_t _fxsr_st[8]; /* FXSR FPU reg data is ignored */
    kernel_xmmreg_t _xmm[8];
    unsigned long padding1[44];

    union {
        unsigned long padding2[12];
        kernel_fpx_sw_bytes_t sw_reserved; /* represents the extended
                                            * state info */
    };
} kernel_fpstate_t;

#    define X86_FXSR_MAGIC 0x0000
/* This is the size of the FSAVE fields the kernel prepends to fpstate.
 * We have them in our kernel_fpstate_t struct.
 */
#    define FSAVE_FPSTATE_PREFIX_SIZE offsetof(kernel_fpstate_t, _fxsr_env)

/*
 * User-space might still rely on the old definition:
 */
typedef struct _kernel_sigcontext_t {
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
    kernel_fpstate_t *fpstate;
    unsigned long oldmask;
    unsigned long cr2;
} kernel_sigcontext_t;

#elif defined(X86) && defined(X64)

/* FXSAVE frame */
/* Note: reserved1/2 may someday contain valuable data. Always save/restore
   them when you change signal frames. */
typedef struct _kernel_fpstate_t {
    __u16 cwd;
    __u16 swd;
    __u16 twd; /* Note this is not the same as the
                  32bit/x87/FSAVE twd */
    __u16 fop;
    __u64 rip;
    __u64 rdp;
    __u32 mxcsr;
    __u32 mxcsr_mask;
    __u32 st_space[32];  /* 8*16 bytes for each FP-reg */
    __u32 xmm_space[64]; /* 16*16 bytes for each XMM-reg  */
    __u32 reserved2[12];
    union {
        __u32 reserved3[12];
        kernel_fpx_sw_bytes_t sw_reserved; /* represents the extended
                                            * state information */
    };
} kernel_fpstate_t;

/*
 * User-space might still rely on the old definition:
 */
typedef struct _kernel_sigcontext_t {
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
    unsigned long eflags; /* RFLAGS */
    unsigned short cs;
    unsigned short gs;
    unsigned short fs;
    unsigned short __pad0;
    unsigned long err;
    unsigned long trapno;
    unsigned long oldmask;
    unsigned long cr2;
    kernel_fpstate_t *fpstate; /* zero when no FPU context */
    unsigned long reserved1[8];
} kernel_sigcontext_t;

#endif

#ifdef X86
typedef struct _kernel_xsave_hdr_t {
    __u64 xstate_bv;
    __u64 reserved1[2];
    __u64 reserved2[5];
} kernel_xsave_hdr_t;

typedef struct _kernel_ymmh_state_t {
    /* 16 * 16 bytes for each YMMH-reg */
    __u32 ymmh_space[64];
} kernel_ymmh_state_t;

/*
 * Extended state pointed by the fpstate pointer in the sigcontext.
 * In addition to the fpstate, information encoded in the xstate_hdr
 * indicates the presence of other extended state information
 * supported by the processor and OS.
 */
typedef struct _kernel_xstate_t {
    kernel_fpstate_t fpstate;
    kernel_xsave_hdr_t xstate_hdr;
    kernel_ymmh_state_t ymmh;
    /* new processor state extensions go here */
} kernel_xstate_t;
#endif

#ifdef ARM
/*
 * Signal context structure - contains all info to do with the state
 * before the signal handler was invoked.  Note: only add new entries
 * to the end of the structure.
 */
typedef struct _kernel_sigcontext_t {
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
} kernel_sigcontext_t;

/* user_vfp is defined in <sys/user.h> on Android, so we use sys_user_vfp instead */
typedef struct _kernel_sys_user_vfp_t {
    unsigned long long fpregs[32]; /* 16-31 ignored for VFPv2 and below */
    unsigned long fpscr;
} kernel_sys_user_vfp_t;

typedef struct _kernel_sys_user_vfp_exc_t {
    unsigned long fpexc;
    unsigned long fpinst;
    unsigned long fpinst2;
} kernel_sys_user_vfp_exc_t;

#    define VFP_MAGIC 0x56465001

typedef struct _kernel_vfp_sigframe_t {
    unsigned long magic;
    unsigned long size;
    kernel_sys_user_vfp_t ufp;
    kernel_sys_user_vfp_exc_t ufp_exc;
} __attribute__((__aligned__(8))) kernel_vfp_sigframe_t;

typedef struct _kernel_iwmmxt_struct_t {
    unsigned int save[38];
} kernel_iwmmxt_struct_t;

#    define IWMMXT_MAGIC 0x12ef842a

typedef struct _kernel_iwmmxt_sigframe_t {
    unsigned long magic;
    unsigned long size;
    kernel_iwmmxt_struct_t storage;
} __attribute__((__aligned__(8))) kernel_iwmmxt_sigframe_t;

/* Dummy padding block: a block with this magic should be skipped. */
#    define DUMMY_MAGIC 0xb0d9ed01

#endif /* ARM */

#ifdef AARCH64

typedef struct _kernel_sigcontext_t {
    unsigned long long fault_address;
    unsigned long long regs[31];
    unsigned long long sp;
    unsigned long long pc;
    unsigned long long pstate;
    /* 4K reserved for FP/SIMD state and future expansion */
    unsigned char __reserved[4096] __attribute__((__aligned__(16)));
} kernel_sigcontext_t;

/* XXX: These defines come from the system include files for a regular
 * build (signal.h is included), but for DR_HOST_NOT_TARGET we need
 * them defined here.  Probably what we should do is rename them so
 * they can always come from here and not rely on the system header.
 */
#    ifndef FPSIMD_MAGIC
/*
 * Header to be used at the beginning of structures extending the user
 * context. Such structures must be placed after the rt_sigframe on the stack
 * and be 16-byte aligned. The last structure must be a dummy one with the
 * magic and size set to 0.
 */
struct _aarch64_ctx {
    __u32 magic;
    __u32 size;
};

#        define FPSIMD_MAGIC 0x46508001

struct fpsimd_context {
    struct _aarch64_ctx head;
    __u32 fpsr;
    __u32 fpcr;
    __uint128_t vregs[32];
};

/* TODO i#5365: Storage of sve_context in kernel_sigcontext_t.__reserved, see
 * above. See also sigcontext_to_mcontext_simd() and
 * mcontext_to_sigcontext_simd().
 */

#        define SVE_MAGIC 0x53564501

struct sve_context {
    struct _aarch64_ctx head;
    __u16 vl;
    __u16 __reserved[3];
};
#    endif

#endif /* AARCH64 */

#ifdef RISCV64

struct user_regs_struct {
    unsigned long pc;
    unsigned long ra;
    unsigned long sp;
    unsigned long gp;
    unsigned long tp;
    unsigned long t0;
    unsigned long t1;
    unsigned long t2;
    unsigned long s0;
    unsigned long s1;
    unsigned long a0;
    unsigned long a1;
    unsigned long a2;
    unsigned long a3;
    unsigned long a4;
    unsigned long a5;
    unsigned long a6;
    unsigned long a7;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
    unsigned long t3;
    unsigned long t4;
    unsigned long t5;
    unsigned long t6;
};

struct __riscv_f_ext_state {
    __u32 f[32];
    __u32 fcsr;
};

struct __riscv_d_ext_state {
    __u64 f[32];
    __u32 fcsr;
};

struct __riscv_q_ext_state {
    __u64 f[64] __attribute__((aligned(16)));
    __u32 fcsr;
    __u32 reserved[3];
};

union __riscv_fp_state {
    struct __riscv_f_ext_state f;
    struct __riscv_d_ext_state d;
    struct __riscv_q_ext_state q;
};

typedef struct _kernel_sigcontext_t {
    struct user_regs_struct sc_regs;
    union __riscv_fp_state sc_fpregs;
} kernel_sigcontext_t;

#endif /* RISCV64 */

#endif /* _SIGCONTEXT_H_ */
