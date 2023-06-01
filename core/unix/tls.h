/* **********************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
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
 * tls.h - kernel tls support shared among os-specific files, but not
 * exported to the rest of the code.
 *
 * XXX: originally I was going to have this just be kernel tls support
 * and leave os_local_state_t inside os.c, but it was a pain to refactor
 * os_local_state_t access out of the routines here.  We should either
 * go ahead and do that, or pull all the os_local_state_t setup into here?
 */

#ifndef _OS_TLS_H_
#define _OS_TLS_H_ 1

#include "os_private.h" /* ASM_XAX */
#if defined(ARM) && defined(LINUX)
#    include "include/syscall.h" /* SYS_set_tls */
#endif

/* We support 3 different methods of creating a segment (see os_tls_init()) */
typedef enum {
    TLS_TYPE_NONE,
    TLS_TYPE_LDT,
    TLS_TYPE_GDT,
#if defined(X86) && defined(X64)
    TLS_TYPE_ARCH_PRCTL,
#endif
    /* Used with stealing a register in code cache, we use a (app/priv) lib TLS
     * slot to store DR's tls base in DR C code.
     */
    TLS_TYPE_SLOT,
} tls_type_t;

extern tls_type_t tls_global_type;

/* XXX: more cleanly separate the code so we don't need this here */
extern bool return_stolen_lib_tls_gdt;

#define GDT_NO_SIZE_LIMIT 0xfffff

/* The ldt struct in Linux asm/ldt.h used to be just "struct modify_ldt_ldt_s"; then that
 * was also typdef-ed as modify_ldt_t; then it was just user_desc.
 * To compile on old and new we inline our own copy of the struct.
 * We also use this as a cross-platform representation.
 */
typedef struct _our_modify_ldt_t {
    unsigned int entry_number;
    unsigned int base_addr;
    unsigned int limit;
    unsigned int seg_32bit : 1;
    unsigned int contents : 2;
    unsigned int read_exec_only : 1;
    unsigned int limit_in_pages : 1;
    unsigned int seg_not_present : 1;
    unsigned int useable : 1;
} our_modify_ldt_t;

/* segment selector format:
 * 15..............3      2          1..0
 *        index      0=GDT,1=LDT  Requested Privilege Level
 */
#define USER_PRIVILEGE 3
#define LDT_NOT_GDT 1
#define GDT_NOT_LDT 0
#define SELECTOR_IS_LDT 0x4
#define LDT_SELECTOR(idx) ((idx) << 3 | ((LDT_NOT_GDT) << 2) | (USER_PRIVILEGE))
#define GDT_SELECTOR(idx) ((idx) << 3 | ((GDT_NOT_LDT) << 2) | (USER_PRIVILEGE))
#define SELECTOR_INDEX(sel) ((sel) >> 3)

#ifdef MACOS64
#    define WRITE_DR_SEG(val) ASSERT_NOT_REACHED()
#    define WRITE_LIB_SEG(val) ASSERT_NOT_REACHED()
#    define TLS_SLOT_VAL_EXITED ((byte *)PTR_UINT_MINUS_1)
#elif defined(X86)
#    define WRITE_DR_SEG(val)                                                     \
        do {                                                                      \
            ASSERT(sizeof(val) == sizeof(reg_t));                                 \
            asm volatile("mov %0,%%" ASM_XAX "; mov %%" ASM_XAX ", %" ASM_SEG ";" \
                         :                                                        \
                         : "m"((val))                                             \
                         : ASM_XAX);                                              \
        } while (0)
#    define WRITE_LIB_SEG(val)                                                        \
        do {                                                                          \
            ASSERT(sizeof(val) == sizeof(reg_t));                                     \
            asm volatile("mov %0,%%" ASM_XAX "; mov %%" ASM_XAX ", %" LIB_ASM_SEG ";" \
                         :                                                            \
                         : "m"((val))                                                 \
                         : ASM_XAX);                                                  \
        } while (0)
#elif defined(AARCHXX) || defined(RISCV64)
#    define WRITE_DR_SEG(val) ASSERT_NOT_REACHED()
#    define WRITE_LIB_SEG(val) ASSERT_NOT_REACHED()
#    define TLS_SLOT_VAL_EXITED ((byte *)PTR_UINT_MINUS_1)
#endif /* X86/ARM */

