/* *******************************************************************************
 * Copyright (c) 2010-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * tls_linux_x86.c - TLS support via x86 segments
 */

#include "../globals.h"
#include "tls.h"
#include "include/syscall.h" /* our own local copy */

#ifdef X86
#    include "instr.h" /* for SEG_ constants */
#    include "os_private.h"
#endif

#ifndef LINUX
#    error Linux-only
#endif

#include <asm/ldt.h>

#ifdef X64
/* Linux GDT layout in x86_64:
 * #define GDT_ENTRY_TLS_MIN 12
 * #define GDT_ENTRY_TLS_MAX 14
 * #define GDT_ENTRY_TLS 1
 * TLS indexes for 64-bit, hardcode in arch_prctl
 * #define FS_TLS 0
 * #define GS_TLS 1
 * #define GS_TLS_SEL ((GDT_ENTRY_TLS_MIN+GS_TLS)*8 + 3)
 * #define FS_TLS_SEL ((GDT_ENTRY_TLS_MIN+FS_TLS)*8 + 3)
 */
#    define FS_TLS 0 /* used in arch_prctl handling */
#    define GS_TLS 1 /* used in arch_prctl handling */
#else
/* Linux GDT layout in x86_32
 * 6 - TLS segment #1 0x33 [ glibc's TLS segment ]
 * 7 - TLS segment #2 0x3b [ Wine's %fs Win32 segment ]
 * 8 - TLS segment #3 0x43
 * FS and GS is not hardcode.
 */
#endif
#define GDT_ENTRY_TLS_MIN_32 6
#define GDT_ENTRY_TLS_MIN_64 12
/* when x86-64 emulate i386, it still use 12-14, so using ifdef x64
 * cannot detect the right value.
 * The actual value will be updated later in os_tls_app_seg_init.
 */
static uint gdt_entry_tls_min = IF_X64_ELSE(GDT_ENTRY_TLS_MIN_64, GDT_ENTRY_TLS_MIN_32);

static bool tls_global_init = false;

/* GDT slot we use for set_thread_area.
 * This depends on the kernel, not on the app!
 */
static int tls_gdt_index = -1;
/* GDT slot we use for private library TLS. */
static int lib_tls_gdt_index = -1;

#ifdef X64
static bool tls_using_msr;
static bool on_WSL;
#endif

/* Indicates that on the next request for a GDT entry, we should return the GDT
 * entry we stole for private library TLS.  The entry index is in
 * lib_tls_gdt_index.
 * FIXME i#107: For total segment transparency, we can use the same approach
 * with tls_gdt_index.
 */
bool return_stolen_lib_tls_gdt;

#ifdef DEBUG
#    define GDT_32BIT 8  /*  6=NPTL, 7=wine */
#    define GDT_64BIT 14 /* 12=NPTL, 13=wine */
#endif

#ifdef X64
#    define NON_ZERO_UNINIT_GSBASE 0x1000UL
#endif

static int
modify_ldt_syscall(int func, void *ptr, unsigned long bytecount)
{
    return dynamorio_syscall(SYS_modify_ldt, 3, func, ptr, bytecount);
}

/* reading ldt entries gives us the raw format, not struct modify_ldt_ldt_s */
typedef struct {
    unsigned int limit1500 : 16;
    unsigned int base1500 : 16;
    unsigned int base2316 : 8;
    unsigned int type : 4;
    unsigned int not_system : 1;
    unsigned int privilege_level : 2;
    unsigned int seg_present : 1;
    unsigned int limit1916 : 4;
    unsigned int custom : 1;
    unsigned int zero : 1;
    unsigned int seg_32bit : 1;
    unsigned int limit_in_pages : 1;
    unsigned int base3124 : 8;
} raw_ldt_entry_t;

enum {
    LDT_TYPE_CODE = 0x8,
    LDT_TYPE_DOWN = 0x4,
    LDT_TYPE_WRITE = 0x2,
    LDT_TYPE_ACCESSED = 0x1,
};

#define LDT_BASE(ldt) \
    (((ldt)->base3124 << 24) | ((ldt)->base2316 << 16) | (ldt)->base1500)

#ifdef DEBUG
#    if 0 /* not used */
#        ifdef X64
    /* Intel docs confusingly say that app descriptors are 8 bytes while
     * system descriptors are 16; more likely all are 16, as linux kernel
     * docs seem to assume. */
