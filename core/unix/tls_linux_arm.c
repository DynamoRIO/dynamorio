/* *******************************************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * tls_linux_arm.c - TLS support on ARM
 */

#include <stddef.h> /* offsetof */
#include "../globals.h"
#include "tls.h"
#include "include/syscall.h"

#ifndef LINUX
# error Linux-only
#endif

#ifndef ARM
# error ARM-only
#endif

/* Get the offset of app_tls_swap in os_local_state_t.
 * They should be used with os_tls_offset or RESTORE_FROM_TLS,
 * so do not need add TLS_OS_LOCAL_STATE here.
 */
ushort
os_get_app_tls_swap_offset(void)
{
    return offsetof(os_local_state_t, app_tls_swap);
}

byte **
get_app_tls_swap_addr(void)
{
    byte *app_tls_base = (byte *)read_thread_register(LIB_SEG_TLS);
    if (app_tls_base == NULL) {
        ASSERT_NOT_REACHED();
        return NULL;
    }
    return (byte **)(app_tls_base + APP_TLS_SWAP_SLOT);
}

void
tls_thread_init(os_local_state_t *os_tls, byte *segment)
{
    byte **tls_swap_slot;

    ASSERT((byte *)(os_tls->self) == segment);
    tls_swap_slot = get_app_tls_swap_addr();
    /* we assume the swap slot is initialized as 0 */
    ASSERT_NOT_IMPLEMENTED(*tls_swap_slot == NULL);
    os_tls->app_tls_swap = *tls_swap_slot;
    *tls_swap_slot = segment;
    os_tls->tls_type = TLS_TYPE_SWAP;
}

void
tls_thread_free(tls_type_t tls_type, int index)
{
    byte **tls_swap_slot;
    os_local_state_t *os_tls;

    ASSERT(tls_type == TLS_TYPE_SWAP);
    tls_swap_slot = get_app_tls_swap_addr();
    os_tls = (os_local_state_t *)*tls_swap_slot;
    ASSERT(os_tls->self == os_tls);
    /* swap back for the case of detach */
    *tls_swap_slot = os_tls->app_tls_swap;
    return;
}

void
tls_early_init(void)
{
    /* App TLS is not yet initialized (we're probably using early injection).
     * We set up our own and "steal" its slot.  When app pthread inits it will
     * clobber it (but from code cache: and DR won't rely on swapped slot there)
     * and it will keep working seamlessly.  Strangely, tpidrro is not zero
     * though, so we do this here via explicit early invocation and not inside
     * get_app_tls_swap_slot_addr().
     */
    static byte early_app_fake_tls[16];
    int res;
    /* We assume we're single-threaded, b/c every dynamic app will have
     * this set up prior to creating any threads.
     */
    ASSERT(!dynamo_initialized);
    ASSERT(sizeof(early_app_fake_tls) >= APP_TLS_SWAP_SLOT + sizeof(void*));
    res = dynamorio_syscall(SYS_set_tls, 1, early_app_fake_tls);
    ASSERT(res == 0);
    ASSERT((byte *)read_thread_register(LIB_SEG_TLS) == early_app_fake_tls);
}
