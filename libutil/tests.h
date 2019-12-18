/* **********************************************************
 * Copyright (c) 2005-2006 VMware, Inc.  All rights reserved.
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

/* DR core / shared library interface unit tests */
/* executed as part of the unit tests for processes.c */

/* need to have a core build anyway for these tests to work */
#include "win32/events.h"

/* nudge tends to hang/timeout if you nudge right after the
 *  process starts. so in order to let the basic tests pass,
 *  they all sleep for at least this long before trying to
 *  nudge.
 * of course this should be fixed and then we should create
 *  stress tests to address this specifically. */
#define NUDGE_LET_PROCESS_START_WAIT 500

#define TEST_TIMEOUT 2000
#define LAUNCH_TIMEOUT 1000

#define WAIT_FOR_APP(hProc) \
    DO_ASSERT(WAIT_OBJECT_0 == WaitForSingleObject(hProc, TEST_TIMEOUT * 2))

#define LONG_WAIT_FOR_APP(hProc) \
    DO_ASSERT(WAIT_OBJECT_0 == WaitForSingleObject(hProc, TEST_TIMEOUT * 20))

#define DO_TEST(name, appstr, block) DO_TEST_HP(name, appstr, TRUE, block)

#define TESTER_1_BLOCK                                                                   \
    "BEGIN_BLOCK\n"                                                                      \
    "APP_NAME=tester_1.exe\n"                                                            \
    "DYNAMORIO_OPTIONS=-kill_thread -kill_thread_max 1000 -report_max 0 -dumpcore_mask " \
    "0xff\n"                                                                             \
    "END_BLOCK\n"

#define TESTER_2_BLOCK                        \
    "BEGIN_BLOCK\n"                           \
    "APP_NAME=tester_2.exe\n"                 \
    "DYNAMORIO_OPTIONS=-dumpcore_mask 0xff\n" \
    "END_BLOCK\n"

#define TESTER_1_HOT_PATCH_BLOCK                           \
    "BEGIN_BLOCK\n"                                        \
    "APP_NAME=tester_1.exe\n"                              \
    "DYNAMORIO_OPTIONS=-kill_thread -dumpcore_mask 0xff\n" \
    "DYNAMORIO_HOT_PATCH_MODES=\\conf\\test-modes.cfg\n"   \
    "BEGIN_MP_MODES\n"                                     \
    "1\n"                                                  \
    "TEST.000A:2\n"                                        \
    "END_MP_MODES\n"                                       \
    "END_BLOCK\n"

#define TESTER_1_HOT_PATCH_DETECT_BLOCK                    \
    "BEGIN_BLOCK\n"                                        \
    "APP_NAME=tester_1.exe\n"                              \
    "DYNAMORIO_OPTIONS=-kill_thread -dumpcore_mask 0xff\n" \
    "DYNAMORIO_HOT_PATCH_MODES=\\conf\\test-modes.cfg\n"   \
    "BEGIN_MP_MODES\n"                                     \
    "1\n"                                                  \
    "TEST.000A:1\n"                                        \
    "END_MP_MODES\n"                                       \
    "END_BLOCK\n"

