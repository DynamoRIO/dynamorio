/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drmgr.h"
#include "client_tools.h"
#include <string.h> /* memset */

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

#define ARG_BUF_SIZE 4
#define ARG_STR_BUF_SIZE 400

DR_EXPORT
void
dr_init(client_id_t id)
{
    /* XXX i#2662: Windows not yet supported. */
#ifdef UNIX
    dr_app_arg_t args_buf[ARG_BUF_SIZE];
    char buf[ARG_STR_BUF_SIZE];

    int count = dr_get_app_args(args_buf, ARG_BUF_SIZE);
    CHECK(count == 4, "app count is incorrect");

    const char *app_argv = dr_app_arg_as_utf8(&args_buf[1], buf, ARG_STR_BUF_SIZE);
    CHECK(app_argv != NULL, "should not be NULL");
    CHECK(strcmp(app_argv, "Test") == 0, "first arg should be Test");
    app_argv = dr_app_arg_as_utf8(&args_buf[2], buf, ARG_STR_BUF_SIZE);
    CHECK(app_argv != NULL, "should not be NULL");
    CHECK(strcmp(app_argv, "Test2") == 0, "second arg should be Test2");
    app_argv = dr_app_arg_as_utf8(&args_buf[3], buf, ARG_STR_BUF_SIZE);
    CHECK(app_argv != NULL, "should not be NULL");
    CHECK(strcmp(app_argv, "Test3") == 0, "third arg should be Test3");
#endif
}