#            error NYI
#        endif
static void
print_raw_ldt(raw_ldt_entry_t *ldt)
{
    LOG(GLOBAL, LOG_ALL, 1,
        "ldt @"PFX":\n", ldt);
    LOG(GLOBAL, LOG_ALL, 1,
        "\traw: "PFX" "PFX"\n", *((unsigned int *)ldt), *(1+(unsigned int *)ldt));
    LOG(GLOBAL, LOG_ALL, 1,
        "\tbase: 0x%x\n", LDT_BASE(ldt));
    LOG(GLOBAL, LOG_ALL, 1,
        "\tlimit: 0x%x\n", (ldt->limit1916<<16) | ldt->limit1500);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tlimit_in_pages: 0x%x\n", ldt->limit_in_pages);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tseg_32bit: 0x%x\n", ldt->seg_32bit);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tseg_present: 0x%x\n", ldt->seg_present);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tprivilege_level: 0x%x\n", ldt->privilege_level);
    LOG(GLOBAL, LOG_ALL, 1,
        "\tnot_system: 0x%x\n", ldt->not_system);
    LOG(GLOBAL, LOG_ALL, 1,
        "\ttype: 0x%x == %s%s%s%s\n", ldt->type,
           (ldt->type & LDT_TYPE_CODE)  ? "code":"data",
           (ldt->type & LDT_TYPE_DOWN)  ? " top-down":"",
           (ldt->type & LDT_TYPE_WRITE) ? " W":" ",
           (ldt->type & LDT_TYPE_ACCESSED)  ? " accessed":"");
}

static void
print_all_ldt(void)
{
    int i, bytes;
    /* can't fit 64K on our stack */
    raw_ldt_entry_t *ldt = global_heap_alloc(sizeof(raw_ldt_entry_t) * LDT_ENTRIES
                                           HEAPACCT(ACCT_OTHER));
    /* make sure our struct size jives w/ ldt.h */
    ASSERT(sizeof(raw_ldt_entry_t) == LDT_ENTRY_SIZE);
    memset(ldt, 0, sizeof(*ldt));
    bytes = modify_ldt_syscall(0, (void *)ldt, sizeof(raw_ldt_entry_t) * LDT_ENTRIES);
    LOG(GLOBAL, LOG_ALL, 3, "read %d bytes, should == %d * %d\n",
        bytes, sizeof(raw_ldt_entry_t), LDT_ENTRIES);
    ASSERT(bytes == 0 /* no ldt entries */ ||
           bytes == sizeof(raw_ldt_entry_t) * LDT_ENTRIES);
    for (i = 0; i < bytes/sizeof(raw_ldt_entry_t); i++) {
        if (((ldt[i].base3124<<24) | (ldt[i].base2316<<16) | ldt[i].base1500) != 0) {
            LOG(GLOBAL, LOG_ALL, 1, "ldt at index %d:\n", i);
            print_raw_ldt(&ldt[i]);
        }
    }
    global_heap_free(ldt, sizeof(raw_ldt_entry_t) * LDT_ENTRIES HEAPACCT(ACCT_OTHER));
}
#    endif /* #if 0 */
#endif     /* DEBUG */

#define LDT_ENTRIES_TO_CHECK 128

/* returns -1 if all indices are in use */
static int
find_unused_ldt_index()
{
    int i, bytes;
    /* N.B.: we don't have 64K of stack for the full LDT_ENTRIES
     * array, and I don't want to allocate a big array on the heap
     * when it's very doubtful any more than a handful of these
     * descriptors are actually in use
     */
    raw_ldt_entry_t ldt[LDT_ENTRIES_TO_CHECK];
    ASSERT(LDT_ENTRIES_TO_CHECK < LDT_ENTRIES);
    /* make sure our struct size jives w/ ldt.h */
    ASSERT(sizeof(raw_ldt_entry_t) == LDT_ENTRY_SIZE);
    memset(ldt, 0, sizeof(ldt));
    bytes = modify_ldt_syscall(0, (void *)ldt, sizeof(ldt));
    if (bytes == 0) {
        /* no indices are taken yet */
        return 0;
    }
    ASSERT(bytes == sizeof(ldt));
    for (i = 0; i < bytes / sizeof(raw_ldt_entry_t); i++) {
        if (((ldt[i].base3124 << 24) | (ldt[i].base2316 << 16) | ldt[i].base1500) == 0) {
            return i;
        }
    }
    return -1;
}

