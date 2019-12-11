/* **********************************************************
 * Copyright (c) 2015-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2004-2009 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2004-2007 Determina Corp. */

/*
 * unit-rct.c unit tests for rct.c
 */

#include "rct.c"

UNIT_TEST_MAIN

/* This part is more of a regression test - but the rest of the unit
 * test can use its own executable image as a good test case.
 * FIXME: convert the first part into a regression test to run us on top of it
 * or even better - run DR on top of the unit tests
 */

#include <ctype.h>

typedef int (*fconvert_t)(int c);
typedef int (*fmult_t)(int);

int
foo(int a, bool lower)
{
    fconvert_t f = toupper;
    int res;
    if (lower)
        f = tolower;
    res = f(a);
    LOG(GLOBAL, LOG_ALL, 1, "foo('%c',%d): '%c'\n", a, lower, res);
    return res;
}

int
f2(int a)
{
    return 2 * a;
}

int
f3(int a)
{
    return 3 * a;
}

int
f7(int a)
{
    return 7 * a;
}

int
bar(int a, fmult_t f)
{
    int x = f2(a);
    int y = f3(a);
    int z = f(a);

    LOG(GLOBAL, LOG_ALL, 1, "bar(%d): %d %d %d\n", x, y, z);
    return z;
}

/*  Writable yet initialized data indeed needs to be processed.*/
fmult_t farr[2] = { f2, f7 };

static void
test_indcalls()
{
    EXPECT(foo('a', true), 'a');
    EXPECT(foo('a', false), 'A');
    EXPECT(foo('Z', true), 'z');
    EXPECT(foo('Z', false), 'Z');
    EXPECT(bar(5, f2), 10);
    EXPECT(bar(7, f3), 21);
    EXPECT(bar(7, f3), 21);
}

static char
test_switch_helper(char c)
{
    switch (c) {
    case 'a': return 'j';
    case 'b': return 'k';
    case 'c': return 'o';
    default: return c;
    }
}

static void
test_switch()
{
    EXPECT(test_switch_helper('a'), 'j');
    EXPECT(test_switch_helper('z'), 'z');
}

/* start of real unit test */

/* work on a small arrays of carefully planted values
 *
 * TODO: verify end
 * of region conditions - and add this at the end of a page to verify
 * not reaching out to bad memory out of the array
 */
static int
test_small_array(dcontext_t *dcontext)
{
    /*
     * [0 1 2 3 4 5 6 7] 8)
     * [4 3 2 1 5 3 2 1]
     */

    char arr[100];

    arr[0] = 4;
    arr[1] = 3;
    arr[2] = 2;
    arr[3] = 1;

    arr[4 + 0] = 5;
    arr[4 + 1] = 3;
    arr[4 + 2] = 2;
    arr[4 + 3] = 1;

    d_r_mutex_lock(&rct_module_lock); /* around whole sequence */

    EXPECT(find_address_references(dcontext, (app_pc)arr, (app_pc)(arr + 8),
                                   (app_pc)0x01020304, (app_pc)0x01020304),
           0);
    /* clean up to start over */
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 0);

    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020305),
           1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);

    /* repetition */
    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020305),
           1);
    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020305),
           0);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 0);

    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020309),
           2);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 2);

    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020309),
           2);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)0x01020304), 0);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)0x01020305), 1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, (app_pc)0x01020306,
                                              (app_pc)0x01020309),
           0);
    EXPECT(invalidate_ind_branch_target_range(dcontext, (app_pc)0x01020305,
                                              (app_pc)0x01020306),
           1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 0);

    EXPECT(find_address_references(dcontext, arr + 1, arr + 8, (app_pc)0x01020304,
                                   (app_pc)0x01020309),
           1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);

    EXPECT(find_address_references(dcontext, arr + 1, arr + 8, (app_pc)0x01020305,
                                   (app_pc)0x01020309),
           1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);

    EXPECT(find_address_references(dcontext, arr + 1, arr + 8, (app_pc)0x01020306,
                                   (app_pc)0x01020309),
           0);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 0);

    EXPECT(find_address_references(dcontext, arr + 4, arr + 8, (app_pc)0x01020300,
                                   (app_pc)0x01020309),
           1);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);

    EXPECT(find_address_references(dcontext, arr + 5, arr + 8, (app_pc)0x01020300,
                                   (app_pc)0x01020309),
           0);

    /* unreadable */
    EXPECT(find_address_references(dcontext, (app_pc)5, (app_pc)8, (app_pc)0x01020300,
                                   (app_pc)0x01020309),
           0);

    /* all address space for code */
    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0, (app_pc)-1),
           5); /* all match */

    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0, (app_pc)-1),
           0); /* all duplicates of last search */

    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 5);

    arr[0] = 4;
    arr[1] = 3;
    arr[2] = 2;
    arr[3] = 1;

    arr[4 + 0] = 4; /* duplicate entry */
    arr[4 + 1] = 3;
    arr[4 + 2] = 2;
    arr[4 + 3] = 1;

    /* all address space for code */
    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0, (app_pc)-1),
           4); /* all match but we have a duplicate */
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 4);

    EXPECT(find_address_references(dcontext, arr, arr + 8, (app_pc)0x01020300,
                                   (app_pc)0x01020305),
           1); /* two matches but with a duplicate */
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 1);

    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), 0);

    d_r_mutex_unlock(&rct_module_lock);

    return 1;
}

