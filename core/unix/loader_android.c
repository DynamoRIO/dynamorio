/* *******************************************************************************
 * Copyright (c) 2016-2019 Google, Inc.  All rights reserved.
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
/* Unfortunately the struct layout changes (i#1920): */
static android_v5_pthread_internal_t init_thread_v5;
static android_v6_pthread_internal_t init_thread_v6;
static uint android_version = 6;

extern void *kernel_init_sp;

uint android_tls_base_offs;

static size_t
get_pthread_tls_offs(void)
{
    if (android_version <= 5)
        return offsetof(android_v5_pthread_internal_t, tls);
    else
        return offsetof(android_v6_pthread_internal_t, tls);
}

void
init_android_version(void)
{
#define VER_FILE "/system/build.prop"
#define VER_PROP "ro.build.version.release="
    file_t fd = os_open(VER_FILE, OS_OPEN_READ);
    uint read_ver = 0;
    if (fd != INVALID_FILE) {
        size_t sz = PAGE_SIZE;
        byte *map = d_r_map_file(fd, &sz, 0, NULL, MEMPROT_READ | MEMPROT_WRITE,
                                 MAP_FILE_COPY_ON_WRITE);
        if (map != NULL) {
            const char *prop;
            *(map + sz - 1) = '\0'; /* ensure our strstr stops */
            prop = strstr((const char *)map, VER_PROP);
            if (prop != NULL) {
                if (sscanf(prop + strlen(VER_PROP), "%d", &read_ver) == 1)
                    android_version = read_ver;
            }
            d_r_unmap_file(map, sz);
        }
    }
    LOG(GLOBAL, LOG_LOADER, 1, "Android version %s is %d\n",
        read_ver == 0 ? "(default)" : "from /system/build.prop", android_version);

    /* We have to exactly duplicate the offset of key fields in Android's
     * pthread_internal_t struct.
     */
    if (android_version <= 5) {
        android_tls_base_offs = offsetof(android_v5_pthread_internal_t, dr_tls_base) -
            offsetof(android_v5_pthread_internal_t, tls) /* the self slot */;
        /* i#1931: ensure we do not cross onto a new page. */
        ASSERT(PAGE_START(android_tls_base_offs) ==
               PAGE_START(sizeof(android_v5_pthread_internal_t) - sizeof(void *)));
    } else {
        android_tls_base_offs = offsetof(android_v6_pthread_internal_t, dr_tls_base) -
            offsetof(android_v6_pthread_internal_t, tls) /* the self slot */;
        ASSERT(PAGE_START(android_tls_base_offs) ==
               PAGE_START(sizeof(android_v6_pthread_internal_t) - sizeof(void *)));
    }
}

static size_t
size_of_pthread_internal(void)
{
    if (android_version <= 5)
        return sizeof(android_v5_pthread_internal_t);
    else
        return sizeof(android_v6_pthread_internal_t);
}

void
privload_mod_tls_init(privmod_t *mod)
{
    /* Android does not yet support per-module TLS */
}

/* Called post-reloc. */
void
privload_mod_tls_primary_thread_init(privmod_t *mod)
{
    /* Android does not yet support per-module TLS */
}

