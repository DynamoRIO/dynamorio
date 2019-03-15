/* *******************************************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * *******************************************************************************/

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

/*
 * memquery_test.h - unit test for unix memquery logic, it is meant to be
 * included into memquery.c after its includes.
 *
 * See also memquery_test_data.h
 */

#define MEMQUERY_ITERATOR_START memquery_iterator_start_test
#define MEMQUERY_ITERATOR_NEXT memquery_iterator_next_test
#define MEMQUERY_ITERATOR_STOP memquery_iterator_stop_test
#define MODULE_IS_HEADER module_is_header_test
#define MODULE_WALK_PROGRAM_HEADERS module_walk_program_headers_test

typedef struct {
    memquery_iter_t iter_result;
    bool is_header;
    app_pc mod_base, mod_end;
} fake_memquery_result;

typedef struct {
    fake_memquery_result *iters;
    size_t iters_count;

    const char *in_name;
    app_pc in_start;

    int want_return;
    app_pc want_start;
    app_pc want_end;
    const char *want_fulldir;
    const char *want_filename;
} memquery_library_bounds_test;

#include "memquery_test_data.h"

memquery_library_bounds_test cur_bounds;
int next_iter_position = -1;
bool enable_memquery_unit_test;

/* CHECK_EQ verifies that the provded x and y are equal; if not, it prints a
 * message to STDERR and returns false.
 */
