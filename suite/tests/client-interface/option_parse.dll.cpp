/* **********************************************************
 * Copyright (c) 2015 Google, Inc.  All rights reserved.
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

/* Test parsing options.
 */

#include "dr_api.h"
#include "client_tools.h"
#include "droption.h"
#include <string.h>

static droption_t<unsigned int> op_x
(DROPTION_SCOPE_CLIENT, "x", 0, 0, 64, "Some param",
 "Longer desc of some param.");
static droption_t<std::string> op_y
(DROPTION_SCOPE_CLIENT, DROPTION_FLAG_ACCUMULATE, "y", "<default>", "Another param",
 "Longer desc of another param.");
static droption_t<std::string> op_z
(DROPTION_SCOPE_CLIENT, "z", "", "Yet another param",
 "Longer desc of yet another param.");
static droption_t<int> op_foo
(DROPTION_SCOPE_CLIENT, "foo", 8, "Missing param",
 "Longer desc of missing param.");
static droption_t<std::string> op_bar
(DROPTION_SCOPE_CLIENT, "bar", "some string with spaces", "Missing string param",
 "Longer desc of missing string param.");
static droption_t<bool> op_flag
(DROPTION_SCOPE_CLIENT, "flag", true, "Bool param",
 "Longer desc of bool param.");

DR_EXPORT void
dr_init(client_id_t client_id)
{
    // Test dr_get_option_array().
    int argc;
    const char **argv;
    bool ok = dr_get_option_array(client_id, &argc, &argv, MAXIMUM_PATH);
    ASSERT(ok);
    ASSERT(argc == 12);
    ASSERT(strcmp(argv[1], "-x") == 0);
    ASSERT(strcmp(argv[2], "4") == 0);
    ASSERT(strcmp(argv[3], "-y") == 0);
    ASSERT(strcmp(argv[4], "quoted string") == 0);
    ASSERT(strcmp(argv[5], "-z") == 0);
    ASSERT(strcmp(argv[6], "first") == 0);
    ASSERT(strcmp(argv[7], "-z") == 0);
    ASSERT(strcmp(argv[8], "single quotes -dash --dashes") == 0);
    ASSERT(strcmp(argv[9], "-y") == 0);
    ASSERT(strcmp(argv[10], "accum") == 0);
    ASSERT(strcmp(argv[11], "-no_flag") == 0);
    ok = dr_free_option_array(argc, argv);
    ASSERT(ok);

    // Test dr_parse_options() and droption_t declarations above.
    ok = dr_parse_options(client_id, NULL);
    ASSERT(ok);
    ASSERT(op_x.specified());
    ASSERT(op_y.specified());
    ASSERT(op_z.specified());
    dr_printf("param x = %d\n", op_x.get_value());
    dr_printf("param y = |%s|\n", op_y.get_value().c_str());
    dr_printf("param z = |%s|\n", op_z.get_value().c_str());
    dr_printf("param foo = %d\n", op_foo.get_value());
    dr_printf("param bar = |%s|\n", op_bar.get_value().c_str());
    dr_printf("param flag = |%d|\n", op_flag.get_value());
    ASSERT(!op_foo.specified());
    ASSERT(!op_bar.specified());
}