/* simple detach */
DO_TEST(detach, TESTER_1_BLOCK, {
    UINT pid;
    LAUNCH_APP(L"tester_1.exe 5000", &pid);
    /* FIXME: if this isn't here, the first run (right after
     *  building the tests) always fails. */
    Sleep(LAUNCH_TIMEOUT * 2);
    VERIFY_UNDER_DR(pid);
    CHECKED_OPERATION(detach(pid, TRUE, DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid);
    TERMINATE_PROCESS(pid);
});

/* pending restart */
DO_TEST(pending_restart, TESTER_1_BLOCK, {
    UINT pid;
    LAUNCH_APP(L"tester_1.exe 5000", &pid);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    CHECKED_OPERATION(detach(pid, TRUE, DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid);
    DO_ASSERT(is_process_pending_restart(pid));
    DO_ASSERT(is_any_process_pending_restart());
    TERMINATE_PROCESS(pid);
});

/* detach_exe */
DO_TEST(detach_exe, TESTER_1_BLOCK TESTER_2_BLOCK, {
    UINT pid1;
    UINT pid2;
    UINT pid3;
    LAUNCH_APP(L"tester_1.exe", &pid1);
    LAUNCH_APP(L"tester_2.exe", &pid2);
    LAUNCH_APP(L"tester_1.exe", &pid3);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid1);
    VERIFY_UNDER_DR(pid2);
    VERIFY_UNDER_DR(pid3);
    CHECKED_OPERATION(detach_exe(L"tester_1.exe", DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid1);
    VERIFY_UNDER_DR(pid2);
    VERIFY_NOT_UNDER_DR(pid3);
    DO_ASSERT(is_process_pending_restart(pid1));
    DO_ASSERT(!is_process_pending_restart(pid2));
    DO_ASSERT(is_any_process_pending_restart());
    TERMINATE_PROCESS(pid1);
    TERMINATE_PROCESS(pid2);
    TERMINATE_PROCESS(pid3);
});

/* detach_all */
DO_TEST(detach_all, TESTER_1_BLOCK TESTER_2_BLOCK, {
    UINT pid1;
    UINT pid2;
    UINT pid3;
    LAUNCH_APP(L"tester_1.exe", &pid1);
    LAUNCH_APP(L"tester_2.exe", &pid2);
    LAUNCH_APP(L"tester_1.exe", &pid3);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid1);
    VERIFY_UNDER_DR(pid2);
    VERIFY_UNDER_DR(pid3);
    CHECKED_OPERATION(detach_all(DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid1);
    VERIFY_NOT_UNDER_DR(pid2);
    VERIFY_NOT_UNDER_DR(pid3);
    DO_ASSERT(is_process_pending_restart(pid1));
    DO_ASSERT(is_any_process_pending_restart());
    TERMINATE_PROCESS(pid1);
    TERMINATE_PROCESS(pid2);
    TERMINATE_PROCESS(pid3);
});

/* consistency_detach */
DO_TEST(consistency_detach, TESTER_1_BLOCK TESTER_2_BLOCK, {
    UINT pid1;
    UINT pid2;
    UINT pid3;
    LAUNCH_APP(L"tester_1.exe", &pid1);
    LAUNCH_APP(L"tester_2.exe", &pid2);
    LAUNCH_APP(L"tester_1.exe", &pid3);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid1);
    VERIFY_UNDER_DR(pid2);
    VERIFY_UNDER_DR(pid3);
    CHECKED_OPERATION(load_test_config(TESTER_2_BLOCK, FALSE));
    CHECKED_OPERATION(consistency_detach(DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid1);
    VERIFY_UNDER_DR(pid2);
    VERIFY_NOT_UNDER_DR(pid3);
    TERMINATE_PROCESS(pid1);
    TERMINATE_PROCESS(pid2);
    TERMINATE_PROCESS(pid3);
});

/* check start/stop events */
DO_TEST(start_stop_event, TESTER_1_BLOCK, {
    UINT pid;
    HANDLE hProc;
    LAUNCH_APP_HANDLE(L"tester_1.exe 10", &pid, &hProc);
    WAIT_FOR_APP(hProc);
    DO_ASSERT(
        check_for_event(MSG_INFO_PROCESS_START, L"tester_1.exe", pid, NULL, NULL, 0));
    DO_ASSERT(
        check_for_event(MSG_INFO_PROCESS_STOP, L"tester_1.exe", pid, NULL, NULL, 0));
});

/* check detach event */
DO_TEST(detach_event, TESTER_1_BLOCK, {
    UINT pid;
    LAUNCH_APP(L"tester_1.exe 2000", &pid);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    CHECKED_OPERATION(detach(pid, TRUE, DETACH_RECOMMENDED_TIMEOUT));
    VERIFY_NOT_UNDER_DR(pid);
    Sleep(100);
    DO_ASSERT(
        check_for_event(MSG_INFO_PROCESS_START, L"tester_1.exe", pid, NULL, NULL, 0));
    DO_ASSERT(check_for_event(MSG_INFO_DETACHING, L"tester_1.exe", pid, NULL, NULL, 0));
    TERMINATE_PROCESS(pid);
});

/* check attack event */
DO_TEST(attack_event, TESTER_1_BLOCK, {
    UINT pid;
    HANDLE hProc;
    WCHAR s3[MAX_PATH];
    WCHAR s4[MAX_PATH];
    LAUNCH_APP_HANDLE(L"tester_1.exe 10 10 1", &pid, &hProc);
    WAIT_FOR_APP(hProc);
    DO_ASSERT(check_for_event(MSG_SEC_VIOLATION_THREAD, L"tester_1.exe", pid, s3, s4,
                              MAX_PATH));
    /* make sure threat id looks ok */
    DO_ASSERT(11 == wcslen(s3));
});

/* check forensics event/file */
DO_TEST(forensics_file, TESTER_1_BLOCK, {
    UINT pid;
    HANDLE hProc;
    WCHAR s3[MAX_PATH];
    WCHAR s4[MAX_PATH];
    LAUNCH_APP_HANDLE(L"tester_1.exe 10 10 1", &pid, &hProc);
    WAIT_FOR_APP(hProc);
    DO_ASSERT(
        check_for_event(MSG_SEC_VIOLATION_THREAD, L"tester_1.exe", pid, NULL, NULL, 0));
    DO_ASSERT(check_for_event(MSG_SEC_FORENSICS, L"tester_1.exe", pid, s3, s4, MAX_PATH));
    DO_ASSERT(file_exists(s3));
});

/* stress forensics event/file */
DO_TEST(forensics_stress, TESTER_1_BLOCK, {
    UINT pid;
    HANDLE hProc;
    WCHAR s3[MAX_PATH];
    WCHAR s4[MAX_PATH];
    int i;
    LAUNCH_APP_HANDLE(L"tester_1.exe 10 10 0 100", &pid, &hProc);
    LONG_WAIT_FOR_APP(hProc);
    for (i = 0; i < 100; i++) {
        DO_ASSERT(check_for_event(MSG_SEC_VIOLATION_THREAD, L"tester_1.exe", pid, NULL,
                                  NULL, 0));
        DO_ASSERT(
            check_for_event(MSG_SEC_FORENSICS, L"tester_1.exe", pid, s3, s4, MAX_PATH));
        DO_ASSERT(file_exists(s3));
    }
    DO_ASSERT(
        check_for_event(MSG_INFO_PROCESS_STOP, L"tester_1.exe", pid, NULL, NULL, 0));
});

/* check to make sure nudge doesn't leave the code lying around */
DO_TEST(check_nudge, TESTER_1_BLOCK, {
    UINT pid;
    WCHAR nudge_code_buf[MAX_PATH];

    LAUNCH_APP(L"tester_1.exe", &pid);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    Sleep(NUDGE_LET_PROCESS_START_WAIT);
    CHECKED_OPERATION(hotp_notify_modes_update(pid, TRUE, TEST_TIMEOUT));
    DO_ASSERT(ERROR_SUCCESS !=
              get_config_parameter(L_PRODUCT_NAME, FALSE, L_DYNAMORIO_VAR_NUDGE,
                                   nudge_code_buf, MAX_PATH));
    TERMINATE_PROCESS(pid);
});

/* simple test for hotpatching */
DO_TEST(hotp_protect, TESTER_1_HOT_PATCH_BLOCK, {
    UINT pid;
    char fc[MAX_PATH];
    LAUNCH_APP_AND_WAIT(L"tester_1.exe 100", &pid);
    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "10", 2));
});

