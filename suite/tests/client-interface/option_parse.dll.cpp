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

static droption_t<long> op_l(DROPTION_SCOPE_CLIENT, "l", 0L, -64, 64, "Some param",
                             "Longer desc of some param.");
static droption_t<long long> op_ll(DROPTION_SCOPE_CLIENT, "ll", 0LL, "Some param",
                                   "Longer desc of some param.");
static droption_t<unsigned long> op_ul(DROPTION_SCOPE_CLIENT, "ul", 0UL, 0, 64,
                                       "Some param", "Longer desc of some param.");
static droption_t<unsigned long long> op_ull(DROPTION_SCOPE_CLIENT, "ull", 0ULL,
                                             "Some param", "Longer desc of some param.");
static droption_t<unsigned int> op_x(DROPTION_SCOPE_CLIENT, "x", 0, 0, 64, "Some param",
                                     "Longer desc of some param.");
static droption_t<std::string> op_y(DROPTION_SCOPE_CLIENT, "y", DROPTION_FLAG_ACCUMULATE,
                                    "<default>", "Another param",
                                    "Longer desc of another param.");
static droption_t<std::string> op_z(DROPTION_SCOPE_CLIENT, "z", "", "Yet another param",
                                    "Longer desc of yet another param.");
static droption_t<int> op_foo(DROPTION_SCOPE_CLIENT, "foo", 8, "Missing param",
                              "Longer desc of missing param.");
static droption_t<std::string> op_bar(DROPTION_SCOPE_CLIENT, "bar",
                                      "some string with spaces", "Missing string param",
                                      "Longer desc of missing string param.");
static droption_t<bool> op_flag(DROPTION_SCOPE_CLIENT, "flag", true, "Bool param",
                                "Longer desc of bool param.");
static droption_t<std::string> op_sweep(DROPTION_SCOPE_CLIENT, "sweep",
                                        DROPTION_FLAG_SWEEP | DROPTION_FLAG_ACCUMULATE,
                                        "", "All the unknown params",
                                        "Longer desc of unknown param accum.");
static droption_t<std::string> op_front(DROPTION_SCOPE_FRONTEND, "front", "",
                                        "Front-end param",
                                        "Longer desc of front-end param.");
static droption_t<std::string> op_front2(DROPTION_SCOPE_FRONTEND, "front2", "",
                                         "Front-end param2",
                                         "Longer desc of front-end param2.");
static droption_t<twostring_t> op_takes2(DROPTION_SCOPE_CLIENT, "takes2",
                                         DROPTION_FLAG_ACCUMULATE, twostring_t("", ""),
                                         "Param that takes 2",
                                         "Longer desc of param that takes 2.");
static droption_t<std::string>
    op_val_sep(DROPTION_SCOPE_CLIENT, "val_sep", DROPTION_FLAG_ACCUMULATE, "+",
               std::string(""), "Param that uses customized separator \"+\"",
               "Longer desc of that uses customized separator \"+\"");
static droption_t<twostring_t>
    op_val_sep2(DROPTION_SCOPE_CLIENT, "val_sep2", DROPTION_FLAG_ACCUMULATE, "+",
                twostring_t("", ""),
                "Param that takes 2 and uses customized separator \"+\"",
                "Longer desc of param that takes 2 and uses customized separator \"+\"");
static droption_t<bytesize_t>
    op_large_bytesize(DROPTION_SCOPE_CLIENT, "large_bytesize", DROPTION_FLAG_ACCUMULATE,
                      0, "Param that takes in a large bytesize value",
                      "Longer desc of param that takes in a large bytesize value");

