/* **********************************************************
 * Copyright (c) 2012 Google, Inc.  All rights reserved.
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

/* Test an app that has a large amount of __thread data.  This disturbs the
 * location of libc's TLS, making it difficult for the private loader to find
 * it.  If the loader fails to copy it, we will likely crash on some libc string
 * routine that needs locale, like strcasecmp.
 *
 * Layout when the app uses __thread vars:
 *  -------
 *  libc __thread vars, locale, _res, malloc arena, etc
 *  -------
 *  app's __thread vars, in this case 0x200 bytes of it
 *  -------  <---  app fs/gs point here
 *  thread control block, used by pthreads, ld.so, and others
 *  -------
 *
 * Currently we need libc's TLS to be within APP_LIBC_TLS_SIZE bytes of the
 * segment base, but libc independence for DR will make this unnecessary.
 * Clients load a private copy of libc that uses its own tls, so they are
 * unaffected.
 */

#include <string.h>

#include "tools.h"

__thread char tls_data[0x200];

int
main(void)
{
    memset(tls_data, 0, sizeof(tls_data));
    print("all done\n");
    return 0;
}
