/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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
(DROPTION_SCOPE_CLIENT, "y", DROPTION_FLAG_ACCUMULATE, "<default>", "Another param",
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
static droption_t<std::string> op_sweep
(DROPTION_SCOPE_CLIENT, "sweep", DROPTION_FLAG_SWEEP | DROPTION_FLAG_ACCUMULATE,
 "", "All the unknown params",
 "Longer desc of unknown param accum.");
static droption_t<std::string> op_front
(DROPTION_SCOPE_FRONTEND, "front", "", "Front-end param",
 "Longer desc of front-end param.");
static droption_t<std::string> op_front2
(DROPTION_SCOPE_FRONTEND, "front2", "", "Front-end param2",
 "Longer desc of front-end param2.");
static droption_t<twostring_t> op_takes2
(DROPTION_SCOPE_CLIENT, "takes2", DROPTION_FLAG_ACCUMULATE,
 twostring_t("",""), "Param that takes 2",
 "Longer desc of param that takes 2.");

static void
test_argv(int argc, const char *argv[])
{
    ASSERT(argc == 22);
    ASSERT(strcmp(argv[1], "-x") == 0);
    ASSERT(strcmp(argv[2], "4") == 0);
    ASSERT(strcmp(argv[3], "-y") == 0);
    ASSERT(strcmp(argv[4], "quoted string") == 0);
    ASSERT(strcmp(argv[5], "-z") == 0);
    ASSERT(strcmp(argv[6], "first") == 0);
    ASSERT(strcmp(argv[7], "-z") == 0);
    ASSERT(strcmp(argv[8], "single quotes -dash --dashes") == 0);
    ASSERT(strcmp(argv[9], "-front") == 0);
    ASSERT(strcmp(argv[10], "value") == 0);
    ASSERT(strcmp(argv[11], "-y") == 0);
    ASSERT(strcmp(argv[12], "accum") == 0);
    ASSERT(strcmp(argv[13], "-front2") == 0);
    ASSERT(strcmp(argv[14], "value2") == 0);
    ASSERT(strcmp(argv[15], "-no_flag") == 0);
    ASSERT(strcmp(argv[16], "-takes2") == 0);
    ASSERT(strcmp(argv[17], "1_of_4") == 0);
    ASSERT(strcmp(argv[18], "2_of_4") == 0);
    ASSERT(strcmp(argv[19], "-takes2") == 0);
    ASSERT(strcmp(argv[20], "3_of_4") == 0);
    ASSERT(strcmp(argv[21], "4_of_4") == 0);
}

DR_EXPORT void
dr_client_main(client_id_t client_id, int argc, const char *argv[])
{
    test_argv(argc, argv);

    // Test dr_get_option_array().
    int ask_argc;
    const char **ask_argv;
    bool ok = dr_get_option_array(client_id, &ask_argc, &ask_argv);
    ASSERT(ok);
    test_argv(ask_argc, ask_argv);

    // Test droption parsing and droption_t declarations above.
    ok = droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, argc, argv, NULL, NULL);
    ASSERT(ok);
    ASSERT(op_x.specified());
    ASSERT(op_y.specified());
    ASSERT(op_z.specified());
    dr_fprintf(STDERR, "param x = %d\n", op_x.get_value());
    dr_fprintf(STDERR, "param y = |%s|\n", op_y.get_value().c_str());
    dr_fprintf(STDERR, "param z = |%s|\n", op_z.get_value().c_str());
    dr_fprintf(STDERR, "param foo = %d\n", op_foo.get_value());
    dr_fprintf(STDERR, "param bar = |%s|\n", op_bar.get_value().c_str());
    dr_fprintf(STDERR, "param flag = |%d|\n", op_flag.get_value());
    dr_fprintf(STDERR, "param sweep = |%s|\n", op_sweep.get_value().c_str());
    dr_fprintf(STDERR, "param takes2 = |%s|,|%s|\n",
               op_takes2.get_value().first.c_str(), op_takes2.get_value().second.c_str());
    ASSERT(!op_foo.specified());
    ASSERT(!op_bar.specified());

    // Test set_value.
    unsigned int old_x = op_x.get_value();
    op_x.set_value(old_x + 3);
    ASSERT(op_x.get_value() == old_x + 3);

    // Minimal sanity check that dr_parse_options() works, but 2nd parsing is
    // not really supported by droption.
    ok = dr_parse_options(client_id, NULL, NULL);
    ASSERT(ok);
}
