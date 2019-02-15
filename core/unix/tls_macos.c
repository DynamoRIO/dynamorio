/* *******************************************************************************
 * Copyright (c) 2013-2019 Google, Inc.  All rights reserved.
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
 * tls_macos.c - tls support from the kernel
 *
 * FIXME i#58: NYI (see comments below as well):
 * + not at all implemented, though 32-bit seems straightforward
 * + don't have a good story for 64-bit
 * + longer-term i#1291: use raw syscalls instead of libSystem wrappers
 */

#include "../globals.h"
#include "tls.h"
#include <architecture/i386/table.h>
#include <i386/user_ldt.h>

#ifndef MACOS
#    error Mac-only
#endif

/* From the (short) machdep syscall table */
#define SYS_thread_set_user_ldt 4
#define SYS_i386_set_ldt 5
#define SYS_i386_get_ldt 6

#ifdef X64
#    error TLS NYI
#else
/* This is what thread_set_user_ldt and i386_set_ldt give us.
 * XXX: a 32-bit Mac kernel will return 0x3f?
 * If so, update GDT_NUM_TLS_SLOTS in tls.h.
 */
#    define TLS_DR_SELECTOR 0x1f
#    define TLS_DR_INDEX 0x3
#endif

static uint tls_app_index;

void
tls_thread_init(os_local_state_t *os_tls, byte *segment)
{
#ifdef X64
    /* FIXME: for 64-bit, our only option is thread_fast_set_cthread_self64
     * and sharing with the app.  No way to read current base?!?
     */
#    error NYI
#else
    /* SYS_thread_set_user_ldt looks appealing, as it has built-in kernel
     * support which swaps it on thread switches.
     * However, when I invoke it, while the call succeeds and returns the
     * expected 0x1f, I get a fault when I load that selector value into %fs.
     * Thus we fall back to i386_set_ldt.
     */
    ldt_t ldt;
    int res;

    ldt.data.base00 = (ushort)(ptr_uint_t)segment;
    ldt.data.base16 = (byte)((ptr_uint_t)segment >> 16);
    ldt.data.base24 = (byte)((ptr_uint_t)segment >> 24);
    ldt.data.limit00 = (ushort)PAGE_SIZE;
    ldt.data.limit16 = 0;
    ldt.data.type = DESC_DATA_WRITE;
    ldt.data.dpl = USER_PRIVILEGE;
    ldt.data.present = 1;
    ldt.data.stksz = DESC_DATA_32B;
    ldt.data.granular = 0;

    res = dynamorio_mach_dep_syscall(SYS_i386_set_ldt, 3, LDT_AUTO_ALLOC, &ldt, 1);
    if (res < 0) {
        LOG(THREAD_GET, LOG_THREADS, 4, "%s failed with code %d\n", __FUNCTION__, res);
        ASSERT_NOT_REACHED();
    } else {
        uint index = (uint)res;
        uint selector = LDT_SELECTOR(index);
        /* XXX i#1405: we end up getting index 3 for the 1st thread,
         * but later ones seem to need new slots (originally I thought
         * the kernel would swap our one slot for us).  We leave
         * GDT_NUM_TLS_SLOTS as just 3 under the assumption the app
         * won't use more than that.
         */
        ASSERT(dynamo_initialized || selector == TLS_DR_SELECTOR);
        LOG(THREAD_GET, LOG_THREADS, 2, "%s: LDT index %d\n", __FUNCTION__, res);
        os_tls->tls_type = TLS_TYPE_LDT;
        os_tls->ldt_index = selector;
        WRITE_DR_SEG(selector); /* macro needs lvalue! */
    }
#endif
}

bool
tls_thread_preinit()
{
    return true;
}

#ifndef X64
/* The kernel clears fs in signal handlers, so we have to re-instate our selector */
void
tls_reinstate_selector(uint selector)
{
    /* We can't assert that selector == TLS_DR_SELECTOR b/c of i#1405 */
    WRITE_DR_SEG(selector); /* macro needs lvalue! */
}
#endif

void
tls_thread_free(tls_type_t tls_type, int index)
{
#ifdef X64
    /* FIXME: for 64-bit, our only option is thread_fast_set_cthread_self64
     * and sharing with the app.  No way to read current base?!?
     */
#    error NYI
#else
    int res = dynamorio_mach_dep_syscall(SYS_thread_set_user_ldt, 3, NULL, 0, 0);
    if (res < 0) {
        LOG(THREAD_GET, LOG_THREADS, 4, "%s failed with code %d\n", __FUNCTION__, res);
        ASSERT_NOT_REACHED();
    }
#endif
}