static void
test_argv(int argc, const char *argv[])
{
    ASSERT(argc == 42);

    ASSERT(strcmp(argv[1], "-l") == 0);
    ASSERT(strcmp(argv[2], "-4") == 0);
    ASSERT(strcmp(argv[3], "-ll") == 0);
    ASSERT(strcmp(argv[4], "-3220721071790640321") == 0);
    ASSERT(strcmp(argv[5], "-ul") == 0);
    ASSERT(strcmp(argv[6], "4") == 0);
    ASSERT(strcmp(argv[7], "-ull") == 0);
    ASSERT(strcmp(argv[8], "1384772493926445887") == 0);
    ASSERT(strcmp(argv[9], "-x") == 0);
    ASSERT(strcmp(argv[10], "4") == 0);
    ASSERT(strcmp(argv[11], "-y") == 0);
    ASSERT(strcmp(argv[12], "quoted string") == 0);
    ASSERT(strcmp(argv[13], "-z") == 0);
    ASSERT(strcmp(argv[14], "first") == 0);
    ASSERT(strcmp(argv[15], "-z") == 0);
    ASSERT(strcmp(argv[16], "single quotes -dash --dashes") == 0);
    ASSERT(strcmp(argv[17], "-front") == 0);
    ASSERT(strcmp(argv[18], "value") == 0);
    ASSERT(strcmp(argv[19], "-y") == 0);
    ASSERT(strcmp(argv[20], "accum") == 0);
    ASSERT(strcmp(argv[21], "-front2") == 0);
    ASSERT(strcmp(argv[22], "value2") == 0);
    ASSERT(strcmp(argv[23], "-no_flag") == 0);
    ASSERT(strcmp(argv[24], "-takes2") == 0);
    ASSERT(strcmp(argv[25], "1_of_4") == 0);
    ASSERT(strcmp(argv[26], "2_of_4") == 0);
    ASSERT(strcmp(argv[27], "-takes2") == 0);
    ASSERT(strcmp(argv[28], "3_of_4") == 0);
    ASSERT(strcmp(argv[29], "4_of_4") == 0);
    ASSERT(strcmp(argv[30], "-val_sep") == 0);
    ASSERT(strcmp(argv[31], "v1.1 v1.2") == 0);
    ASSERT(strcmp(argv[32], "-val_sep") == 0);
    ASSERT(strcmp(argv[33], "v2.1 v2.2") == 0);
    ASSERT(strcmp(argv[34], "-val_sep2") == 0);
    ASSERT(strcmp(argv[35], "v1") == 0);
    ASSERT(strcmp(argv[36], "v2") == 0);
    ASSERT(strcmp(argv[37], "-val_sep2") == 0);
    ASSERT(strcmp(argv[38], "v3") == 0);
    ASSERT(strcmp(argv[39], "v4") == 0);
    ASSERT(strcmp(argv[40], "-large_bytesize") == 0);
    ASSERT(strcmp(argv[41], "9999999999") == 0);
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
    ASSERT(op_l.specified());
    ASSERT(op_ll.specified());
    ASSERT(op_ul.specified());
    ASSERT(op_ull.specified());
    ASSERT(op_x.specified());
    ASSERT(op_y.specified());
    ASSERT(op_z.specified());
    dr_fprintf(STDERR, "param l = %ld\n", op_l.get_value());
    dr_fprintf(STDERR, "param ll = %lld\n", op_ll.get_value());
    dr_fprintf(STDERR, "param ul = %lu\n", op_ul.get_value());
    dr_fprintf(STDERR, "param ull = %llu\n", op_ull.get_value());
    dr_fprintf(STDERR, "param x = %d\n", op_x.get_value());
    dr_fprintf(STDERR, "param y = |%s|\n", op_y.get_value().c_str());
    dr_fprintf(STDERR, "param z = |%s|\n", op_z.get_value().c_str());
    dr_fprintf(STDERR, "param foo = %d\n", op_foo.get_value());
    dr_fprintf(STDERR, "param bar = |%s|\n", op_bar.get_value().c_str());
    dr_fprintf(STDERR, "param flag = |%d|\n", op_flag.get_value());
    dr_fprintf(STDERR, "param sweep = |%s|\n", op_sweep.get_value().c_str());
    dr_fprintf(STDERR, "param takes2 = |%s|,|%s|\n", op_takes2.get_value().first.c_str(),
               op_takes2.get_value().second.c_str());
    dr_fprintf(STDERR, "param val_sep = |%s|\n", op_val_sep.get_value().c_str());
    dr_fprintf(STDERR, "param val_sep2 = |%s|,|%s|\n",
               op_val_sep2.get_value().first.c_str(),
               op_val_sep2.get_value().second.c_str());
    dr_fprintf(STDERR, "param large_bytesize = %lld\n", op_large_bytesize.get_value());
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

    // Test get_value_separator()
    ASSERT(op_y.get_value_separator() == std::string(" "));
    ASSERT(op_val_sep.get_value_separator() == std::string("+"));
    ASSERT(op_val_sep2.get_value_separator() == std::string("+"));
}
