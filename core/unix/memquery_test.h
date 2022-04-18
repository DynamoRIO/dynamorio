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

#ifdef STANDALONE_UNIT_TEST

typedef struct {
    memquery_iter_t iter_result;
    bool is_header;
    app_pc mod_base, mod_end;
} fake_memquery_result;

typedef struct {
    const char *test_name;
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

#    include "memquery_test_data.h"

/* The fake iterator state during the unit_test_memquery is held in these two
 * variables; we assume single-threaded usage.
 */
static const memquery_library_bounds_test *cur_bounds;
static int next_iter_position = -1;

static bool
run_single_memquery_test(const memquery_library_bounds_test *test);

/* unit_test_memquery executes a standalone unit test on
 * memquery_library_bounds_by_iterator; it is not static because it is needed in
 * unit_tests.c
 */
void
unit_test_memquery()
{
#    ifndef X64
    /* Instead of putting effort into generating test cases which work under both
     * 32/64 bit, just skip the test when not running 64 bits.
     */
    return;
#    endif
    bool all_passed = true;
    print_file(STDERR, "START unit_test_memquery\n");
    for (int i = 0; i < NUM_MEMQUERY_TESTS; i++) {
        memquery_library_bounds_test *test = &all_memquery_tests[i];
        print_file(STDERR, "Run memquery unit test %s...\n", test->test_name);
        const char *status = "OK";
        if (!run_single_memquery_test(test)) {
            status = "FAILED";
            all_passed = false;
        }
        print_file(STDERR, "**** memquery unit test %s %s\n", test->test_name, status);
    }
    EXPECT(all_passed, true);
    print_file(STDERR, "END unit_test_memquery\n");
}

static bool
memquery_iterator_start_test(memquery_iter_t *iter, app_pc start, bool may_alloc)
{
    ASSERT(cur_bounds);
    next_iter_position = 0;
    return true;
}

static bool
memquery_iterator_next_test(memquery_iter_t *iter)
{
    ASSERT(cur_bounds);
    if (next_iter_position >= cur_bounds->iters_count) {
        return false;
    }
    *iter = cur_bounds->iters[next_iter_position++].iter_result;
    return true;
}

static void
memquery_iterator_stop_test(memquery_iter_t *iter)
{
    ASSERT(cur_bounds);
}

static bool
module_is_header_test(app_pc base, size_t size)
{
    ASSERT(cur_bounds);
    for (int i = 0; i < cur_bounds->iters_count; i++) {
        fake_memquery_result *cur = &cur_bounds->iters[i];
        memquery_iter_t iter = cur->iter_result;
        if (iter.vm_start <= base && base < iter.vm_end) {
            return cur->is_header;
        }
    }
    print_file(STDERR, "UNKNOWN BASE PC " PFX "\n", base);
    return false;
}

static bool
module_walk_program_headers_test(app_pc base, size_t view_size, bool at_map,
                                 bool dyn_reloc, OUT app_pc *out_base /* relative pc */,
                                 OUT app_pc *out_first_end /* relative pc */,
                                 OUT app_pc *out_max_end /* relative pc */,
                                 OUT char **out_soname, OUT os_module_data_t *out_data)
{
    ASSERT(cur_bounds);
    if (out_first_end != NULL || out_soname != NULL || out_data != NULL ||
        out_base == NULL || out_max_end == NULL) {
        ASSERT(false &&
               "out_data, out_first_end, and out_soname must be NULL, and out_base and "
               "out_max_end must not be!");
    }
    for (int i = 0; i < cur_bounds->iters_count; i++) {
        fake_memquery_result *cur = &cur_bounds->iters[i];
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

static const memquery_library_bounds_funcs fake_memquery_library_bounds_funcs = {
    .memquery_iterator_start = memquery_iterator_start_test,
    .memquery_iterator_next = memquery_iterator_next_test,
    .memquery_iterator_stop = memquery_iterator_stop_test,
    .module_is_header = module_is_header_test,
    .module_walk_program_headers = module_walk_program_headers_test,
};

static bool
run_single_memquery_test(const memquery_library_bounds_test *test)
{
    cur_bounds = test;

    char fulldir[MAXIMUM_PATH];
    char filename[MAXIMUM_PATH];

    app_pc start = test->in_start;
    app_pc end;

    int got_return = memquery_library_bounds_by_iterator_internal(
        test->in_name, &start, &end, test->want_fulldir ? fulldir : NULL,
        BUFFER_SIZE_ELEMENTS(fulldir), test->want_filename ? filename : NULL,
        BUFFER_SIZE_ELEMENTS(filename), &fake_memquery_library_bounds_funcs);

    EXPECT(start, test->want_start);
    EXPECT(end, test->want_end);
    EXPECT(got_return, test->want_return);
    if (test->want_fulldir) {
        EXPECT_STR(fulldir, test->want_fulldir, BUFFER_SIZE_ELEMENTS(fulldir));
    }
    if (test->want_filename) {
        EXPECT_STR(filename, test->want_filename, BUFFER_SIZE_ELEMENTS(filename));
    }

    cur_bounds = NULL;
    return true;
}

#endif /* STANDALONE_UNIT_TEST */

/****************************************************************************
 * Test case recording feature for memquery_library_bounds_by_iterator.
 */
#ifdef RECORD_MEMQUERY

#    ifndef RECORD_MEMQUERY_RESULTS_FILE
#        define RECORD_MEMQUERY_RESULTS_FILE "/tmp/memquery_results.txt"
#        define RECORD_MEMQUERY_TESTS_FILE "/tmp/memquery_tests.txt"
#    endif

/* We rename memquery_library_bounds_by_iterator to
 * real_memquery_library_bounds_by_iterator in a define below.
 */
#    define record_memquery_library_bounds_by_iterator memquery_library_bounds_by_iterator

int
real_memquery_library_bounds_by_iterator(const char *name, app_pc *start /*IN/OUT*/,
                                         app_pc *end /*OUT*/,
                                         char *fulldir /*OPTIONAL OUT*/,
                                         size_t fulldir_size,
                                         char *filename /*OPTIONAL OUT*/,
                                         size_t filename_size);

/* record_memquery_library_bounds_by_iterator wraps
 * memquery_library_bounds_by_iterator, printing out test case data for the
 * memquery unit test directly to STDERR. See memquery_test_data.h for a
 * selection of tests.
 *
 * To use this function to generate more test cases, compile with RECORD_MEQUERY
 * defined and run DR for the program you want to gather a test case from; it
 * will write its results to the RECORD_MEMQUERY_RESULTS_FILE and
 * RECORD_MEMQUERY_TESTS_FILE (which have defaults defined above if not already
 * defined during preprocessing).
 *
 * TODO(chowski): include more test cases for more interesting scenarios.
 */
int
record_memquery_library_bounds_by_iterator(const char *name, app_pc *start /*IN/OUT*/,
                                           app_pc *end /*OUT*/,
                                           char *fulldir /*OPTIONAL OUT*/,
                                           size_t fulldir_size,
                                           char *filename /*OPTIONAL OUT*/,
                                           size_t filename_size)
{
    int results_fd =
        os_open(RECORD_MEMQUERY_RESULTS_FILE, OS_OPEN_WRITE | OS_OPEN_APPEND);
    int test_fd = os_open(RECORD_MEMQUERY_TESTS_FILE, OS_OPEN_WRITE | OS_OPEN_APPEND);
    ASSERT(results_fd != 0 && test_fd != 0);

    /* To support generating new tests from arbitrary sources, we pick the
     * timestamp to avoid name collisions.
     */
    char identifier[100];
    d_r_snprintf(identifier, BUFFER_SIZE_ELEMENTS(identifier), "%X", query_time_micros());

    memquery_iter_t iter;

    memquery_iterator_start(&iter, NULL, dynamo_heap_initialized);

    print_file(results_fd, "\nfake_memquery_result results_%s[] = {", identifier);
    int iters_count = 0;
    while (memquery_iterator_next(&iter)) {
        iters_count++;
        print_file(results_fd, "{\n");
        print_file(results_fd,
                   ".iter_result = {"
                   " .vm_start = (app_pc)" PFX ","
                   " .vm_end = (app_pc)" PFX ","
                   " .prot = %X,"
                   " .comment = \"%s\" },",
                   iter.vm_start, iter.vm_end, iter.prot, iter.comment);

        app_pc mod_base = 0, mod_end = 0;
        bool is_header = false;

        if (TEST(MEMPROT_READ, iter.prot) &&
#    ifdef X64
            /* We have observed segfaults reading data from very high addresses,
             * even though their mappings are listed as readable.
             */
            iter.vm_start < (app_pc)0xffffffff00000000 &&
#    endif /* X64 */
            module_is_header(iter.vm_start, iter.vm_end - iter.vm_start)) {

            is_header = true;
            if (!module_walk_program_headers(
                    iter.vm_start, iter.vm_end - iter.vm_start, false,
                    /*i#1589: ld.so relocated .dynamic*/
                    true, &mod_base, NULL, &mod_end, NULL, NULL)) {
                ASSERT_NOT_REACHED();
            }
        }
        print_file(results_fd,
                   ".is_header = %s, .mod_base = (app_pc)" PFX ", .mod_end = (app_pc)" PFX
                   ", \n},\n",
                   is_header ? "true" : "false", mod_base, mod_end);
    }

    print_file(results_fd, "};\n");
    memquery_iterator_stop(&iter);

    print_file(test_fd,
               "{\n"
               " .test_name = \"test_%s\",\n"
               " .iters = results_%s,\n",
               identifier, identifier, identifier);
    print_file(test_fd,
               " .iters_count = %d,\n"
               " .in_start = (app_pc)" PFX ",\n",
               iters_count, *start);
    if (name) {
        print_file(test_fd, " .in_name = \"%s\",\n", name);
    }
    int ret = real_memquery_library_bounds_by_iterator(
        name, start, end, fulldir, fulldir_size, filename, filename_size);
    print_file(test_fd,
               " .want_return = %d,\n"
               " .want_start = (app_pc)" PFX ",\n"
               " .want_end = (app_pc)" PFX ",\n",
               ret, *start, *end);
    if (fulldir) {
        print_file(test_fd, " .want_fulldir = \"%s\",\n", fulldir);
    }
    if (filename) {
        print_file(test_fd, " .want_filename = \"%s\",\n", filename);
    }
    print_file(test_fd, "},\n");

    os_close(results_fd);
    os_close(test_fd);
    return ret;
}

#    define memquery_library_bounds_by_iterator real_memquery_library_bounds_by_iterator

#endif /* RECORD_MEMQUERY */