static void
initialize_ldt_struct(our_modify_ldt_t *ldt, void *base, size_t size, uint index)
{
    ASSERT(ldt != NULL);
    ldt->entry_number = index;
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint((ptr_uint_t)base)));
    ldt->base_addr = (int)(ptr_int_t)base;
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(size)));
    ldt->limit = size;
    ldt->seg_32bit = IF_X64_ELSE(0, 1);
    ldt->contents = MODIFY_LDT_CONTENTS_DATA;
    ldt->read_exec_only = 0;
    ldt->limit_in_pages = (size == GDT_NO_SIZE_LIMIT) ? 1 : 0;
    ldt->seg_not_present = 0;
    /* While linux kernel doesn't care if we set this, vmkernel requires it */
    ldt->useable = 1; /* becomes custom AVL bit */
}

static void
clear_ldt_struct(our_modify_ldt_t *ldt, uint index)
{
    /* set fields to match LDT_empty() macro from linux kernel */
    memset(ldt, 0, sizeof(*ldt));
    ldt->seg_not_present = 1;
    ldt->read_exec_only = 1;
    ldt->entry_number = index;
}

static void
create_ldt_entry(void *base, size_t size, uint index)
{
    our_modify_ldt_t array;
    int ret;
    initialize_ldt_struct(&array, base, size, index);
    ret = modify_ldt_syscall(1, (void *)&array, sizeof(array));
    ASSERT(ret >= 0);
}

static void
clear_ldt_entry(uint index)
{
    our_modify_ldt_t array;
    int ret;
    clear_ldt_struct(&array, index);
    ret = modify_ldt_syscall(1, (void *)&array, sizeof(array));
    ASSERT(ret >= 0);
}

/* Queries the set of available GDT slots, and initializes:
 * - tls_gdt_index
 * - gdt_entry_tls_min on ia32
 * - lib_tls_gdt_index if using private loader
 * GDT slots are initialized with a base and limit of zero.  The caller is
 * responsible for setting them to a real base.
 */
static void
choose_gdt_slots(os_local_state_t *os_tls)
{
    our_modify_ldt_t desc;
    int i;
    int avail_index[GDT_NUM_TLS_SLOTS];
    our_modify_ldt_t clear_desc;
    int res;

    /* using local static b/c dynamo_initialized is not set for a client thread
     * when created in client's dr_client_main routine
     */
    /* FIXME: Could be racy if we have multiple threads initializing during
     * startup.
     */
    if (tls_global_init)
        return;
    tls_global_init = true;

    /* We don't want to break the assumptions of pthreads or wine,
     * so we try to take the last slot.  We don't want to hardcode
     * the index b/c the kernel will let us clobber entries so we want
     * to only pass in -1.
     */
    ASSERT(!dynamo_initialized);
    ASSERT(tls_gdt_index == -1);
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++)
        avail_index[i] = -1;
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        /* We use a base and limit of 0 for testing what's available. */
        initialize_ldt_struct(&desc, NULL, 0, -1);
        res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
        LOG(GLOBAL, LOG_THREADS, 4, "%s: set_thread_area -1 => %d res, %d index\n",
            __FUNCTION__, res, desc.entry_number);
        if (res >= 0) {
            /* We assume monotonic increases */
            avail_index[i] = desc.entry_number;
            ASSERT(avail_index[i] > tls_gdt_index);
            tls_gdt_index = desc.entry_number;
        } else
            break;
    }

#ifndef X64
    /* In x86-64's ia32 emulation,
     * set_thread_area(6 <= entry_number && entry_number <= 8) fails
     * with EINVAL (22) because x86-64 only accepts GDT indices 12 to 14
     * for TLS entries.
     */
    if (tls_gdt_index > (gdt_entry_tls_min + GDT_NUM_TLS_SLOTS))
        gdt_entry_tls_min = GDT_ENTRY_TLS_MIN_64; /* The kernel is x64. */
#endif

    /* Now give up the earlier slots */
    for (i = 0; i < GDT_NUM_TLS_SLOTS; i++) {
        if (avail_index[i] > -1 && avail_index[i] != tls_gdt_index) {
            LOG(GLOBAL, LOG_THREADS, 4, "clearing set_thread_area index %d\n",
                avail_index[i]);
            clear_ldt_struct(&clear_desc, avail_index[i]);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &clear_desc);
            ASSERT(res >= 0);
        }
    }

#ifndef VMX86_SERVER
    ASSERT_CURIOSITY(tls_gdt_index == (kernel_is_64bit() ? GDT_64BIT : GDT_32BIT));