static void
test_lookup_delete(dcontext_t *dcontext)
{

    app_pc tag = (app_pc)0x1234567;
    fragment_t *f = rct_ind_branch_target_lookup(dcontext, tag);
    EXPECT_RELATION((ptr_uint_t)f, ==, 0);

    d_r_mutex_lock(&rct_module_lock);
    EXPECT(rct_add_valid_ind_branch_target(dcontext, tag), true);
    EXPECT(rct_add_valid_ind_branch_target(dcontext, tag), false);
    d_r_mutex_unlock(&rct_module_lock);

    f = rct_ind_branch_target_lookup(dcontext, tag);
    EXPECT_RELATION((ptr_uint_t)f, !=, 0);
    rct_flush_ind_branch_target_entry(dcontext, (FutureAfterCallFragment *)f);
    f = rct_ind_branch_target_lookup(dcontext, tag);
    EXPECT_RELATION((ptr_uint_t)f, ==, 0);
}

static void
test_self_direct(dcontext_t *dcontext)
{
    app_pc base_pc;
    size_t size;
    uint found;
    uint newfound;

#ifdef WINDOWS
    /* this will get both code and data FIXME: data2data reference
     * will be the majority
     */
    size = get_allocation_size((app_pc)test_self_direct, &base_pc);
#else
    /* platform agnostic but only looks for current CODE section, and
     * on windows is not quite what we want - since base_pc will just
     * be just page aligned
     */
    get_memory_info((app_pc)test_self_direct, &base_pc, &size, NULL);
#endif

    d_r_mutex_lock(&rct_module_lock);
    found = find_address_references(dcontext, base_pc, base_pc + size, base_pc,
                                    base_pc + size);
    d_r_mutex_unlock(&rct_module_lock);

    /* guesstimate */
    EXPECT_RELATION(found, >, 140);
#ifdef WINDOWS
    /* FIXME: note data2data have a huge part here */
    EXPECT_RELATION(found, <, 20000);
#else
    EXPECT_RELATION(found, <, 1000);
#endif
    EXPECT(is_address_taken(dcontext, (app_pc)f3), true);
    EXPECT(is_address_taken(dcontext, (app_pc)f2), true);
    EXPECT(is_address_taken(dcontext, (app_pc)f7), true); /* array reference only */

    /* it is pretty hard to produce the address of a static
     * (e.g. test_self) without making it address taken ;) so we just
     * add a number to known to be good one's */
    EXPECT(is_address_taken(dcontext, (app_pc)f3 + 1), false);
    EXPECT(is_address_taken(dcontext, (app_pc)f3 + 2), false);
    EXPECT(is_address_taken(dcontext, (app_pc)f2 + 1), false);
    EXPECT(is_address_taken(dcontext, (app_pc)f7 + 1), false);

    d_r_mutex_lock(&rct_module_lock);
    EXPECT(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), found);
    EXPECT_RELATION(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), ==,
                    0); /* nothing missed */
    d_r_mutex_unlock(&rct_module_lock);

    /* now try manually rct_analyze_module_at_violation */

    d_r_mutex_lock(&rct_module_lock);
    EXPECT(rct_analyze_module_at_violation(dcontext, (app_pc)test_self_direct), true);

    /* should be all found */
    /* FIXME: with the data2data in fact a few noisy entries show up
     * since second lookup in data may differ from original
     */
    newfound = find_address_references(dcontext, base_pc, base_pc + size, base_pc,
                                       base_pc + size);
    EXPECT_RELATION(newfound, <, 4);
    EXPECT_RELATION(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), >,
                    found + newfound - 5); /* FIXME: data references uncomparable */

    EXPECT_RELATION(invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1), ==,
                    0); /* nothing missed */
    d_r_mutex_unlock(&rct_module_lock);
}

