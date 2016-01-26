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
 * loader_android.c: Android-specific private loader code
 */

#include "../globals.h"
#include "../module_shared.h"
#include "include/syscall.h"
#include "tls.h"
#include "include/android_linker.h"
#include <stddef.h>

/****************************************************************************
 * Thread Local Storage
 */

static android_kernel_args_t kernel_args;
static android_pthread_internal_t init_thread;

extern void *kernel_init_sp;

#ifdef X64
# define PTHREAD_TLS_OFFS 176
#else
# define PTHREAD_TLS_OFFS  72
#endif

void
privload_mod_tls_init(privmod_t *mod)
{
    /* Android does not yet support per-module TLS */
}

/* We expect app_tls to be either NULL or garbage, as we have early injection */
void *
privload_tls_init(void *app_tls)
{
    char **e;

    /* We have to exactly duplicate the offset of key fields in Android's
     * pthread_internal_t struct.
     */
    ASSERT(PTHREAD_TLS_OFFS == offsetof(android_pthread_internal_t, tls));
    ASSERT(DR_TLS_BASE_OFFSET == offsetof(android_pthread_internal_t, dr_tls_base) -
           PTHREAD_TLS_OFFS /* the self slot */);

    init_thread.tid = dynamorio_syscall(SYS_set_tid_address, 1, &init_thread.tid);
    init_thread.cached_pid_ = init_thread.tid;

    /* init_thread.attr is set to all 0 (sched is SCHED_NORMAL==0, and sizes are
     * zeroed out)
     */

    /* init_thread.join_state is set to 0 (THREAD_NOT_JOINED) */

    init_thread.tls[ANDROID_TLS_SLOT_SELF] = init_thread.tls;
    init_thread.tls[ANDROID_TLS_SLOT_THREAD_ID] = &init_thread;
    /* tls[TLS_SLOT_STACK_GUARD] is set to 0 */

    /* Set up the data struct pointing at kernel args that Bionic expects */
    kernel_args.argc = *(int *)kernel_init_sp;
    kernel_args.argv = (char **)kernel_init_sp + 1;
    kernel_args.envp = kernel_args.argv + kernel_args.argc + 1;
    /* The aux vector is after the last environment pointer. */
    for (e = kernel_args.envp; *e != NULL; e++)
        ; /* nothing */
    kernel_args.auxv = (ELF_AUXV_TYPE *)(e + 1);
    init_thread.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT] = &kernel_args;

    /* We use our own alternate signal stack */

    LOG(GLOBAL, LOG_LOADER, 2, "%s: kernel sp is "PFX"; TLS set to "PFX"\n",
        __FUNCTION__, init_thread.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT],
        init_thread.tls[ANDROID_TLS_SLOT_SELF]);

    /* Android does not yet support per-module TLS */

    return init_thread.tls[ANDROID_TLS_SLOT_SELF];
}

void
privload_tls_exit(void *dr_tp)
{
    /* nothing to do */
}
