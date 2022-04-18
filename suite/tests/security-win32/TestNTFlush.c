/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

#include "tools.h"

#define BUF_LEN 160 /* nice somewhat arbitrary length */
#define BUF2_LEN 3 * 4096

static char buf[BUF_LEN];
static char buf2[BUF2_LEN];

#define VERBOSE2(msg, arg1, arg2)
#define VERBOSE3(msg, arg1, arg2, arg3)

static void
do_test(char *buf, size_t len, Copy_Mode mode)
{
    char *code;
    size_t copied_len;
    code = copy_to_buf(buf, len, &copied_len, CODE_INC, mode);
    VERBOSE2("wrote code " PFX "-" PFX "\n", code, code + copied_len);
    NTFlush(code, copied_len);
    VERBOSE3("flushed " PFX "-" PFX " (0x%x bytes)\n", code, code + copied_len,
             copied_len);
    VERBOSE2("executing code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 1); // 2
    VERBOSE2("executing code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 2); // 3
    code = copy_to_buf(buf, len, &copied_len, CODE_DEC, mode);
    VERBOSE2("wrote code " PFX "-" PFX "\n", code, code + copied_len);
    VERBOSE2("executing code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 1); // 0
    code = copy_to_buf(buf, len, &copied_len, CODE_INC, mode);
    VERBOSE2("wrote code " PFX "-" PFX "\n", code, code + copied_len);
    /* do whole buf here so catch region of self mod below */
    NTFlush(buf, len);
    VERBOSE3("flushed " PFX "-" PFX " (0x%x bytes)\n", buf, buf + len, len);
    VERBOSE2("executing code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 2); // 3
    code = copy_to_buf(buf, len, &copied_len, CODE_SELF_MOD, mode);
    VERBOSE2("wrote code " PFX "-" PFX "\n", code, code + copied_len);
    VERBOSE2("executing self-mod code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 43981);
    NTFlush(code, copied_len);
    VERBOSE3("flushed " PFX "-" PFX " (0x%x bytes)\n", code, code + copied_len,
             copied_len);
    VERBOSE2("executing self-mod code " PFX "-" PFX "\n", code, code + copied_len);
    test_print(code, 4660);
}

int
main()
{
    char buf_stack[BUF_LEN];
    char buf2_stack[BUF2_LEN];
    INIT();

#if USE_DYNAMO
    dynamorio_app_init();
    dynamorio_app_start();
#endif

    print("starting tests\n");

    do_test(buf, BUF_LEN, COPY_NORMAL);
    do_test(buf2, BUF2_LEN, COPY_CROSS_PAGE);

    do_test(buf_stack, BUF_LEN, COPY_NORMAL);
    do_test(buf2_stack, BUF2_LEN, COPY_CROSS_PAGE);

    print("about to exit\n");

#if USE_DYNAMO
    dynamorio_app_stop();
    dynamorio_app_exit();
#endif
}