static void
test_rct_ind_branch_check(void)
{
    uint found;

    linkstub_t l;
    fragment_t f;

    /* to pass args security_violation assumes present */
    dcontext_t *dcontext = create_new_dynamo_context(true /*initial*/, NULL, NULL);

    f.tag = (app_pc)0xbeef;
    l.fragment = &f;
    l.flags = LINK_INDIRECT | LINK_CALL;
    set_last_exit(dcontext, &l);
    dcontext->logfile = GLOBAL;

    /* this should auto call rct_analyze_module_at_violation((app_pc)test_self) */
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3), 1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3), 1);

    /* running in -detect_mode we should get -1 */
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3 + 1), -1);

    /* not code */
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)0xbad), 2);

    /* starting over */
    d_r_mutex_lock(&rct_module_lock);
    invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1);
    d_r_mutex_unlock(&rct_module_lock);

    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3), 1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f2), 1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f7), 1); /* array reference only */

    /* it is pretty hard to produce the address of a static
     * (e.g. test_self) without making it address taken ;) so we just
     * add a number to known to be good one's */
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3 + 1), -1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f3 + 2), -1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f2 + 1), -1);
    EXPECT(rct_ind_branch_check(dcontext, (app_pc)f7 + 1), -1);

    d_r_mutex_lock(&rct_module_lock);
    found = invalidate_ind_branch_target_range(dcontext, 0, (app_pc)-1);
    d_r_mutex_unlock(&rct_module_lock);

    EXPECT_RELATION(found, >, 140);
}

/*
 * TODO: LoadLibrary(kernel32) and work on that
 */
static void
test_loaddll()
{
    /* TODO: LoadLibrary/GetProcAddress */
    /* and dlopen/dlsym */
    LOG(GLOBAL, LOG_ALL, 1, "test_loaddll: NYI\n");
}

/*
 * TODO: add a unit test that in fact creates multiple sections by using
 * #pragma(code_section) that is used by device drivers to mark
 * PAGEABLE code sections
 *
 */

#if TEST_MULTI_SECTIONS /* NYI */
#    pragma code_seg(".my_code1")
void
func2()
{
}

#    pragma code_seg(push, r1, ".my_code2")
void
func3()
{
}

#    pragma code_seg(pop, r1) /* back to my_code1 */
void
func4()
{
}
#    pragma code_seg /* back in .text */
#endif

static int
unit_main(void)
{
    dcontext_t *dcontext = GLOBAL_DCONTEXT;

    standalone_init();
    /* keep in mind that not all units are properly initialized above */
    fragment_init();

    /* options have to be set on the command line since after
     * synchronization any overrides will be gone
     */
    EXPECT(dynamo_options.detect_mode, true);
    EXPECT(dynamo_options.rct_ind_call, 11);
    EXPECT(dynamo_options.rct_ind_jump, 11);

    /* FIXME: report_current_process calls is_couldbelinking()
     * maybe we should just set the TEB entry with a good context
     */
    EXPECT(dynamo_options.diagnostics, false);

    TESTRUN(test_indcalls());
    TESTRUN(test_switch());

    TESTRUN(test_lookup_delete(dcontext));
    TESTRUN(test_small_array(dcontext));

    TESTRUN(test_self_direct(dcontext));

    TESTRUN(test_rct_ind_branch_check());

    TESTRUN(test_loaddll());

    LOG(GLOBAL, LOG_ALL, 1, "DONE unit-rct.c:unit_main()\n");

    dynamo_exited = true;
    fragment_exit();
    standalone_exit();

    return 0;
}