/* detect should report 00 */
DO_TEST(hotp_detect, TESTER_1_HOT_PATCH_DETECT_BLOCK, {
    UINT pid;
    char fc[MAX_PATH];
    LAUNCH_APP_AND_WAIT(L"tester_1.exe 100", &pid);
    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "00", 2));
});

/* basic nudge test */
DO_TEST_HP(hotp_defs_nudge, TESTER_1_BLOCK, FALSE, /* don't load the modes file! */
           {
               UINT pid;
               char fc[MAX_PATH];
               HANDLE hProc;
               hotp_policy_status_table_t *status_table = NULL;

               /* first launch app w/o hotpatch */
               LAUNCH_APP_AND_WAIT(L"tester_1.exe 10", &pid);
               CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
               DO_ASSERT(0 == strncmp(fc, "00", 2));

               /* now, same thing with longer wait */
               LAUNCH_APP_HANDLE(L"tester_1.exe 2000", &pid, &hProc);
               Sleep(LAUNCH_TIMEOUT);
               VERIFY_UNDER_DR(pid);

               /* make sure nothing's there */
               DO_ASSERT(ERROR_DRMARKER_ERROR == get_hotp_status(pid, &status_table));

               /* load the new config -- this time with hot patching*/
               CHECKED_OPERATION(load_test_config(TESTER_1_HOT_PATCH_BLOCK, TRUE));

               /* and nudge */
               Sleep(NUDGE_LET_PROCESS_START_WAIT);
               CHECKED_OPERATION(hotp_notify_defs_update(pid, TRUE, TEST_TIMEOUT));
               VERIFY_UNDER_DR(pid);
               WAIT_FOR_APP(hProc);

               CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
               DO_ASSERT(0 == strncmp(fc, "10", 2));
           });

