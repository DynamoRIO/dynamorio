/* *******************************************************************************
 * Copyright (c) 2011-2020 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * *******************************************************************************/

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
 * loader_linux.c: Linux-specific private loader code
 */

#include "../globals.h"
#include "../module_shared.h"
#include "os_private.h"
#include "../ir/instr.h" /* SEG_GS/SEG_FS */
#include "module.h"
#include "module_private.h"
#include "../heap.h" /* HEAPACCT */
#ifdef LINUX
#    include "include/syscall.h"
#else
#    include <sys/syscall.h>
#endif
#include "tls.h"
#include <stddef.h>
#if defined(X86) && defined(DEBUG)
#    include "os_asm_defines.asm" /* for TLS_APP_SELF_OFFSET_ASM */
#endif

/****************************************************************************
 * Thread Local Storage
 */

/* The description of Linux Thread Local Storage Implementation on x86 arch
 * Following description is based on the understanding of glibc-2.11.2 code
 */
/* TLS is achieved via memory reference using segment register on x86.
 * Each thread has its own memory segment whose base is pointed by [%seg:0x0],
 * so different thread can access thread private memory via the same memory
 * reference opnd [%seg:offset].
 */
/* In Linux, FS and GS are used for TLS reference.
 * In current Linux libc implementation, %gs/%fs is used for TLS access
 * in 32/64-bit x86 architecture, respectively.
 */
/* TCB (thread control block) is a data structure to describe the thread
 * information. which is actually struct pthread in x86 Linux.
 * In x86 arch, [%seg:0x0] is used as TP (thread pointer) pointing to
 * the TCB. Instead of allocating modules' TLS after TCB,
 * they are put before the TCB, which allows TCB to have any size.
 * Using [%seg:0x0] as the TP, all modules' static TLS are accessed
 * via negative offsets, and TCB fields are accessed via positive offsets.
 */
/* There are two possible TLS memory, static TLS and dynamic TLS.
 * Static TLS is the memory allocated in the TLS segment, and can be accessed
 * via direct [%seg:offset].
 * Dynamic TLS is the memory allocated dynamically when the process
 * dynamically loads a shared library (e.g. via dl_open), which has its own TLS
 * but cannot fit into the TLS segment created at beginning.
 *
 * DTV (dynamic thread vector) is the data structure used to maintain and
 * reference those modules' TLS.
 * Each module has a id, which is the index into the DTV to check
 * whether its tls is static or dynamic, and where it is.
 */

/* The maxium number modules that we support to have TLS here.
 * Because any libraries having __thread variable will have tls segment.
 * we pick 64 and hope it is large enough.
 */
#define MAX_NUM_TLS_MOD 64
typedef struct _tls_info_t {
    uint num_mods;
    int offset;
    int max_align;
    int offs[MAX_NUM_TLS_MOD];
    privmod_t *mods[MAX_NUM_TLS_MOD];
} tls_info_t;
static tls_info_t tls_info;

/* Maximum size of TLS for client private libraries.
 * We will round this up to a multiple of the page size.
 */
static size_t client_tls_size = 2 * 4096;

