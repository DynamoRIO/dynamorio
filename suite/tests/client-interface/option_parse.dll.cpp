/* **********************************************************
 * Copyright (c) 2015-2023 Google, Inc.  All rights reserved.
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

namespace {

using ::dynamorio::droption::bytesize_t;
using ::dynamorio::droption::dr_parse_options;
using ::dynamorio::droption::DROPTION_FLAG_ACCUMULATE;
using ::dynamorio::droption::DROPTION_FLAG_SWEEP;
using ::dynamorio::droption::droption_parser_t;
using ::dynamorio::droption::DROPTION_SCOPE_ALL;
using ::dynamorio::droption::DROPTION_SCOPE_CLIENT;
using ::dynamorio::droption::DROPTION_SCOPE_FRONTEND;
using ::dynamorio::droption::droption_t;
using ::dynamorio::droption::twostring_t;

static droption_t<long> op_l(DROPTION_SCOPE_CLIENT, "l", 0L, -64, 64, "Some param",
                             "Longer desc of some param.");
static droption_t<long long> op_ll(DROPTION_SCOPE_CLIENT, "ll", 0LL, "Some param",
                                   "Longer desc of some param.");
static droption_t<unsigned long> op_ul(DROPTION_SCOPE_CLIENT, "ul", 0UL, 0, 64,
                                       "Some param", "Longer desc of some param.");
static droption_t<unsigned long long> op_ull(DROPTION_SCOPE_CLIENT, "ull", 0ULL,
                                             "Some param", "Longer desc of some param.");
static droption_t<unsigned int> op_x(DROPTION_SCOPE_CLIENT,
                                     std::vector<std::string>({ "x", "x_alias" }), 0, 0,
                                     64, "Some param", "Longer desc of some param.");
static droption_t<std::string> op_y(DROPTION_SCOPE_CLIENT, "y", DROPTION_FLAG_ACCUMULATE,
                                    "<default>", "Another param",
                                    "Longer desc of another param.");
static droption_t<std::string> op_z(DROPTION_SCOPE_CLIENT,
                                    std::vector<std::string>({ "z", "z_alias" }), "",
                                    "Yet another param",
                                    "Longer desc of yet another param.");
static droption_t<int> op_foo(DROPTION_SCOPE_CLIENT, "foo", 8, "Missing param",
                              "Longer desc of missing param.");
static droption_t<std::string> op_bar(DROPTION_SCOPE_CLIENT, "bar",
                                      "some string with spaces", "Missing string param",
                                      "Longer desc of missing string param.");
static droption_t<bool> op_flag(DROPTION_SCOPE_CLIENT,
                                { "flag", "flag_alias1", "flag_alias2" }, true,
                                "Bool param", "Longer desc of bool param.");
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
static droption_t<int> op_oi(DROPTION_SCOPE_CLIENT, "oi", 0, "Some param",
                             "Longer desc of some param.");
static droption_t<long> op_ol(DROPTION_SCOPE_CLIENT, "ol", 0L, "Some param",
                              "Longer desc of some param.");
static droption_t<long long> op_oll(DROPTION_SCOPE_CLIENT, "oll", 0LL, "Some param",
                                    "Longer desc of some param.");
static droption_t<unsigned int> op_ou(DROPTION_SCOPE_CLIENT, "ou", 0, "Some param",
                                      "Longer desc of some param.");
static droption_t<unsigned long> op_oul(DROPTION_SCOPE_CLIENT, "oul", 0UL, "Some param",
                                        "Longer desc of some param.");
static droption_t<unsigned long long> op_oull(DROPTION_SCOPE_CLIENT, "oull", 0ULL,
                                              "Some param", "Longer desc of some param.");
static droption_t<int> op_xi(DROPTION_SCOPE_CLIENT, "xi", 0, "Some param",
                             "Longer desc of some param.");
static droption_t<long> op_xl(DROPTION_SCOPE_CLIENT, "xl", 0L, "Some param",
                              "Longer desc of some param.");
static droption_t<long long> op_xll(DROPTION_SCOPE_CLIENT, "xll", 0LL, "Some param",
                                    "Longer desc of some param.");
static droption_t<unsigned int> op_xu(DROPTION_SCOPE_CLIENT, "xu", 0, "Some param",
                                      "Longer desc of some param.");
static droption_t<unsigned long> op_xul(DROPTION_SCOPE_CLIENT, "xul", 0UL, "Some param",
                                        "Longer desc of some param.");
static droption_t<unsigned long long> op_xull(DROPTION_SCOPE_CLIENT, "xull", 0ULL,
                                              "Some param", "Longer desc of some param.");

static void
test_argv(int argc, const char *argv[])
{
    ASSERT(argc == 70);

    int i = 1;
    ASSERT(strcmp(argv[i++], "-l") == 0);
    ASSERT(strcmp(argv[i++], "-4") == 0);
    ASSERT(strcmp(argv[i++], "-ll") == 0);
    ASSERT(strcmp(argv[i++], "-3220721071790640321") == 0);
    ASSERT(strcmp(argv[i++], "-ul") == 0);
    ASSERT(strcmp(argv[i++], "4") == 0);
    ASSERT(strcmp(argv[i++], "-ull") == 0);
    ASSERT(strcmp(argv[i++], "1384772493926445887") == 0);
    ASSERT(strcmp(argv[i++], "-x") == 0);
    ASSERT(strcmp(argv[i++], "3") == 0);
    ASSERT(strcmp(argv[i++], "-x_alias") == 0);
    ASSERT(strcmp(argv[i++], "4") == 0);
    ASSERT(strcmp(argv[i++], "-y") == 0);
    ASSERT(strcmp(argv[i++], "quoted string") == 0);
    ASSERT(strcmp(argv[i++], "-z") == 0);
    ASSERT(strcmp(argv[i++], "first") == 0);
    ASSERT(strcmp(argv[i++], "-z_alias") == 0);
    ASSERT(strcmp(argv[i++], "single quotes -dash --dashes") == 0);
    ASSERT(strcmp(argv[i++], "-front") == 0);
    ASSERT(strcmp(argv[i++], "value") == 0);
    ASSERT(strcmp(argv[i++], "-y") == 0);
    ASSERT(strcmp(argv[i++], "accum") == 0);
    ASSERT(strcmp(argv[i++], "-front2") == 0);
    ASSERT(strcmp(argv[i++], "value2") == 0);
    ASSERT(strcmp(argv[i++], "-flag") == 0);
    ASSERT(strcmp(argv[i++], "-flag_alias1") == 0);
    ASSERT(strcmp(argv[i++], "-no_flag_alias2") == 0);
    ASSERT(strcmp(argv[i++], "-takes2") == 0);
    ASSERT(strcmp(argv[i++], "1_of_4") == 0);
    ASSERT(strcmp(argv[i++], "2_of_4") == 0);
    ASSERT(strcmp(argv[i++], "-takes2") == 0);
    ASSERT(strcmp(argv[i++], "3_of_4") == 0);
    ASSERT(strcmp(argv[i++], "4_of_4") == 0);
    ASSERT(strcmp(argv[i++], "-val_sep") == 0);
    ASSERT(strcmp(argv[i++], "v1.1 v1.2") == 0);
    ASSERT(strcmp(argv[i++], "-val_sep") == 0);
    ASSERT(strcmp(argv[i++], "v2.1 v2.2") == 0);
    ASSERT(strcmp(argv[i++], "-val_sep2") == 0);
    ASSERT(strcmp(argv[i++], "v1") == 0);
    ASSERT(strcmp(argv[i++], "v2") == 0);
    ASSERT(strcmp(argv[i++], "-val_sep2") == 0);
    ASSERT(strcmp(argv[i++], "v3") == 0);
    ASSERT(strcmp(argv[i++], "v4") == 0);
    ASSERT(strcmp(argv[i++], "-large_bytesize") == 0);
    ASSERT(strcmp(argv[i++], "9999999999") == 0);
    ASSERT(strcmp(argv[i++], "-oi") == 0);
    ASSERT(strcmp(argv[i++], "-012") == 0);
    ASSERT(strcmp(argv[i++], "-ol") == 0);
    ASSERT(strcmp(argv[i++], "-012") == 0);
    ASSERT(strcmp(argv[i++], "-oll") == 0);
    ASSERT(strcmp(argv[i++], "-012") == 0);
    ASSERT(strcmp(argv[i++], "-ou") == 0);
    ASSERT(strcmp(argv[i++], "012") == 0);
    ASSERT(strcmp(argv[i++], "-oul") == 0);
    ASSERT(strcmp(argv[i++], "012") == 0);
    ASSERT(strcmp(argv[i++], "-oull") == 0);
    ASSERT(strcmp(argv[i++], "012") == 0);
    ASSERT(strcmp(argv[i++], "-xi") == 0);
    ASSERT(strcmp(argv[i++], "-0xa") == 0);
    ASSERT(strcmp(argv[i++], "-xl") == 0);
    ASSERT(strcmp(argv[i++], "-0xa") == 0);
    ASSERT(strcmp(argv[i++], "-xll") == 0);
    ASSERT(strcmp(argv[i++], "-0xa") == 0);
    ASSERT(strcmp(argv[i++], "-xu") == 0);
    ASSERT(strcmp(argv[i++], "0xa") == 0);
    ASSERT(strcmp(argv[i++], "-xul") == 0);
    ASSERT(strcmp(argv[i++], "0xa") == 0);
    ASSERT(strcmp(argv[i++], "-xull") == 0);
    ASSERT(strcmp(argv[i++], "0xa") == 0);
}

} // namespace

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

    // Test get_value_separator().
    ASSERT(op_y.get_value_separator() == std::string(" "));
    ASSERT(op_val_sep.get_value_separator() == std::string("+"));
    ASSERT(op_val_sep2.get_value_separator() == std::string("+"));

    // Test parsing octal format numeric strings.
    dr_fprintf(STDERR, "param oi = %d\n", op_oi.get_value());
    dr_fprintf(STDERR, "param ol = %ld\n", op_ol.get_value());
    dr_fprintf(STDERR, "param oll = %lld\n", op_oll.get_value());
    dr_fprintf(STDERR, "param ou = %u\n", op_ou.get_value());
    dr_fprintf(STDERR, "param oul = %lu\n", op_oul.get_value());
    dr_fprintf(STDERR, "param oull = %llu\n", op_oull.get_value());

    // Test parsing hexadecimal format numeric strings.
    dr_fprintf(STDERR, "param xi = %d\n", op_xi.get_value());
    dr_fprintf(STDERR, "param xl = %ld\n", op_xl.get_value());
    dr_fprintf(STDERR, "param xll = %lld\n", op_xll.get_value());
    dr_fprintf(STDERR, "param xu = %u\n", op_xu.get_value());
    dr_fprintf(STDERR, "param xul = %lu\n", op_xul.get_value());
    dr_fprintf(STDERR, "param xull = %llu\n", op_xull.get_value());

    // Test parsing a string of negative digits. These tests are used to check whether the
    // unsigned option can recognize negative input and output an error.
    std::vector<droption_parser_t *> negtive_test_options;
    negtive_test_options.push_back(static_cast<droption_parser_t *>(&op_ou));
    negtive_test_options.push_back(static_cast<droption_parser_t *>(&op_oul));
    negtive_test_options.push_back(static_cast<droption_parser_t *>(&op_oull));
    const char *negative_test_args[3] = { "", "", "   -1" };
    std::string parse_err;
    for (auto op : negtive_test_options) {
        std::string op_name = "-" + op->get_name();
        negative_test_args[1] = op_name.c_str();
        ok = droption_parser_t::parse_argv(DROPTION_SCOPE_CLIENT, 3, negative_test_args,
                                           &parse_err, NULL);
        ASSERT(!ok);
        dr_fprintf(STDERR, "%s\n", parse_err.c_str());
    }
}