#endif

    if (INTERNAL_OPTION(private_loader) && tls_gdt_index != -1) {
        /* Use the app's selector with our own TLS base for libraries.  app_fs
         * and app_gs are initialized by the caller in os_tls_app_seg_init().
         */
        int index = SELECTOR_INDEX(os_tls->app_lib_tls_reg);
        if (index == 0) {
            /* An index of zero means the app has no TLS (yet), and happens
             * during early injection.  We use -1 to grab a new entry.  When the
             * app asks for its first table entry with set_thread_area, we give
             * it this one and emulate its usage of the segment.
             */
            ASSERT_CURIOSITY(DYNAMO_OPTION(early_inject) &&
                             "app has "
                             "no TLS, but we used non-early injection");
            initialize_ldt_struct(&desc, NULL, 0, -1);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 4, "%s: set_thread_area -1 => %d res, %d index\n",
                __FUNCTION__, res, desc.entry_number);
            ASSERT(res >= 0);
            if (res >= 0) {
                return_stolen_lib_tls_gdt = true;
                index = desc.entry_number;
            }
        }
        lib_tls_gdt_index = index;
    } else {
        /* For no private loader, e.g., app statically linked with DR,
         * we use app's lib tls gdt index.
         */
        lib_tls_gdt_index = SELECTOR_INDEX(os_tls->app_lib_tls_reg);
    }
}

void
tls_thread_init(os_local_state_t *os_tls, byte *segment)
{
    /* We have four different ways to obtain TLS, each with its own limitations:
     *
     * 1) Piggyback on the threading system (like we do on Windows): here that would
     *    be pthreads, which uses a segment since at least RH9, and uses gdt-based
     *    segments for NPTL.  The advantage is we won't run out of ldt or gdt entries
     *    (except when the app itself would).  The disadvantage is we're stealing
     *    application slots and we rely on user mode interfaces.
     *
     * 2) Steal an ldt entry via SYS_modify_ldt.  This suffers from the 8K ldt entry
     *    limit and requires that we update manually on a new thread.  For 64-bit
     *    we're limited here to a 32-bit base.  (Strangely, the kernel's
     *    include/asm-x86_64/ldt.h implies that the base is ignored: but it doesn't
     *    seem to be.)
     *
     * 3) Steal a gdt entry via SYS_set_thread_area.  There is a 3rd unused entry
     *    (after pthreads and wine) we could use.  The kernel swaps for us, and with
     *    CLONE_TLS the kernel will set up the entry for a new thread for us.  Xref
     *    PR 192231 and PR 285898.  This system call is disabled on 64-bit 2.6
     *    kernels (though the man page for arch_prctl implies it isn't for 2.5
     *    kernels?!?)
     *
     * 4) Use SYS_arch_prctl.  This is only implemented on 64-bit kernels, and can
     *    only be used to set the gdt entries that fs and gs select for.  Faster to
     *    use <4GB base (obtain with mmap MAP_32BIT) since can use gdt; else have to
     *    use wrmsr.  The man pages say "ARCH_SET_GS is disabled in some kernels".
     */
    uint selector;
    int index = -1;
    int res;
#ifdef X64
    /* First choice is gdt, which means arch_prctl.  Since this may fail
     * on some kernels, we require -heap_in_lower_4GB so we can fall back
     * on modify_ldt.
     */
    byte *cur_gs;
    res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_GET_GS, &cur_gs);
    if (res >= 0) {
        LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: cur gs base is " PFX "\n", cur_gs);
        /* If we're a non-initial thread, gs will be set to the parent thread's value */
        if (cur_gs == NULL || cur_gs == (byte *)NON_ZERO_UNINIT_GSBASE ||
            is_dynamo_address(cur_gs) ||
            /* By resolving i#107, we can handle gs conflicts between app and dr. */
            INTERNAL_OPTION(mangle_app_seg)) {
            res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, segment);
            if (res >= 0) {
                os_tls->tls_type = TLS_TYPE_ARCH_PRCTL;
                LOG(GLOBAL, LOG_THREADS, 1,
                    "os_tls_init: arch_prctl successful for base " PFX "\n", segment);
                res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_GET_GS, &cur_gs);
                if (res >= 0 && cur_gs != segment && !on_WSL) {
                    /* XXX i#1896: on WSL, ARCH_GET_GS is broken and does not return
                     * the true value.  (Plus, fs and gs start out equal to ss (0x2b)
                     * and are not set by ARCH_SET_*).  i#2089's safe read TLS
                     * solution solves this, but we still warn as we haven't fixed
                     * later issues.  Without the safe read we have to abort.
                     */
                    on_WSL = true;
                    LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: running on WSL\n");
                    if (INTERNAL_OPTION(safe_read_tls_init)) {
                        SYSLOG_INTERNAL_WARNING(
                            "Support for the Windows Subsystem for Linux is still "
                            "preliminary, due to missing kernel features.  "
                            "Continuing, but please report any problems encountered.");
                    } else {
                        SYSLOG(SYSLOG_ERROR, WSL_UNSUPPORTED_FATAL, 2,
                               get_application_name(), get_application_pid());
                        os_terminate(NULL, TERMINATE_PROCESS);
                        ASSERT_NOT_REACHED();
                    }
                }
                /* Kernel should have written %gs for us if using GDT */
                if (!dynamo_initialized &&
                    /* We assume that WSL is using MSR */
                    (on_WSL || read_thread_register(SEG_TLS) == 0)) {
                    LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: using MSR\n");
                    tls_using_msr = true;
                }
                if (INTERNAL_OPTION(private_loader)) {
                    res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_FS,
                                            os_tls->os_seg_info.priv_lib_tls_base);
                    /* Assuming set fs must be successful if set gs succeeded. */
                    ASSERT(res >= 0);
                }
            } else {
                /* we've found a kernel where ARCH_SET_GS is disabled */
                ASSERT_CURIOSITY(false && "arch_prctl failed on set but not get");
                LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: arch_prctl failed: error %d\n",
                    res);
            }
        } else {
            /* FIXME PR 205276: we don't currently handle it: fall back on ldt, but
             * we'll have the same conflict w/ the selector...
             */
            ASSERT_BUG_NUM(205276, cur_gs == NULL);
        }
    }
