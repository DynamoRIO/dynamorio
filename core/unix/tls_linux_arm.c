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

#include "../globals.h"
#include "tls.h"

#ifndef LINUX
# error Linux-only
#endif

#ifndef ARM
# error ARM-only
#endif

byte **
get_app_tls_swap_slot_addr(void)
{
    byte *app_tls_base = (byte *)read_thread_register(LIB_SEG_TLS);
    if (app_tls_base == NULL) {
        /* FIXME i#1551: NYI if app TLS is not initialized */
        ASSERT_NOT_IMPLEMENTED(false);
        return NULL;
    }
    return (byte **)(app_tls_base + TLS_SWAP_SLOT_OFFSET);
}

void
tls_thread_init(os_local_state_t *os_tls, byte *segment)
{
    byte **tls_swap_slot;

    ASSERT((byte *)(os_tls->self) == segment);
    tls_swap_slot = get_app_tls_swap_slot_addr();
    /* we assume the swap slot is initialized as 0 */
    ASSERT_NOT_IMPLEMENTED(*tls_swap_slot == NULL);
    os_tls->app_tls_swap_slot_value = *tls_swap_slot;
    *tls_swap_slot = segment;
    os_tls->tls_type = TLS_TYPE_SWAP;
}

void
tls_thread_free(tls_type_t tls_type, int index)
{
    byte **tls_swap_slot;
    os_local_state_t *os_tls;

    ASSERT(tls_type == TLS_TYPE_SWAP);
    tls_swap_slot  = get_app_tls_swap_slot_addr();
    os_tls = (os_local_state_t *)*tls_swap_slot;
    ASSERT(os_tls->self == os_tls);
    /* swap back for the case of detach */
    *tls_swap_slot = os_tls->app_tls_swap_slot_value;
    return;
}