void *
privload_tls_init(void *app_tls)
{
    void *res;
    if (!dynamo_initialized) {
        char **e;
        pid_t tid;

        /* We have to duplicate the pthread setup that the Android loader does.
         * We expect app_tls to be either NULL or garbage, as we have early injection.
         */
        tid = dynamorio_syscall(SYS_set_tid_address, 1, &tid);

        /* Set up the data struct pointing at kernel args that Bionic expects */
        kernel_args.argc = *(int *)kernel_init_sp;
        kernel_args.argv = (char **)kernel_init_sp + 1;
        kernel_args.envp = kernel_args.argv + kernel_args.argc + 1;
        /* The aux vector is after the last environment pointer. */
        for (e = kernel_args.envp; *e != NULL; e++)
            ; /* nothing */
        kernel_args.auxv = (ELF_AUXV_TYPE *)(e + 1);

        /* init_thread*.attr is set to all 0 (sched is SCHED_NORMAL==0, and sizes are
         * zeroed out)
         */

        /* init_thread*.join_state is set to 0 (THREAD_NOT_JOINED) */

        /* We use our own alternate signal stack */

        if (android_version <= 5) {
            init_thread_v5.tid = tid;
            init_thread_v5.cached_pid_ = tid;
            init_thread_v5.tls[ANDROID_TLS_SLOT_SELF] = init_thread_v5.tls;
            init_thread_v5.tls[ANDROID_TLS_SLOT_THREAD_ID] = &init_thread_v5;
            /* tls[TLS_SLOT_STACK_GUARD] is set to 0 */
            init_thread_v5.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT] = &kernel_args;
            LOG(GLOBAL, LOG_LOADER, 2, "%s: kernel sp is " PFX "; TLS set to " PFX "\n",
                __FUNCTION__, init_thread_v5.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT],
                init_thread_v5.tls[ANDROID_TLS_SLOT_SELF]);
            res = init_thread_v5.tls[ANDROID_TLS_SLOT_SELF];
        } else {
            init_thread_v6.tid = tid;
            init_thread_v6.cached_pid_ = tid;
            init_thread_v6.tls[ANDROID_TLS_SLOT_SELF] = init_thread_v6.tls;
            init_thread_v6.tls[ANDROID_TLS_SLOT_THREAD_ID] = &init_thread_v6;
            /* tls[TLS_SLOT_STACK_GUARD] is set to 0 */
            init_thread_v6.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT] = &kernel_args;
            LOG(GLOBAL, LOG_LOADER, 2, "%s: kernel sp is " PFX "; TLS set to " PFX "\n",
                __FUNCTION__, init_thread_v6.tls[ANDROID_TLS_SLOT_BIONIC_PREINIT],
                init_thread_v6.tls[ANDROID_TLS_SLOT_SELF]);
            res = init_thread_v6.tls[ANDROID_TLS_SLOT_SELF];
        }
    } else {
        res = heap_mmap(ALIGN_FORWARD(size_of_pthread_internal(), PAGE_SIZE),
                        MEMPROT_READ | MEMPROT_WRITE, VMM_SPECIAL_MMAP);
        LOG(GLOBAL, LOG_LOADER, 2,
            "%s: allocated new TLS at " PFX "; copying from " PFX "\n", __FUNCTION__, res,
            app_tls);
        if (app_tls != NULL)
            memcpy(res, app_tls, size_of_pthread_internal());
        if (android_version <= 5) {
            android_v5_pthread_internal_t *thrd;
            thrd = (android_v5_pthread_internal_t *)res;
            thrd->tls[ANDROID_TLS_SLOT_SELF] = thrd->tls;
            thrd->tls[ANDROID_TLS_SLOT_THREAD_ID] = thrd;
            thrd->tid = d_r_get_thread_id();
            thrd->dr_tls_base = NULL;
            res = thrd->tls[ANDROID_TLS_SLOT_SELF];
            LOG(GLOBAL, LOG_LOADER, 2, "%s: TLS set to " PFX "\n", __FUNCTION__,
                thrd->tls[ANDROID_TLS_SLOT_SELF]);
        } else {
            android_v6_pthread_internal_t *thrd;
            thrd = (android_v6_pthread_internal_t *)res;
            thrd->tls[ANDROID_TLS_SLOT_SELF] = thrd->tls;
            thrd->tls[ANDROID_TLS_SLOT_THREAD_ID] = thrd;
            thrd->tid = d_r_get_thread_id();
            thrd->dr_tls_base = NULL;
            res = thrd->tls[ANDROID_TLS_SLOT_SELF];
            LOG(GLOBAL, LOG_LOADER, 2, "%s: TLS set to " PFX "\n", __FUNCTION__,
                thrd->tls[ANDROID_TLS_SLOT_SELF]);
        }
    }

    /* Android does not yet support per-module TLS */

    return res;
}

void
privload_tls_exit(void *dr_tp)
{
    byte *alloc;
    if (dr_tp == NULL || dr_tp == init_thread_v5.tls || dr_tp == init_thread_v6.tls)
        return;
    alloc = (byte *)dr_tp - get_pthread_tls_offs();
    heap_munmap(alloc, ALIGN_FORWARD(size_of_pthread_internal(), PAGE_SIZE),
                VMM_SPECIAL_MMAP);
}

/* For standalone lib usage (i#1862: the Android loader passes
 * *nothing* to lib init routines).  This will only succeed prior to
 * Bionic's initializer, which clears the tls slot.
 */
bool
get_kernel_args(int *argc OUT, char ***argv OUT, char ***envp OUT)
{
    android_kernel_args_t *kargs;
    void **tls = (void **)get_segment_base(TLS_REG_LIB);
    if (tls != NULL) {
        kargs = (android_kernel_args_t *)tls[ANDROID_TLS_SLOT_BIONIC_PREINIT];
        if (kargs != NULL) {
            *argc = kargs->argc;
            *argv = kargs->argv;
            *envp = kargs->envp;
            return true;
        }
    }
    return false;
}