DO_TEST(hotp_modes_nudge, TESTER_1_HOT_PATCH_BLOCK, {
    UINT pid;
    char fc[MAX_PATH];
    HANDLE hProc;

    /* first launch app w/hotpatch protect */
    LAUNCH_APP_AND_WAIT(L"tester_1.exe 10", &pid);
    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "10", 2));

    /* now, same thing with longer wait */
    LAUNCH_APP_HANDLE(L"tester_1.exe 2000", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);

    /* load the new config */
    CHECKED_OPERATION(load_test_config(TESTER_1_HOT_PATCH_DETECT_BLOCK, TRUE));

    /* and do a modes nudge */
    Sleep(NUDGE_LET_PROCESS_START_WAIT);
    CHECKED_OPERATION(hotp_notify_modes_update(pid, TRUE, TEST_TIMEOUT));
    VERIFY_UNDER_DR(pid);
    WAIT_FOR_APP(hProc);

    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "00", 2));
});

/* nudge twice to make sure we don't die */
DO_TEST_HP(hotp_nudge_twice, TESTER_1_BLOCK, FALSE, {
    UINT pid;
    char fc[MAX_PATH];
    HANDLE hProc;

    /* first launch app w/o hotpatch */
    LAUNCH_APP_AND_WAIT(L"tester_1.exe 10", &pid);
    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "00", 2));

    LAUNCH_APP_HANDLE(L"tester_1.exe 2000", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);

    /* load the new config */
    CHECKED_OPERATION(load_test_config(TESTER_1_HOT_PATCH_BLOCK, TRUE));
    Sleep(NUDGE_LET_PROCESS_START_WAIT);
    CHECKED_OPERATION(hotp_notify_defs_update(pid, TRUE, TEST_TIMEOUT));
    VERIFY_UNDER_DR(pid);

    /* load the old config back */
    CHECKED_OPERATION(load_test_config(TESTER_1_HOT_PATCH_DETECT_BLOCK, TRUE));
    CHECKED_OPERATION(hotp_notify_defs_update(pid, TRUE, TEST_TIMEOUT));
    VERIFY_UNDER_DR(pid);
    WAIT_FOR_APP(hProc);

    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "00", 2));
});