#endif

    if (os_tls->tls_type == TLS_TYPE_NONE) {
        /* Second choice is set_thread_area */
        /* PR 285898: if we added CLONE_SETTLS to all clone calls (and emulated vfork
         * with clone) we could avoid having to set tls up for each thread (as well
         * as solve race PR 207903), at least for kernel 2.5.32+.  For now we stick
         * w/ manual setup.
         */
        our_modify_ldt_t desc;

        /* Pick which GDT slots we'll use for DR TLS and for library TLS if
         * using the private loader.
         */
        choose_gdt_slots(os_tls);

        if (tls_gdt_index > -1) {
            /* Now that we know which GDT slot to use, install the per-thread base
             * into it.
             */
            /* Base here must be 32-bit */
            IF_X64(
                ASSERT(DYNAMO_OPTION(heap_in_lower_4GB) && segment <= (byte *)UINT_MAX));
            initialize_ldt_struct(&desc, segment, PAGE_SIZE, tls_gdt_index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 3, "%s: set_thread_area %d => %d res, %d index\n",
                __FUNCTION__, tls_gdt_index, res, desc.entry_number);
            ASSERT(res < 0 || desc.entry_number == tls_gdt_index);
        } else {
            res = -1; /* fall back on LDT */
        }

        if (res >= 0) {
            LOG(GLOBAL, LOG_THREADS, 1,
                "os_tls_init: set_thread_area successful for base " PFX " @index %d\n",
                segment, tls_gdt_index);
            os_tls->tls_type = TLS_TYPE_GDT;
            index = tls_gdt_index;
            selector = GDT_SELECTOR(index);
            WRITE_DR_SEG(selector); /* macro needs lvalue! */
        } else {
            IF_VMX86(ASSERT_NOT_REACHED()); /* since no modify_ldt */
            LOG(GLOBAL, LOG_THREADS, 1, "os_tls_init: set_thread_area failed: error %d\n",
                res);
        }

        /* Install the library TLS base. */
        if (INTERNAL_OPTION(private_loader) && res >= 0) {
            app_pc base = os_tls->os_seg_info.priv_lib_tls_base;
            /* lib_tls_gdt_index is picked in choose_gdt_slots. */
            ASSERT(lib_tls_gdt_index >= gdt_entry_tls_min);
            initialize_ldt_struct(&desc, base, GDT_NO_SIZE_LIMIT, lib_tls_gdt_index);
            res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
            LOG(GLOBAL, LOG_THREADS, 3, "%s: set_thread_area %d => %d res, %d index\n",
                __FUNCTION__, lib_tls_gdt_index, res, desc.entry_number);
            if (res >= 0) {
                /* i558 update lib seg reg to enforce the segment changes */
                selector = GDT_SELECTOR(lib_tls_gdt_index);
                LOG(GLOBAL, LOG_THREADS, 2, "%s: setting %s to selector 0x%x\n",
                    __FUNCTION__, reg_names[LIB_SEG_TLS], selector);
                WRITE_LIB_SEG(selector);
            }
        }
    }

    if (os_tls->tls_type == TLS_TYPE_NONE) {
        /* Third choice: modify_ldt, which should be available on kernel 2.3.99+ */
        /* Base here must be 32-bit */
        IF_X64(ASSERT(DYNAMO_OPTION(heap_in_lower_4GB) && segment <= (byte *)UINT_MAX));
        /* we have the thread_initexit_lock so no race here */
        index = find_unused_ldt_index();
        selector = LDT_SELECTOR(index);
        ASSERT(index != -1);
        create_ldt_entry((void *)segment, PAGE_SIZE, index);
        os_tls->tls_type = TLS_TYPE_LDT;
        WRITE_DR_SEG(selector); /* macro needs lvalue! */
        LOG(GLOBAL, LOG_THREADS, 1,
            "os_tls_init: modify_ldt successful for base " PFX " w/ index %d\n", segment,
            index);
    }

    os_tls->ldt_index = index;
}