/* The actual tcb size is the size of struct pthread from nptl/descr.h, which is
 * a glibc internal header that we can't include.  We hardcode a guess for the
 * tcb size, and try to recover if we guessed too large.  This value was
 * recalculated by building glibc and printing sizeof(struct pthread) from
 * _dl_start() in elf/rtld.c.  The value can also be determined from the
 * assembly of _dl_allocate_tls_storage() in ld.so:
 * Dump of assembler code for function _dl_allocate_tls_storage:
 *    0x00007ffff7def0a0 <+0>:  push   %r12
 *    0x00007ffff7def0a2 <+2>:  mov    0x20eeb7(%rip),%rdi # _dl_tls_static_align
 *    0x00007ffff7def0a9 <+9>:  push   %rbp
 *    0x00007ffff7def0aa <+10>: push   %rbx
 *    0x00007ffff7def0ab <+11>: mov    0x20ee9e(%rip),%rbx # _dl_tls_static_size
 *    0x00007ffff7def0b2 <+18>: mov    %rbx,%rsi
 *    0x00007ffff7def0b5 <+21>: callq  0x7ffff7ddda88 <__libc_memalign@plt>
 * => 0x00007ffff7def0ba <+26>: test   %rax,%rax
 *    0x00007ffff7def0bd <+29>: mov    %rax,%rbp
 *    0x00007ffff7def0c0 <+32>: je     0x7ffff7def180 <_dl_allocate_tls_storage+224>
 *    0x00007ffff7def0c6 <+38>: lea    -0x900(%rax,%rbx,1),%rbx
 *    0x00007ffff7def0ce <+46>: mov    $0x900,%edx
 * This is typically an allocation larger than 4096 bytes aligned to 64 bytes.
 * The "lea -0x900(%rax,%rbx,1),%rbx" instruction computes the thread pointer to
 * install.  The allocator used by the loader has no headers, so we don't have a
 * good way to guess how big this allocation was.  Instead we use this estimate.
 */
/* On A32, the pthread is put before tcbhead instead tcbhead being part of pthread */
static size_t tcb_size = IF_X86_ELSE(IF_X64_ELSE(0x900, 0x490), 0x40);

/* thread contol block header type from
 * - sysdeps/x86_64/nptl/tls.h
 * - sysdeps/i386/nptl/tls.h
 * - sysdeps/arm/nptl/tls.h
 * - sysdeps/riscv/nptl/tls.h
 */
typedef struct _tcb_head_t {
#ifdef X86
    void *tcb;
    void *dtv;
    void *self;
    int multithread;
#    ifdef X64
    int gscope_flag;
#    endif
    ptr_uint_t sysinfo;
    /* Later fields are copied verbatim. */

    ptr_uint_t stack_guard;
    ptr_uint_t pointer_guard;
#elif defined(AARCH64)
    /* FIXME i#1569: This may be wrong! */
    void *dtv;
    void *private;
#elif defined(ARM)
    void *dtv;
    void *private;
    byte padding[2]; /* make it 16-byte align */
#elif defined(RISCV64)
    void *dtv;
    void *private;
#endif /* X86/ARM */
} tcb_head_t;

#ifdef X86
#    define TLS_PRE_TCB_SIZE 0
#elif defined(AARCHXX)
/* FIXME i#1569: This may be wrong for AArch64! */
/* Data structure to match libc pthread.
 * GDB reads some slot in TLS, which is pid/tid of pthread, so we must make sure
 * the size and member locations match to avoid gdb crash.
 */
typedef struct _dr_pthread_t {
    byte data1[0x68]; /* # of bytes before tid within pthread */
    process_id_t tid;
    thread_id_t pid;
    byte data2[0x450]; /* # of bytes after pid within pthread */
} dr_pthread_t;
#    define TLS_PRE_TCB_SIZE sizeof(dr_pthread_t)
#    define LIBC_PTHREAD_SIZE 0x4c0
#    define LIBC_PTHREAD_TID_OFFSET 0x68
#elif defined(RISCV64)
typedef struct _dr_pthread_t {
    byte data1[0xd0]; /* # of bytes before tid within pthread */
    process_id_t tid;
    thread_id_t pid;
    byte data2[0x6b8]; /* # of bytes after pid within pthread */
} dr_pthread_t;
#    define TLS_PRE_TCB_SIZE sizeof(dr_pthread_t)
#    define LIBC_PTHREAD_SIZE 0x790
#    define LIBC_PTHREAD_TID_OFFSET 0xd0
#endif /* X86/ARM */

