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

/* Test querying DR options from a client.
 */

#include "dr_api.h"
#include "client_tools.h"

#include <string.h>

DR_EXPORT void
dr_init(client_id_t client_id)
{
    char buf[DR_MAX_OPTIONS_LENGTH];
    bool success;
    uint64 int_option = 0;

    /* Query existing string option. */
    success = dr_get_string_option("native_exec_list", buf, sizeof(buf));
    ASSERT(success);
    ASSERT(strcmp("foo.dll,bar.dll", buf) == 0);

    /* Query existing integer option. */
    success = dr_get_integer_option("opt_cleancall", &int_option);
    ASSERT(success);
    ASSERT(int_option == 3);

    /* Query existing boolean option. */
    success = dr_get_integer_option("thread_private", &int_option);
    ASSERT(success);
    ASSERT(int_option == 1);
    /* For major behavior changing options, we expose dedicated query APIs which
     * should match the value read from the arbitrary query API.
     */
    ASSERT(dr_using_all_private_caches());

    /* Query non-existent options. */
    int_option = 1;
    success = dr_get_string_option("opt_does_not_exist", buf, sizeof(buf));
    ASSERT(!success);
    success = dr_get_integer_option("opt_does_not_exist", &int_option);
    ASSERT(!success);
    /* Undocumented: we set out val to zero even on failure. */
    ASSERT(int_option == 0);
}