bool
tls_thread_preinit()
{
#ifdef X64
    /* i#3356: Write a non-zero value to the gs base to work around an AMD bug
     * present on pre-4.7 Linux kernels.  See the call to this in our signal
     * handler for more information.
     */
    if (proc_get_vendor() != VENDOR_AMD)
        return true;
    /* First identify a temp-native thread with a real segment in
     * place but just an invalid .magic field.  We do not want to clobber the
     * legitimate segment base in that case.
     */
    if (safe_read_tls_magic() == TLS_MAGIC_INVALID) {
        os_local_state_t *tls = (os_local_state_t *)safe_read_tls_self();
        if (tls != NULL &&
            tls->state.spill_space.dcontext->owning_thread == get_sys_thread_id())
            return true;
    }
    /* XXX: What about Mac on AMD?  Presumably by the time anyone wants to run
     * that combination the Mac kernel will have fixed this if they haven't already.
     */
    /* We just don't have time to support non-arch_prctl and test it. */
    if (tls_global_type != TLS_TYPE_ARCH_PRCTL) {
        ASSERT_BUG_NUM(3356, tls_global_type == TLS_TYPE_ARCH_PRCTL);
        return false;
    }
    int res = dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, NON_ZERO_UNINIT_GSBASE);
    LOG(GLOBAL, LOG_THREADS, 1,
        "%s: set non-zero pre-init gs base for thread " TIDFMT "\n", __FUNCTION__,
        get_sys_thread_id());
    return res == 0;
#else
    return true;
#endif
}

/* i#2089: we skip this for non-detach */
void
tls_thread_free(tls_type_t tls_type, int index)
{
    /* XXX i#107 (and i#2088): We need to restore the segment base the
     * app was using when we detach, instead of just clearing.
     */
    if (tls_type == TLS_TYPE_LDT)
        clear_ldt_entry(index);
    else if (tls_type == TLS_TYPE_GDT) {
        our_modify_ldt_t desc;
        clear_ldt_struct(&desc, index);
        DEBUG_DECLARE(int res =)
        dynamorio_syscall(SYS_set_thread_area, 1, &desc);
        ASSERT(res >= 0);
    }
#ifdef X64
    else if (tls_type == TLS_TYPE_ARCH_PRCTL) {
        byte *restore_base =
            /* i#3356: We need a non-zero value for AMD */
            (proc_get_vendor() == VENDOR_AMD) ? (byte *)NON_ZERO_UNINIT_GSBASE : NULL;
        DEBUG_DECLARE(int res =)
        dynamorio_syscall(SYS_arch_prctl, 2, ARCH_SET_GS, restore_base);
        ASSERT(res >= 0);
        /* syscall re-sets gs register so caller must re-clear it */
    }
#endif
}

/* Assumes it's passed either SEG_FS or SEG_GS.
 * Returns POINTER_MAX on failure.
 */