/* tests state of the patches */
DO_TEST(hotp_protect_status, TESTER_1_HOT_PATCH_BLOCK, {
    UINT pid;
    HANDLE hProc;
    hotp_policy_status_table_t *status_table = NULL;

    LAUNCH_APP_HANDLE(L"tester_1.exe 10 2500", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    Sleep(500);
    CHECKED_OPERATION(get_hotp_status(pid, &status_table));
    DO_ASSERT(status_table != NULL && status_table->num_policies > 0);
    if (status_table != NULL && status_table->num_policies > 0) {
        UINT j;
        for (j = 0; j < status_table->num_policies; j++) {
            if (0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000A") ||
                0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000B")) {
                DO_ASSERT(status_table->policy_status_array[j].inject_status ==
                          HOTP_INJECT_PROTECT);
            }
        }
        free_hotp_status_table(status_table);
    }
    WAIT_FOR_APP(hProc);
});

/* tests state of the patches */
DO_TEST(hotp_pending_status, TESTER_1_HOT_PATCH_BLOCK, {
    UINT pid;
    HANDLE hProc;
    hotp_policy_status_table_t *status_table = NULL;

    LAUNCH_APP_HANDLE(L"tester_1.exe 2500", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    Sleep(200);
    CHECKED_OPERATION(get_hotp_status(pid, &status_table));
    DO_ASSERT(status_table != NULL && status_table->num_policies > 0);
    if (status_table != NULL && status_table->num_policies > 0) {
        UINT j;
        for (j = 0; j < status_table->num_policies; j++) {
            if (0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000A") ||
                0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000B")) {
                DO_ASSERT(status_table->policy_status_array[j].inject_status ==
                          HOTP_INJECT_PENDING);
            }
        }
        free_hotp_status_table(status_table);
    }
    WAIT_FOR_APP(hProc);
});

/* sanity check to make sure we have different state */
DO_TEST(hotp_detect_status, TESTER_1_HOT_PATCH_DETECT_BLOCK, {
    UINT pid;
    HANDLE hProc;
    hotp_policy_status_table_t *status_table = NULL;

    LAUNCH_APP_HANDLE(L"tester_1.exe 10 2500", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);
    Sleep(500);
    CHECKED_OPERATION(get_hotp_status(pid, &status_table));
    DO_ASSERT(status_table != NULL && status_table->num_policies > 0);
    if (status_table != NULL && status_table->num_policies > 0) {
        UINT j;
        for (j = 0; j < status_table->num_policies; j++) {
            if (0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000A") ||
                0 ==
                    strcmp(status_table->policy_status_array[j].policy_id, "TEST.000B")) {
                DO_ASSERT(status_table->policy_status_array[j].inject_status ==
                          HOTP_INJECT_DETECT);
            }
        }
        free_hotp_status_table(status_table);
    }
    WAIT_FOR_APP(hProc);
});

DO_TEST(hotp_modes_nudge_all, TESTER_1_HOT_PATCH_BLOCK, {
    UINT pid;
    char fc[MAX_PATH];
    HANDLE hProc;

    /* first launch app w/hotpatch protect */
    LAUNCH_APP_AND_WAIT(L"tester_1.exe 10", &pid);
    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "10", 2));

    /* now, same thing with longer wait */
    LAUNCH_APP_HANDLE(L"tester_1.exe 2000", &pid, &hProc);
    Sleep(LAUNCH_TIMEOUT);
    VERIFY_UNDER_DR(pid);

    /* load the new config */
    CHECKED_OPERATION(load_test_config(TESTER_1_HOT_PATCH_DETECT_BLOCK, TRUE));

    /* and do a modes nudge */
    Sleep(NUDGE_LET_PROCESS_START_WAIT);
    CHECKED_OPERATION(hotp_notify_all_modes_update(TEST_TIMEOUT));
    VERIFY_UNDER_DR(pid);
    WAIT_FOR_APP(hProc);

    CHECKED_OPERATION(read_file_contents(L"tester.out", fc, MAX_PATH, NULL));
    DO_ASSERT(0 == strncmp(fc, "00", 2));
});
