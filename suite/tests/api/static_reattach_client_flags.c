/* **********************************************************
 * Copyright (c) 2018 Google, Inc.  All rights reserved.
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

#define _CRT_SECURE_NO_WARNINGS 1
#include "configure.h"
#include "dr_api.h"
#include "tools.h"
#include <math.h>
#include <stdlib.h>

/* A test to verify that flags are appropriately piped through to client
 * libraries for static reattach, verifying that the lazy-loading logic is
 * correctly reset for reattach.
 */

// last_argv is updated in dr_client_main with a copy of the 1st argv passed to
// that function.
static char last_argv[256];

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    print("in dr_client_main with argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        print("\tArg %d: |%s|\n", i, argv[i]);
    }
    if (argc != 2) {
        print("ERROR: want only 2 argc!\n");
        return;
    };

    strncpy(last_argv, argv[1], sizeof(last_argv));
}

typedef struct {
    const char *input_dynamorio_options;
    const char *want_argv;
} test_arg_t;

#define TEST_ARG_COUNT 3
const test_arg_t test_args[TEST_ARG_COUNT] = {
    {
        .input_dynamorio_options = " -client_lib ';;a'",
        .want_argv = "a",
    },
    {
        .input_dynamorio_options = " -client_lib ';;b'",
        .want_argv = "b",
    },
    {
        .input_dynamorio_options = " -client_lib ';;c'",
        .want_argv = "c",
    },
};

int
main(int argc, const char *argv[])
{
    int failed = 0;
#define BUF_LEN 256
    char original_options[BUF_LEN];
    if (!my_getenv("DYNAMORIO_OPTIONS", original_options, BUF_LEN)) {
        print("Couldn't read DYNAMORIO_OPTIONS!\n");
        return 1;
    }
    print("Got DYNAMORIO_OPTIONS: %s\n", original_options);
    for (int i = 0; i < TEST_ARG_COUNT; i++) {
        char option_buffer[BUF_LEN];
        strncpy(option_buffer, original_options, BUF_LEN);
        strncat(option_buffer, test_args[i].input_dynamorio_options, BUF_LEN);
        my_setenv("DYNAMORIO_OPTIONS", option_buffer);
        print("Set DYNAMORIO_OPTIONS: %s\n", option_buffer);

        dr_app_setup();
        dr_app_start();
        dr_app_stop_and_cleanup();

        if (last_argv[0] == '\0') {
            print("ERROR: last_argv not set by dr_client_main\n");
            failed = 1;
            continue;
        }
        const char *want_argv = test_args[i].want_argv;
        if (strncmp(last_argv, want_argv, sizeof(last_argv)) != 0) {
            print("ERROR: last_argv doesn't match want_argv: "
                  "got |%s|, want |%s|",
                  last_argv, want_argv);
            failed = 1;
            continue;
        }
        print("Found the appropriate argv\n");
        memset(last_argv, 0, sizeof last_argv);
    }

    print("all done\n");
    return failed;
}