byte *
tls_get_fs_gs_segment_base(uint seg)
{
#ifdef X86
    uint selector = read_thread_register(seg);
    uint index = SELECTOR_INDEX(selector);
    LOG(THREAD_GET, LOG_THREADS, 4, "%s selector %x index %d ldt %d\n", __func__,
        selector, index, TEST(SELECTOR_IS_LDT, selector));

    if (seg != SEG_FS && seg != SEG_GS)
        return (byte *)POINTER_MAX;

    if (TEST(SELECTOR_IS_LDT, selector)) {
        LOG(THREAD_GET, LOG_THREADS, 4, "selector is LDT\n");
        /* we have to read the entire ldt from 0 to the index */
        size_t sz = sizeof(raw_ldt_entry_t) * (index + 1);
        raw_ldt_entry_t *ldt = global_heap_alloc(sz HEAPACCT(ACCT_OTHER));
        int bytes;
        byte *base;
        memset(ldt, 0, sizeof(*ldt));
        bytes = modify_ldt_syscall(0, (void *)ldt, sz);
        base = (byte *)(ptr_uint_t)LDT_BASE(&ldt[index]);
        global_heap_free(ldt, sz HEAPACCT(ACCT_OTHER));
        if (bytes == sz) {
            LOG(THREAD_GET, LOG_THREADS, 4, "modify_ldt %d => %x\n", index, base);
            return base;
        }
    } else {
        our_modify_ldt_t desc;
        int res;
#    ifdef X64
        byte *base;
        if (running_on_WSL()) {
            /* i#1986: arch_prctl queries fail, so we try to read from the
             * self pointer in the DR or lib TLS.
             */
            if (seg == SEG_TLS)
                base = safe_read_tls_self();
            else
                base = safe_read_tls_app_self();
            LOG(THREAD_GET, LOG_THREADS, 4, "safe read of self %s => " PFX "\n",
                reg_names[seg], base);
            return base;
        }
        res = dynamorio_syscall(SYS_arch_prctl, 2,
                                (seg == SEG_FS ? ARCH_GET_FS : ARCH_GET_GS), &base);
        if (res >= 0) {
            LOG(THREAD_GET, LOG_THREADS, 4, "arch_prctl %s => " PFX "\n", reg_names[seg],
                base);
            return base;
        }
        /* else fall back on get_thread_area */
#    endif /* X64 */
        if (selector == 0)
            return NULL;
        DOCHECKINT(1, {
            uint max_idx =
                IF_VMX86_ELSE(tls_gdt_index, (kernel_is_64bit() ? GDT_64BIT : GDT_32BIT));
            ASSERT_CURIOSITY(index <= max_idx && index >= (max_idx - 2));
        });
        initialize_ldt_struct(&desc, NULL, 0, index);
        res = dynamorio_syscall(SYS_get_thread_area, 1, &desc);
        if (res >= 0) {
            LOG(THREAD_GET, LOG_THREADS, 4, "get_thread_area %d => %x\n", index,
                desc.base_addr);
            return (byte *)(ptr_uint_t)desc.base_addr;
        }
    }
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
#endif /* X86/ARM */
    return (byte *)POINTER_MAX;
}

/* Assumes it's passed either SEG_FS or SEG_GS.
 * Sets only the base: does not change the segment selector register.
 */
bool
tls_set_fs_gs_segment_base(tls_type_t tls_type, uint seg,
                           /* For x64 and TLS_TYPE_ARCH_PRCTL, base is used:
                            * else, desc is used.
                            */
                           byte *base, our_modify_ldt_t *desc)
{
    int res = -1;
#ifdef X86
    if (seg != SEG_FS && seg != SEG_GS)
        return false;
    switch (tls_type) {
#    ifdef X64
    case TLS_TYPE_ARCH_PRCTL: {
        int prctl_code = (seg == SEG_FS ? ARCH_SET_FS : ARCH_SET_GS);
        res = dynamorio_syscall(SYS_arch_prctl, 2, prctl_code, base);
        ASSERT(res >= 0);
        break;
    }
#    endif
    case TLS_TYPE_GDT: {
        res = dynamorio_syscall(SYS_set_thread_area, 1, desc);
        ASSERT(res >= 0);
        break;
    }
    default: {
        ASSERT_NOT_IMPLEMENTED(false);
        return false;
    }
    }
#elif defined(ARM)
    /* FIXME i#1551: NYI on ARM */
    ASSERT_NOT_REACHED();
#endif /* X86/ARM */
    return (res >= 0);
}

void
tls_init_descriptor(our_modify_ldt_t *desc OUT, void *base, size_t size, uint index)
{
    initialize_ldt_struct(desc, base, size, index);
}

bool
tls_get_descriptor(int index, our_modify_ldt_t *desc OUT)
{
    int res;
    /* No support for LDT here */
    ASSERT(tls_global_type != TLS_TYPE_LDT);
    initialize_ldt_struct(desc, NULL, 0, index);
    res = dynamorio_syscall(SYS_get_thread_area, 1, desc);
    if (res < 0) {
        clear_ldt_struct(desc, index);
        return false;
    }
    return true;
}

bool
tls_clear_descriptor(int index)
{
    int res;
    our_modify_ldt_t desc;
    /* No support for LDT here */
    ASSERT(tls_global_type != TLS_TYPE_LDT);
    clear_ldt_struct(&desc, index);
    res = dynamorio_syscall(SYS_set_thread_area, 1, &desc);
    return (res >= 0);
}

int
tls_dr_index(void)
{
    /* No support for LDT here */
    ASSERT(tls_global_type != TLS_TYPE_LDT);
    return tls_gdt_index;
}

