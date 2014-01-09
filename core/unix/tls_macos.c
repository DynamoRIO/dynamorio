/* *******************************************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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
# error Mac-only
#endif

tls_type_t tls_global_type;

/* from the (short) machdep syscall table */
#define SYS_i386_get_ldt 6

void
tls_thread_init(os_local_state_t *os_tls, byte *segment)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

void
tls_thread_free(tls_type_t tls_type, int index)
{
    ASSERT_NOT_IMPLEMENTED(false);
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
        return (byte *) POINTER_MAX;

    selector = read_selector(seg);
    index = SELECTOR_INDEX(selector);
    LOG(THREAD_GET, LOG_THREADS, 4, "%s selector %x index %d ldt %d\n",
        __FUNCTION__, selector, index, TEST(SELECTOR_IS_LDT, selector));

    if (!TEST(SELECTOR_IS_LDT, selector) && selector != 0) {
        ASSERT_NOT_IMPLEMENTED(false);
        return (byte *) POINTER_MAX;
    }

    /* The man page is confusing, but experimentation shows it takes in the index,
     * not a selector value.
     */
    res = dynamorio_mach_dep_syscall(SYS_i386_get_ldt, 3, index, &ldt, 1);
    if (res < 0) {
        LOG(THREAD_GET, LOG_THREADS, 4, "%s failed with code 0x%x\n", __FUNCTION__, res);
        ASSERT_NOT_REACHED();
        return (byte *) POINTER_MAX;
    }

    base = (byte *)
        (((ptr_uint_t)ldt.data.base24 << 24) |
         ((ptr_uint_t)ldt.data.base16 << 16) |
         (ptr_uint_t)ldt.data.base00);
    LOG(THREAD_GET, LOG_THREADS, 4, "%s => base "PFX"\n", __FUNCTION__, base);
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
    /* FIXME: for 64-bit, our only option is thread_fast_set_cthread_self64
     * and sharing with the app.  No way to read current base?!?
     * For 32-bit, we can use use thread_set_user_ldt() to set and
     * i386_get_ldt() to read.
     */
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
tls_init_descriptor(our_modify_ldt_t *desc OUT, void *base, size_t size, uint index)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

bool
tls_get_descriptor(int index, our_modify_ldt_t *desc OUT)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
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
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

int
tls_priv_lib_index(void)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}

bool
tls_dr_using_msr(void)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return false;
}

void
tls_initialize_indices(os_local_state_t *os_tls)
{
    ASSERT_NOT_IMPLEMENTED(false);
}

int
tls_min_index(void)
{
    ASSERT_NOT_IMPLEMENTED(false);
    return 0;
}