/* Assumes it's passed either SEG_FS or SEG_GS.
 * Returns POINTER_MAX on failure.
 */
byte *
tls_get_fs_gs_segment_base(uint seg)
{
    uint selector;
    uint index;
    ldt_t ldt;
    byte *base;
    int res;

    if (seg != SEG_FS && seg != SEG_GS)
        return (byte *)POINTER_MAX;

    selector = read_thread_register(seg);
    index = SELECTOR_INDEX(selector);
    LOG(THREAD_GET, LOG_THREADS, 4, "%s selector %x index %d ldt %d\n", __FUNCTION__,
        selector, index, TEST(SELECTOR_IS_LDT, selector));

    if (!TEST(SELECTOR_IS_LDT, selector) && selector != 0) {
        ASSERT_NOT_IMPLEMENTED(false);
        return (byte *)POINTER_MAX;
    }

    /* The man page is confusing, but experimentation shows it takes in the index,
     * not a selector value.
     */
    res = dynamorio_mach_dep_syscall(SYS_i386_get_ldt, 3, index, &ldt, 1);
    if (res < 0) {
        LOG(THREAD_GET, LOG_THREADS, 4, "%s failed with code %d\n", __FUNCTION__, res);
        ASSERT_NOT_REACHED();
        return (byte *)POINTER_MAX;
    }

    base = (byte *)(((ptr_uint_t)ldt.data.base24 << 24) |
                    ((ptr_uint_t)ldt.data.base16 << 16) | (ptr_uint_t)ldt.data.base00);
    LOG(THREAD_GET, LOG_THREADS, 4, "%s => base " PFX "\n", __FUNCTION__, base);
    return base;
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
    /* XXX: we may want to refactor os.c + tls.h to not use our_modify_ldt_t on MacOS */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
tls_init_descriptor(our_modify_ldt_t *desc OUT, void *base, size_t size, uint index)
{
    /* XXX: we may want to refactor os.c + tls.h to not use our_modify_ldt_t on MacOS */
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
tls_get_descriptor(int index, our_modify_ldt_t *desc OUT)
{
    /* XXX: we may want to refactor os.c and tls.h to not use our_modify_ldt_t
     * on MacOS.  For now we implement the handful of such interactions we
     * need to get the initial port running.
     */
    ldt_t ldt;
    int res = dynamorio_mach_dep_syscall(SYS_i386_get_ldt, 3, index, &ldt, 1);
    if (res < 0) {
        memset(desc, 0, sizeof(*desc));
        return false;
    }
    desc->entry_number = index;
    desc->base_addr = (((uint)ldt.data.base24 << 24) | ((uint)ldt.data.base16 << 16) |
                       (uint)ldt.data.base00);
    desc->limit = ((uint)ldt.data.limit16 << 16) | (uint)ldt.data.limit00;
    desc->seg_32bit = ldt.data.stksz;
    desc->contents = ldt.data.type >> 2;
    desc->read_exec_only = !(ldt.data.type & 2);
    desc->limit_in_pages = ldt.data.granular;
    desc->seg_not_present = (ldt.data.present ? 0 : 1);
    desc->useable = 1; /* AVL not exposed in code_desc_t */
    return true;
}

bool
tls_clear_descriptor(int index)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

int
tls_dr_index(void)
{
    return TLS_DR_INDEX;
}

int
tls_priv_lib_index(void)
{
    /* XXX i#1285: implement MacOS private loader */
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
tls_dr_using_msr(void)
{
#ifdef X64
    ASSERT_NOT_IMPLEMENTED(false);
#endif
    return false;
}

void
tls_initialize_indices(os_local_state_t *os_tls)
{
#ifdef X64
#    error NYI
#else
    uint selector = read_thread_register(SEG_GS);
    tls_app_index = SELECTOR_INDEX(selector);
    /* We assume app uses 1 and we get 3 (see TLS_DR_INDEX) */
    ASSERT(tls_app_index == 1);
#endif
}

int
tls_min_index(void)
{
    return tls_app_index;
}

void
tls_early_init(void)
{
    /* nothing */
}