static inline ptr_uint_t
read_thread_register(reg_id_t reg)
{
#ifdef DR_HOST_NOT_TARGET
    ptr_uint_t sel = 0;
    ASSERT_NOT_REACHED();
#elif defined(MACOS64) && !defined(AARCH64)
    ptr_uint_t sel;
    if (reg == SEG_GS) {
        asm volatile("mov %%gs:%1, %0" : "=r"(sel) : "m"(*(void **)0));
    } else if (reg == SEG_FS) {
        return 0;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
#elif defined(X86)
    uint sel;
    if (reg == SEG_FS) {
        asm volatile("movl %%fs, %0" : "=r"(sel));
    } else if (reg == SEG_GS) {
        asm volatile("movl %%gs, %0" : "=r"(sel));
    } else if (reg == SEG_SS) {
        asm volatile("movl %%ss, %0" : "=r"(sel));
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
    /* Pre-P6 family leaves upper 2 bytes undefined, so we clear them.  We don't
     * clear and then use movw because that takes an extra clock cycle, and gcc
     * can optimize this "and" into "test %?x, %?x" for calls from
     * is_segment_register_initialized().
     */
    sel &= 0xffff;
#elif defined(AARCHXX)
    ptr_uint_t sel;
    if (reg == DR_REG_TPIDRURO) {
        IF_X64_ELSE({ asm volatile("mrs %0, tpidrro_el0"
                                   : "=r"(sel)); },
                    {
                        /* read thread register from CP15 (coprocessor 15)
                         * c13 (software thread ID registers) with opcode 3 (user RO)
                         */
                        asm volatile("mrc  p15, 0, %0, c13, c0, 3" : "=r"(sel));
                    });
    } else if (reg == DR_REG_TPIDRURW) {
        IF_X64_ELSE({ asm volatile("mrs %0, tpidr_el0"
                                   : "=r"(sel)); },
                    {
                        /* read thread register from CP15 (coprocessor 15)
                         * c13 (software thread ID registers) with opcode 2 (user RW)
                         */
                        asm volatile("mrc  p15, 0, %0, c13, c0, 2" : "=r"(sel));
                    });
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
#elif defined(RISCV64)
    ptr_uint_t sel;
    if (reg == DR_REG_TP) {
        asm volatile("mv %0, tp" : "=r"(sel));
    } else if (reg == DR_REG_INVALID) {
        /* FIXME i#3544: SEG_TLS is not used. See os_exports.h */
        return 0;
    } else {
        ASSERT_NOT_REACHED();
        return 0;
    }
#else
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return sel;
}

#if defined(AARCHXX) || defined(RISCV64)
static inline bool
write_thread_register(void *val)
{
#    ifdef DR_HOST_NOT_TARGET
    ASSERT_NOT_REACHED();
    return false;
#    elif defined(AARCH64)
    asm volatile("msr " IF_MACOS_ELSE("tpidrro_el0", "tpidr_el0") ", %0" : : "r"(val));
    return true;
#    elif defined(RISCV64)
    asm volatile("mv tp, %0" : : "r"(val));
    return true;
#    else
    return (dynamorio_syscall(SYS_set_tls, 1, val) == 0);
#    endif
}
#endif

#if defined(LINUX) && defined(X86) && defined(X64) && !defined(ARCH_SET_GS)
#    define ARCH_SET_GS 0x1001
#    define ARCH_SET_FS 0x1002
#    define ARCH_GET_FS 0x1003
#    define ARCH_GET_GS 0x1004
#endif

#ifdef LINUX
#    define GDT_NUM_TLS_SLOTS 3
#elif defined(MACOS) && defined(X86)
/* XXX: rename to APP_SAVED_TLS_SLOTS or sthg?
 *
 * XXX i#1405: it seems that the kernel does not swap our entries, so
 * we are currently creating separate entries per thread -- but we
 * only need to save the ones the app might use which we assume will
 * be <= 3.
 */
#    define GDT_NUM_TLS_SLOTS 3 /* index=1 and index=3 are used */
#endif

#define MAX_NUM_CLIENT_TLS 64

/* i#107: handle segment reg usage conflicts */
typedef struct _os_seg_info_t {
    int tls_type;
    void *priv_lib_tls_base;
    void *priv_alt_tls_base;
    void *dr_tls_base;
#ifdef X86
    our_modify_ldt_t app_thread_areas[GDT_NUM_TLS_SLOTS];
#endif
} os_seg_info_t;

/* layout of our TLS */
typedef struct _os_local_state_t {
    /* put state first to ensure that it is cache-line-aligned */
    /* On Linux, we always use the extended structure. */
    local_state_extended_t state;
    /* Linear address of tls page. */
    struct _os_local_state_t *self;
#ifdef X86
    /* Magic number for is_thread_tls_initialized() (i#2089).
     * XXX: keep the offset of this consistent with TLS_MAGIC_OFFSET_ASM in x86.asm.
     */
#    define TLS_MAGIC_VALID 0x244f4952 /* RIO$ */
    /* This value is used for os_thread_take_over() re-takeover. */
#    define TLS_MAGIC_INVALID 0x2d4f4952 /* RIO- */
    uint magic;
#endif
    /* store what type of TLS this is so we can clean up properly */
    tls_type_t tls_type;
    /* For pre-SYS_set_thread_area kernels (pre-2.5.32, pre-NPTL), each
     * thread needs its own ldt entry */
    int ldt_index;
    /* tid needed to ensure children are set up properly */
    thread_id_t tid;
#ifdef X86
    /* i#107 application's tls value and pointed-at base */
    ushort app_lib_tls_reg; /* for mangling seg update/query */
    ushort app_alt_tls_reg; /* for mangling seg update/query */
#endif
    void *app_lib_tls_base; /* for mangling segmented memory ref */
    void *app_alt_tls_base; /* for mangling segmented memory ref */

/* FIXME i#3990: For MACOS, we use a union to save tls space. Unfortunately, this
 * results in not initialising client tls slots which are allocated using
 * dr_raw_tls_calloc. Figuring where to perform memset to clear os_seg_info is not
 * apparently clear due to interleaved thread and instrum inits.
 */
#ifdef LINUX
    os_seg_info_t os_seg_info;
    void *client_tls[MAX_NUM_CLIENT_TLS];
#else
    union {
        /* i#107: We use space in os_tls to store thread area information
         * thread init. It will not conflict with the client_tls usage,
         * so we put them into a union for saving space.
         */
        os_seg_info_t os_seg_info;
        void *client_tls[MAX_NUM_CLIENT_TLS];
    };
#endif
} os_local_state_t;

os_local_state_t *
get_os_tls(void);

void
tls_thread_init(os_local_state_t *os_tls, byte *segment);

/* Sets a non-zero value for unknown threads on attach (see i#3356). */
bool
tls_thread_preinit();

void
tls_thread_free(tls_type_t tls_type, int index);

#if defined(AARCHXX) || defined(RISCV64)
byte **
get_dr_tls_base_addr(void);
#endif

#ifdef MACOS64
void
tls_process_init(void);

void
tls_process_exit(void);

int
tls_get_dr_offs(void);

byte *
tls_get_dr_addr(void);

byte **
get_app_tls_swap_slot_addr(void);
#endif

#ifdef X86
/* Assumes it's passed either SEG_FS or SEG_GS.
 * Returns POINTER_MAX on failure.
 */
byte *
tls_get_fs_gs_segment_base(uint seg);

/* Assumes it's passed either SEG_FS or SEG_GS.
 * Sets only the base: does not change the segment selector register.
 */
bool
tls_set_fs_gs_segment_base(tls_type_t tls_type, uint seg,
                           /* For x64 and TLS_TYPE_ARCH_PRCTL, base is used:
                            * else, desc is used.
                            */
                           byte *base, our_modify_ldt_t *desc);

void
tls_init_descriptor(our_modify_ldt_t *desc OUT, void *base, size_t size, uint index);

bool
tls_get_descriptor(int index, our_modify_ldt_t *desc OUT);

bool
tls_clear_descriptor(int index);

int
tls_dr_index(void);

int
tls_priv_lib_index(void);

bool
tls_dr_using_msr(void);

bool
running_on_WSL(void);

void
tls_initialize_indices(os_local_state_t *os_tls);

int
tls_min_index(void);

#    if defined(LINUX) && defined(X64)
void
tls_handle_post_arch_prctl(dcontext_t *dcontext, int code, reg_t base);
#    endif

#    if defined(MACOS) && !defined(X64)
void
tls_reinstate_selector(uint selector);
#    endif
#endif /* X86 */

#endif /* _OS_TLS_H_ */