#define CHECK_EQ(x, y, fmt)                                                  \
    if ((x) != (y)) {                                                        \
        print_file(STDERR, #x " != " #y ": " fmt " != " fmt "\n", (x), (y)); \
        return false;                                                        \
    }

/* Piggyback on CHECK_EQ for the failure message: since the strncmp
 * showed the strings are not equal, we know that the x and y pointers are also
 * not equal according to ==.
 */
#define ASSERT_STR_EQ(x, y, sz)         \
    if (strncmp((x), (y), (sz)) != 0) { \
        CHECK_EQ(x, y, "%s");           \
    }

bool
run_single_memquery_test(memquery_library_bounds_test test)
{
    cur_bounds = test;

    char fulldir[200];
    char filename[200];

    app_pc start = test.in_start;
    app_pc end;

    int got_return = memquery_library_bounds_by_iterator(
        test.in_name, &start, &end, test.want_fulldir ? fulldir : NULL,
        BUFFER_SIZE_ELEMENTS(fulldir), test.want_filename ? filename : NULL,
        BUFFER_SIZE_ELEMENTS(filename));

    CHECK_EQ(start, test.want_start, PFX);
    CHECK_EQ(end, test.want_end, PFX);
    CHECK_EQ(got_return, test.want_return, "%d");
    if (test.want_fulldir) {
        ASSERT_STR_EQ(fulldir, test.want_fulldir, BUFFER_SIZE_ELEMENTS(fulldir));
    }
    if (test.want_filename) {
        ASSERT_STR_EQ(filename, test.want_filename, BUFFER_SIZE_ELEMENTS(filename));
    }
    return true;
}

void
unit_test_memquery()
{
    bool all_passed = true;
    print_file(STDERR, "START unit_test_memquery\n");
    enable_memquery_unit_test = true;
    for (int i = 0; i < NUM_MEMQUERY_TESTS; i++) {
        const char *test_name = all_memquery_test_names[i];
        print_file(STDERR, "Run memquery unit test %s...\n", test_name);
        const char *status = "OK";
        if (!run_single_memquery_test(*all_memquery_tests[i])) {
            status = "FAILED";
            all_passed = false;
        }
        print_file(STDERR, "**** memquery unit test %s %s\n", test_name, status);
    }
    enable_memquery_unit_test = false;
    if (!all_passed) {
        print_file(STDERR, "unit_test_memquery had failing test(s)!\n");
        os_terminate(NULL, TERMINATE_PROCESS);
    }
    print_file(STDERR, "END unit_test_memquery\n");
}

bool
memquery_iterator_start_test(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    if (!enable_memquery_unit_test) {
        return memquery_iterator_start(iter, start, may_alloc);
    }
    next_iter_position = 0;
    return true;
}

bool
memquery_iterator_next_test(memquery_iter_t *iter)
{
    if (!enable_memquery_unit_test) {
        return memquery_iterator_next(iter);
    }
    if (next_iter_position >= cur_bounds.iters_count) {
        return false;
    }
    *iter = cur_bounds.iters[next_iter_position++].iter_result;
    return true;
}

void
memquery_iterator_stop_test(memquery_iter_t *iter)
{
    if (!enable_memquery_unit_test) {
        return memquery_iterator_stop(iter);
    }
}

bool
module_is_header_test(app_pc base, size_t size)
{
    if (!enable_memquery_unit_test) {
        return module_is_header(base, size);
    }
    if (cur_bounds.iters_count == 0) {
        print_file(STDERR, "module_is_header_test called without setting cur_bounds!");
        *(int *)(42) = 43;
    }
    for (int i = 0; i < cur_bounds.iters_count; i++) {
        fake_memquery_result *cur = &cur_bounds.iters[i];
        memquery_iter_t iter = cur->iter_result;
        if (iter.vm_start <= base && base < iter.vm_end) {
            return cur->is_header;
        }
    }
    print_file(STDERR, "UNKNOWN BASE PC " PFX "\n", base);
    return false;
}

bool
module_walk_program_headers_test(app_pc base, size_t view_size, bool at_map,
                                 bool dyn_reloc, OUT app_pc *out_base /* relative pc */,
                                 OUT app_pc *out_first_end /* relative pc */,
                                 OUT app_pc *out_max_end /* relative pc */,
                                 OUT char **out_soname, OUT os_module_data_t *out_data)
{
    if (!enable_memquery_unit_test) {
        return module_walk_program_headers(
            base, view_size, at_map, dyn_reloc, out_base /* relative pc */,
            out_first_end /* relative pc */, out_max_end /* relative pc */, out_soname,
            out_data);
    }
    if (cur_bounds.iters_count == 0) {
        ASSERT(false && "module_is_header_test called without setting cur_bounds!");
    }
    if (out_first_end != NULL || out_soname != NULL || out_data != NULL ||
        out_base == NULL || out_max_end == NULL) {
        ASSERT(false &&
               "out_data, out_first_end, and out_soname must be NULL, and out_base and "
               "out_max_end must not be!");
    }
    for (int i = 0; i < cur_bounds.iters_count; i++) {
        fake_memquery_result *cur = &cur_bounds.iters[i];
        memquery_iter_t iter = cur->iter_result;
        if (iter.vm_start <= base && base < iter.vm_end) {
            if (cur->is_header) {
                *out_base = cur->mod_base;
                *out_max_end = cur->mod_end;
                return true;
            }
        }
    }
    ASSERT(false && "UNKNOWN BASE PC");
    return false;
}

/* record_memquery_library_bounds_by_iterator wraps
 * memquery_library_bounds_by_iterator, printing out test case data for the
 * memquery unit test. See memquery_test_data.h for a selection of tests.
 */
int
record_memquery_library_bounds_by_iterator(const char *name, app_pc *start /*IN/OUT*/,
                                           app_pc *end /*OUT*/,
                                           char *fulldir /*OPTIONAL OUT*/,
                                           size_t fulldir_size,
                                           char *filename /*OPTIONAL OUT*/,
                                           size_t filename_size)
{
    memquery_iter_t iter;

    memquery_iterator_start(&iter, NULL, dynamo_heap_initialized);

    print_file(STDERR, "\nfake_memquery_result results_TODORENAME[100] = {");
    int iters_count = 0;
    while (memquery_iterator_next(&iter)) {
        iters_count++;
        print_file(STDERR, "{\n");
        print_file(STDERR,
                   ".iter_result = {"
                   " .vm_start = (app_pc)" PFX ","
                   " .vm_end = (app_pc)" PFX ","
                   " .prot = %X,"
                   " .comment = \"%s\" },",
                   iter.vm_start, iter.vm_end, iter.prot, iter.comment);

        app_pc mod_base = 0, mod_end = 0;
        bool is_header = false;

        if (TEST(MEMPROT_READ, iter.prot)
            /* We have observed segfaults reading data from very high addresses,
             * even though their mappings are listed as readable.
             */
            && iter.vm_start < (app_pc)0xffffffff00000000 &&
            module_is_header(iter.vm_start, iter.vm_end - iter.vm_start)) {

            is_header = true;
            if (!module_walk_program_headers(
                    iter.vm_start, iter.vm_end - iter.vm_start, false,
                    /*i#1589: ld.so relocated .dynamic*/
                    true, &mod_base, NULL, &mod_end, NULL, NULL)) {
                ASSERT_NOT_REACHED();
            }
        }
        print_file(STDERR,
                   ".is_header = %s, .mod_base = (app_pc)" PFX ", .mod_end = (app_pc)" PFX
                   ", \n},\n",
                   is_header ? "true" : "false", mod_base, mod_end);
    }

    print_file(STDERR, "\n};\n");
    memquery_iterator_stop(&iter);

    print_file(STDERR,
               "memquery_library_bounds_test test_TODORENAME = {\n"
               " .iters = results_TODORENAME,\n"
               " .iters_count = %d,\n"
               " .in_start = (app_pc)" PFX ",\n",
               iters_count, *start);
    if (name) {
        print_file(STDERR, " .in_name = \"%s\",\n", name);
    }
    int ret = memquery_library_bounds_by_iterator(name, start, end, fulldir, fulldir_size,
                                                  filename, filename_size);
    print_file(STDERR,
               " .want_return = %d,\n"
               " .want_start = (app_pc)" PFX ",\n"
               " .want_end = (app_pc)" PFX ",\n",
               ret, *start, *end);
    if (fulldir) {
        print_file(STDERR, " .want_fulldir = \"%s\",\n", fulldir);
    }
    if (filename) {
        print_file(STDERR, " .want_filename = \"%s\",\n", filename);
    }
    print_file(STDERR, "};\n");
    return ret;
}