#ifdef X86
/* An estimate of the size of the static TLS data before the thread pointer that
 * we need to copy on behalf of libc.  When loading modules that have variables
 * stored in static TLS space, the loader stores them prior to the thread
 * pointer and lets the app intialize them.  Until we stop using the app's libc
 * (i#46), we need to copy this data from before the thread pointer.
 *
 * XXX i#2117: we have seen larger values than 0x400 here.
 * However, this seems to be used for more than just late injection, and even
 * for late, blindly increasing it causes some test failures, so it needs
 * more work.  The comment above should be updated as well, as we do not use
 * the app's libc inside DR.
 */
#    define APP_LIBC_TLS_SIZE 0x400
#elif defined(AARCHXX)
/* FIXME i#1551, i#1569: investigate the difference between ARM and X86 on TLS.
 * On ARM, it seems that TLS variables are not put before the thread pointer
 * as they are on X86.
 */
#    define APP_LIBC_TLS_SIZE 0
#elif defined(RISCV64)
/* FIXME i#3544: Not implemented */
#    define APP_LIBC_TLS_SIZE 0
#endif

#ifdef LINUX /* XXX i#1285: implement MacOS private loader */
/* XXX: add description here to talk how TLS is setup.
 * This should be done *before* relocating the module.
 * There are TLS-specific relocations that depend on having os_privmod_data_t
 * tls fields set.
 */
void
privload_mod_tls_init(privmod_t *mod)
{
    os_privmod_data_t *opd;
    size_t offset;
    int first_byte;

    IF_X86(ASSERT(TLS_APP_SELF_OFFSET_ASM == offsetof(tcb_head_t, self)));
    ASSERT_OWN_RECURSIVE_LOCK(true, &privload_lock);
    opd = (os_privmod_data_t *)mod->os_privmod_data;
    ASSERT(opd != NULL && opd->tls_block_size != 0);
    if (tls_info.num_mods >= MAX_NUM_TLS_MOD) {
        CLIENT_ASSERT(false, "Max number of modules with tls variables reached");
        FATAL_USAGE_ERROR(TOO_MANY_TLS_MODS, 2, get_application_name(),
                          get_application_pid());
    }
    tls_info.mods[tls_info.num_mods] = mod;
    opd->tls_modid = tls_info.num_mods;
    offset = (opd->tls_modid == 0) ? APP_LIBC_TLS_SIZE : tls_info.offset;
    /* decide the offset of each module in the TLS segment from
     * thread pointer.
     * Because the tls memory is located before thread pointer, we use
     * [tp - offset] to get the tls block for each module later.
     * so the first_byte that obey the alignment is calculated by
     * -opd->tls_first_byte & (opd->tls_align - 1);
     */
    first_byte = -opd->tls_first_byte & (opd->tls_align - 1);
    /* increase offset size by adding current mod's tls size:
     * 1. increase the tls_block_size with the right alignment
     *    using ALIGN_FORWARD()
     * 2. add first_byte to make the first byte with right alighment.
     */
    offset = first_byte +
        ALIGN_FORWARD(offset + opd->tls_block_size + first_byte, opd->tls_align);
    opd->tls_offset = offset;
    tls_info.offs[tls_info.num_mods] = offset;
    tls_info.offset = offset;
    LOG(GLOBAL, LOG_LOADER, 2, "%s for #%d %s: offset %zu\n", __FUNCTION__,
        opd->tls_modid, mod->name, offset);

    tls_info.num_mods++;
    if (opd->tls_align > tls_info.max_align) {
        tls_info.max_align = opd->tls_align;
    }
}