int
tls_priv_lib_index(void)
{
    /* No support for LDT here */
    ASSERT(tls_global_type != TLS_TYPE_LDT);
    return lib_tls_gdt_index;
}

bool
tls_dr_using_msr(void)
{
#ifdef X64
    return tls_using_msr;
#else
    return false;
#endif
}

bool
running_on_WSL(void)
{
#ifdef X64
    return on_WSL;
#else
    return false;
#endif
}

void
tls_initialize_indices(os_local_state_t *os_tls)
{
#ifndef X64
    /* Initialize gdt_entry_tls_min on ia32.  On x64, the initial value is
     * correct.
     */
    choose_gdt_slots(os_tls);
#endif
}

int
tls_min_index(void)
{
    /* No support for LDT here */
    ASSERT(tls_global_type != TLS_TYPE_LDT);
#ifndef X64
    /* On x64, the initial value is correct */
    ASSERT(tls_global_init);
#endif
    return gdt_entry_tls_min;
}

#ifdef X64
static void
os_set_dr_seg(dcontext_t *dcontext, reg_id_t seg)
{
    int res;
    os_thread_data_t *ostd = dcontext->os_field;
    res = dynamorio_syscall(SYS_arch_prctl, 2, seg == SEG_GS ? ARCH_SET_GS : ARCH_SET_FS,
                            seg == SEG_GS ? ostd->priv_alt_tls_base
                                          : ostd->priv_lib_tls_base);
    ASSERT(res >= 0);
}

void
tls_handle_post_arch_prctl(dcontext_t *dcontext, int code, reg_t base)
{
    /* XXX: we can move it to pre_system_call to avoid system call. */
    /* i#107 syscalls that might change/query app's segment */
    os_local_state_t *os_tls = get_os_tls();
    switch (code) {
    case ARCH_SET_FS: {
        if (INTERNAL_OPTION(private_loader)) {
            os_thread_data_t *ostd;
            our_modify_ldt_t *desc;
            /* update new value set by app */
            if (TLS_REG_LIB == SEG_FS) {
                os_tls->app_lib_tls_reg = read_thread_register(SEG_FS);
                os_tls->app_lib_tls_base = (void *)base;
            } else {
                os_tls->app_alt_tls_reg = read_thread_register(SEG_FS);
                os_tls->app_alt_tls_base = (void *)base;
            }
            /* update the app_thread_areas */
            ostd = (os_thread_data_t *)dcontext->os_field;
            desc = (our_modify_ldt_t *)ostd->app_thread_areas;
            desc[FS_TLS].entry_number = tls_min_index() + FS_TLS;
            dynamorio_syscall(SYS_get_thread_area, 1, &desc[FS_TLS]);
            /* set it back to the value we are actually using. */
            os_set_dr_seg(dcontext, SEG_FS);
        }
        break;
    }
    case ARCH_GET_FS: {
        if (INTERNAL_OPTION(private_loader)) {
            safe_write_ex((void *)base, sizeof(void *), &os_tls->app_lib_tls_base, NULL);
        }
        break;
    }
    case ARCH_SET_GS: {
        os_thread_data_t *ostd;
        our_modify_ldt_t *desc;
        /* update new value set by app */
        if (TLS_REG_LIB == SEG_GS) {
            os_tls->app_lib_tls_reg = read_thread_register(SEG_GS);
            os_tls->app_lib_tls_base = (void *)base;
        } else {
            os_tls->app_alt_tls_reg = read_thread_register(SEG_GS);
            os_tls->app_alt_tls_base = (void *)base;
        }
        /* update the app_thread_areas */
        ostd = (os_thread_data_t *)dcontext->os_field;
        desc = ostd->app_thread_areas;
        desc[GS_TLS].entry_number = tls_min_index() + GS_TLS;
        dynamorio_syscall(SYS_get_thread_area, 1, &desc[GS_TLS]);
        /* set the value back to the value we are actually using */
        os_set_dr_seg(dcontext, SEG_GS);
        break;
    }
    case ARCH_GET_GS: {
        safe_write_ex((void *)base, sizeof(void *), &os_tls->app_alt_tls_base, NULL);
        break;
    }
    default: {
        ASSERT_NOT_REACHED();
        break;
    }
    } /* switch (dcontext->sys_param0) */
    LOG(THREAD_GET, LOG_THREADS, 2,
        "thread " TIDFMT " segment change => app lib tls base: " PFX ", "
        "alt tls base: " PFX "\n",
        d_r_get_thread_id(), os_tls->app_lib_tls_base, os_tls->app_alt_tls_base);
}
#endif /* X64 */
