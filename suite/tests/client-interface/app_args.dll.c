/* **********************************************************
 * Copyright (c) 2020-2022 Google, Inc.  All rights reserved.
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

    void *drcontext = dr_get_current_drcontext();

    int num_args = dr_num_app_args();
    CHECK(num_args == 4, "number of args is incorrect");

    int count = dr_get_app_args(args_buf, -1);
    CHECK(count == -1, "routine should fail");
    dr_error_code_t error_code = dr_get_error_code(drcontext);
    CHECK(error_code == DR_ERROR_INVALID_PARAMETER, "error code should be invalid param");

    count = dr_get_app_args(args_buf, ARG_BUF_SIZE);
    CHECK(count == 4, "app count is incorrect");

    const char *failed_argv = dr_app_arg_as_cstring(NULL, buf, ARG_STR_BUF_SIZE);
    CHECK(failed_argv == NULL, "should be NULL");
    error_code = dr_get_error_code(drcontext);
    CHECK(error_code == DR_ERROR_INVALID_PARAMETER, "error code should be invalid param");

    const char *app_argv = dr_app_arg_as_cstring(&args_buf[1], buf, ARG_STR_BUF_SIZE);
    CHECK(app_argv != NULL, "should not be NULL");
    CHECK(strcmp(app_argv, "Test") == 0, "first arg should be Test");

    const char *app_argv2 = dr_app_arg_as_cstring(&args_buf[2], buf, ARG_STR_BUF_SIZE);
    CHECK(app_argv2 != NULL, "should not be NULL");
    CHECK(strcmp(app_argv2, "Test2") == 0, "second arg should be Test2");

    const char *aapp_argv3 = dr_app_arg_as_cstring(&args_buf[3], buf, ARG_STR_BUF_SIZE);
    CHECK(aapp_argv3 != NULL, "should not be NULL");
    CHECK(strcmp(aapp_argv3, "Test3") == 0, "third arg should be Test3");
#endif
}
