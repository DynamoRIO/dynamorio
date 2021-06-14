/* **********************************************************
 * Copyright (c) 2011-2021 Google, Inc.  All rights reserved.
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

/* Test passing a really long (600 chars) client option string.  Previously we
 * were truncating at 512 bytes, which is too short.  Now we accept DR options
 * strings up to 1024 chars in size.
 *
 * FIXME: 1024 chars is not very long, but we stack allocate these buffers on
 * dstack before the heap is initialized.  We should go back and see what it
 * would take to remove this limitation.
 */

#include "dr_api.h"
#ifdef UNIX
#    include <string.h>
#endif

static void
event_exit(void)
{
    dr_fprintf(STDERR, "large_options exiting\n");
}

DR_EXPORT void
dr_init(client_id_t client_id)
{
    const char *opts = dr_get_options(client_id);
#ifdef UNIX
    /* Test i#4892. */
    if (strchr(dr_get_application_name(), '/')) {
        dr_fprintf(STDERR, "dr_get_application_name() has slashes!\n");
    }
#endif
    dr_fprintf(STDERR, "large_options passed: %s\n", opts);
    dr_register_exit_event(event_exit);
}
