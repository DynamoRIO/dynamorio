/* *******************************************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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
 * loader_macos.c: MacOSX-specific private loader code
 *
 * XXX i#1285: implement MacOS private loader
 */

#include "../globals.h"
#include "../module_shared.h"
#include "tls.h"
#include <mach/mach.h>
#include "dr_tools.h"

/* clang-only I think */
#include <ptrauth.h>

/****************************************************************************
 * Thread Local Storage
 * This file currently only implements private macOS TLS for ARM64
 */

/* from XNU: libsyscall/os/tsd.h */
#define TSD_THREAD_SELF 0
#define TSD_ERRNO 1
#define TSD_MIG_REPLY 2
#define TSD_MACH_THREAD_SELF 3
#define TSD_PTR_MUNGE 7

/* at least on macOS aarch64 we have 16K pages */

/* we just put it at the end of the mapping */
#define ERRNO_OFFSET (PAGE_SIZE - 8)
#define PTHREAD_TLS_OFFSET 0xe0

#ifdef AARCH64
static uintptr_t pthread_ptr_munge_token;
#endif

void
privload_mod_tls_init(privmod_t *mod)
{
    /* XXX i#1285: implement MacOS private loader */
    ASSERT_NOT_IMPLEMENTED(false);
}

void *
privload_tls_init(void *app_tls)
{
#if defined(AARCH64)
    void **cur_tls = (void **)read_thread_register(TLS_REG_LIB);
    if (cur_tls && !pthread_ptr_munge_token) {
        pthread_ptr_munge_token = (uintptr_t) * (cur_tls + TSD_PTR_MUNGE);
    }

    /* XXX i#1285: implement MacOS private loader */
    byte *pthread = NULL;

    /* We use the mach vm_allocate API here since client threads may need
     * a valid TLS even after the heap has been cleaned up.
     */
    IF_DEBUG(kern_return_t res =)
    vm_allocate(mach_task_self(), (vm_address_t *)&pthread, PAGE_SIZE,
                true /* anywhere */);
    ASSERT(res == KERN_SUCCESS);

    memset(pthread, 0, PAGE_SIZE);

    ASSERT(ALIGNED(pthread, PAGE_SIZE));

    uint64_t *tls = (uint64_t *)(pthread + PTHREAD_TLS_OFFSET);
    tls[TSD_MACH_THREAD_SELF] = mach_thread_self();
    tls[TSD_ERRNO] = (uint64_t)(pthread + ERRNO_OFFSET);
    tls[TSD_THREAD_SELF] = (uint64_t)(pthread);

    /* pthread->_sig */
    void *sig = pthread;
    if (proc_has_feature(FEATURE_PAUTH)) { /* XXX: what is the equivalent of
                                              __has_feature(ptrauth_calls)? */
        uint64_t modifier = 0x5b9;
        __asm__ volatile("pacdb %[ptr], %[mod]"
                         : [ptr] "=r"(sig)
                         : "0"(sig), [mod] "r"(modifier)
                         : /* no clobbers needed */
        );
    }

    *(uint64_t *)(pthread) = (uintptr_t)sig ^ pthread_ptr_munge_token;
    return tls;
#else
    ASSERT_NOT_IMPLEMENTED(false);
    return NULL;
#endif
}

void
privload_tls_exit(void *dr_tp)
{
#if defined(AARCH64)
    ASSERT(ALIGNED(dr_tp - PTHREAD_TLS_OFFSET, PAGE_SIZE));

    IF_DEBUG(kern_return_t res =)
    vm_deallocate(mach_task_self(), (vm_address_t)(dr_tp - PTHREAD_TLS_OFFSET),
                  PAGE_SIZE);

    ASSERT(res == KERN_SUCCESS);
    if (read_thread_register(TLS_REG_LIB) == (uint64_t)dr_tp) {
        write_thread_register(NULL);
    }
#else
    ASSERT_NOT_IMPLEMENTED(false);
#endif
}