static void
privload_copy_tls_block(app_pc priv_tls_base, uint mod_idx)
{
    os_privmod_data_t *opd = tls_info.mods[mod_idx]->os_privmod_data;
    void *dest;
    /* now copy the tls memory from the image */
    dest = priv_tls_base - tls_info.offs[mod_idx];
    LOG(GLOBAL, LOG_LOADER, 2,
        "%s: copying ELF TLS from " PFX " to " PFX " block %zu image %zu\n", __FUNCTION__,
        opd->tls_image, dest, opd->tls_block_size, opd->tls_image_size);
    DOLOG(3, LOG_LOADER, {
        dump_buffer_as_bytes(GLOBAL, opd->tls_image, opd->tls_image_size,
                             DUMP_RAW | DUMP_ADDRESS);
        LOG(GLOBAL, LOG_LOADER, 2, "\n");
    });
    memcpy(dest, opd->tls_image, opd->tls_image_size);
    /* set all 0 to the rest of memory.
     * tls_block_size refers to the size in memory, and
     * tls_image_size refers to the size in file.
     * We use the same way as libc to name them.
     */
    ASSERT(opd->tls_block_size >= opd->tls_image_size);
    memset(dest + opd->tls_image_size, 0, opd->tls_block_size - opd->tls_image_size);
}

/* Called post-reloc. */
void
privload_mod_tls_primary_thread_init(privmod_t *mod)
{
    ASSERT(!dynamo_initialized);
    /* Copy ELF block for primary thread for use in init funcs (i#2751).
     * We do this after relocs and assume reloc ifuncs don't need this:
     * else we'd have to assume there are no relocs in the TLS blocks.
     */
    os_local_state_t *os_tls = get_os_tls();
    app_pc priv_tls_base = os_tls->os_seg_info.priv_lib_tls_base;
    os_privmod_data_t *opd = (os_privmod_data_t *)mod->os_privmod_data;
    privload_copy_tls_block(priv_tls_base, opd->tls_modid);
}
#endif

void *
privload_tls_init(void *app_tp)
{
    size_t client_tls_alloc_size = ALIGN_FORWARD(client_tls_size, PAGE_SIZE);
    app_pc dr_tp;
    tcb_head_t *dr_tcb;
    size_t tls_bytes_read;

    /* FIXME: These should be a thread logs, but dcontext is not ready yet. */
    LOG(GLOBAL, LOG_LOADER, 2, "%s: app TLS segment base is " PFX "\n", __FUNCTION__,
        app_tp);
    dr_tp = heap_mmap(client_tls_alloc_size, MEMPROT_READ | MEMPROT_WRITE,
                      VMM_SPECIAL_MMAP | VMM_PER_THREAD);
    ASSERT(APP_LIBC_TLS_SIZE + TLS_PRE_TCB_SIZE + tcb_size <= client_tls_alloc_size);
#ifdef AARCHXX
    /* GDB reads some pthread members (e.g., pid, tid), so we must make sure
     * the size and member locations match to avoid gdb crash.
     */
    ASSERT(TLS_PRE_TCB_SIZE == LIBC_PTHREAD_SIZE);
    ASSERT(LIBC_PTHREAD_TID_OFFSET == offsetof(dr_pthread_t, tid));
#endif
    LOG(GLOBAL, LOG_LOADER, 2, "%s: allocated %d at " PFX "\n", __FUNCTION__,
        client_tls_alloc_size, dr_tp);
    dr_tp = dr_tp + client_tls_alloc_size - tcb_size;
    dr_tcb = (tcb_head_t *)dr_tp;
    LOG(GLOBAL, LOG_LOADER, 2, "%s: adjust thread pointer to " PFX "\n", __FUNCTION__,
        dr_tp);
    /* We copy the whole tcb to avoid initializing it by ourselves.
     * and update some fields accordingly.
     */
    if (app_tp != NULL &&
        !safe_read_ex(app_tp - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE,
                      APP_LIBC_TLS_SIZE + TLS_PRE_TCB_SIZE + tcb_size,
                      dr_tp - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE, &tls_bytes_read)) {
        LOG(GLOBAL, LOG_LOADER, 2,
            "%s: read failed, tcb was 0x%lx bytes "
            "instead of 0x%lx\n",
            __FUNCTION__, tls_bytes_read - APP_LIBC_TLS_SIZE, tcb_size);
#ifdef AARCHXX
    } else {
        dr_pthread_t *dp = (dr_pthread_t *)(dr_tp - APP_LIBC_TLS_SIZE - TLS_PRE_TCB_SIZE);
        dp->pid = get_process_id();
        dp->tid = get_sys_thread_id();
#endif
    }
    /* We do not assert or warn on a truncated read as it does happen when TCB
     * + our over-estimate crosses a page boundary (our estimate is for latest
     * libc and is larger than on older libc versions): i#855.
     */
    ASSERT(tls_info.offset <= client_tls_alloc_size - TLS_PRE_TCB_SIZE - tcb_size);
#ifdef X86
    /* Update two self pointers. */
    dr_tcb->tcb = dr_tcb;
    dr_tcb->self = dr_tcb;
    /* i#555: replace app's vsyscall with DR's int0x80 syscall */
    dr_tcb->sysinfo = (ptr_uint_t)client_int_syscall;
#elif defined(AARCHXX)
    dr_tcb->dtv = NULL;
    dr_tcb->private = NULL;
#endif

    /* We initialize the primary thread's ELF TLS in privload_mod_tls_init()
     * after finalizing the module load (dependent libs not loaded yet here).
     * For subsequent threads we walk the module list here.
     */
    if (dynamo_initialized) {
        uint i;
        for (i = 0; i < tls_info.num_mods; i++)
            privload_copy_tls_block(dr_tp, i);
    }

    return dr_tp;
}

