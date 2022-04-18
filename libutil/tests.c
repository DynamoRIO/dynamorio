/* **********************************************************
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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

/* ok this is more than a little shady...we'll just run through
 *  assertions, so that if a test fails, we'll keep on trying
 *  other ones.
 * probably the only way to handle this better is to come up with
 *  a way for running these tests in separate processes.
 *  (or: make this driver c++ so we can use try/catch?) */
#ifndef FAIL_ON_TEST_ASSERT
#    define ASSERTION_EXPRESSION(msg) report_assertion(msg)
#    define EXIT_ON_ASSERT FALSE
#endif

#include "share.h"
#include "processes.h"

#ifdef SINGLE_TEST
#    define TESTNAME STRINGIFY(SINGLE_TEST)
#    define RESTRICTED_BOOL TRUE
#else
#    define TESTNAME ""
#    define RESTRICTED_BOOL FALSE
#endif

BOOL test_asserted;
char *assert_message;

void
report_assertion(char *msg)
{
    if (!test_asserted)
        assert_message = msg;
    test_asserted = TRUE;
}

void
display_failure(char *testname)
{
    WCHAR evt_filename[MAX_PATH], w_testname[MAX_PATH];
    FILE *evtfile;

    fflush(stdout);
    fflush(stderr);
    printf("  Test FAILURE: %s\n", assert_message);
    _snwprintf(w_testname, MAX_PATH, L"%S", testname);
    DO_ASSERT(get_unique_filename(L".", w_testname, L".evtlog", evt_filename, MAX_PATH));
    evtfile = _wfopen(evt_filename, L"w");
    show_all_events(evtfile);
    fclose(evtfile);
    printf("  Events written to: %S\n", evt_filename);
    fflush(stdout);
}

int
main()
{
    WCHAR coredir[MAX_PATH];
    WCHAR old_drhome[MAX_PATH];
    WCHAR preinject[MAX_PATH];
    WCHAR mp_cfg_file[MAX_PATH];
    HANDLE *dummy = NULL; /* see LAUNCH_APP_WAIT_HANDLE macro */
    BOOL restricted = RESTRICTED_BOOL;
    int numtests = 0, passed = 0;

    set_debuglevel(DL_WARN);
    set_abortlevel(DL_WARN);

    _snwprintf(old_drhome, MAX_PATH, L"%s", get_dynamorio_home());

    get_testdir(coredir, MAX_PATH);
    CHECKED_OPERATION(setup_installation(coredir, TRUE));

    _snwprintf(mp_cfg_file, MAX_PATH, L"%s\\conf\\mp-defs.cfg", get_dynamorio_home());
    DO_ASSERT(file_exists(mp_cfg_file));

    CHECKED_OPERATION(get_preinject_name(preinject, MAX_PATH));
    DO_ASSERT(file_exists(preinject));

    /* cleanup */
    terminate_process_by_exe(L"tester_1.exe");
    terminate_process_by_exe(L"tester_2.exe");

    /* reset appinit to make sure it's set to the custom value */
    CHECKED_OPERATION(disable_protection());
    CHECKED_OPERATION(clear_policy());
    CHECKED_OPERATION(enable_protection());

    DO_ASSERT_WSTR_EQ(get_dynamorio_home(), coredir);

#define DO_TEST_HP(name, appstr, use_hotp, block)                  \
    {                                                              \
        if (!restricted || 0 == strcmp(#name, TESTNAME)) {         \
            test_asserted = FALSE;                                 \
            numtests++;                                            \
            printf("Executing " #name " test...\n");               \
            clear_eventlog();                                      \
            reset_last_event();                                    \
            CHECKED_OPERATION(load_test_config(appstr, use_hotp)); \
            block if (test_asserted)                               \
            {                                                      \
                display_failure(#name);                            \
            }                                                      \
            else                                                   \
            {                                                      \
                passed++;                                          \
                printf("  Passed.\n");                             \
            }                                                      \
        }                                                          \
    }

#include "tests.h"

#undef DO_TEST

    CHECKED_OPERATION(disable_protection());
    CHECKED_OPERATION(clear_policy());

    /* restore original drhome */
    CHECKED_OPERATION(setup_installation(old_drhome, TRUE));

    printf("\nTest results: %s [%d/%d tests passed]\n",
           passed == numtests ? "PASS" : "FAIL", passed, numtests);

    if (passed == numtests)
        return 0;
    else
        return -1;
}