void
privload_tls_exit(void *dr_tp)
{
    size_t client_tls_alloc_size = ALIGN_FORWARD(client_tls_size, PAGE_SIZE);
    if (dr_tp == NULL)
        return;
    dr_tp = dr_tp + tcb_size - client_tls_alloc_size;
    heap_munmap(dr_tp, client_tls_alloc_size, VMM_SPECIAL_MMAP | VMM_PER_THREAD);
}

/****************************************************************************
 * Function Redirection
 */

/* We did not create dtv, so we need redirect tls_get_addr */
typedef struct _tls_index_t {
    unsigned long int ti_module;
    unsigned long int ti_offset;
} tls_index_t;

void *
redirect___tls_get_addr(tls_index_t *ti)
{
    LOG(GLOBAL, LOG_LOADER, 4, "__tls_get_addr: module: %d, offset: %d\n", ti->ti_module,
        ti->ti_offset);
    ASSERT(ti->ti_module < tls_info.num_mods);
    return (os_get_priv_tls_base(NULL, TLS_REG_LIB) - tls_info.offs[ti->ti_module] +
            ti->ti_offset);
}

void *
redirect____tls_get_addr()
{
    tls_index_t *ti;
    /* XXX: in some version of ___tls_get_addr, ti is passed via xax
     * How can I generalize it?
     */
#ifdef DR_HOST_NOT_TARGET
    ti = NULL;
    ASSERT_NOT_REACHED();
#elif defined(X86)
    asm("mov %%" ASM_XAX ", %0" : "=m"((ti)) : : ASM_XAX);
#elif defined(AARCH64)
    /* FIXME i#1569: NYI */
    asm("str x0, %0" : "=m"((ti)) : : "x0");
    ASSERT_NOT_REACHED();
#elif defined(ARM)
    /* XXX: assuming ti is passed via r0? */
    asm("str r0, %0" : "=m"((ti)) : : "r0");
    ASSERT_NOT_REACHED();
#elif defined(RISCV64)
    /* FIXME i#3544: Check if ti is in a0. */
    asm("sd a0, %0" : "=m"((ti)) : : "a0");
#endif /* X86/ARM/RISCV64 */
    LOG(GLOBAL, LOG_LOADER, 4, "__tls_get_addr: module: %d, offset: %d\n", ti->ti_module,
        ti->ti_offset);
    ASSERT(ti->ti_module < tls_info.num_mods);
    return (os_get_priv_tls_base(NULL, TLS_REG_LIB) - tls_info.offs[ti->ti_module] +
            ti->ti_offset);
}
