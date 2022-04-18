/* **********************************************************
 * Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2010 VMware, Inc.  All rights reserved.
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

#ifndef _DETERMINA_SHAREUTILS_H_
#define _DETERMINA_SHAREUTILS_H_

#include "share.h"
#include "dr_config.h"
#include "our_tchar.h"

#ifdef WINDOWS
#    include "config.h"
#endif

#ifdef DEBUG
#    include <stdio.h>
#endif

#if defined(WINDOWS) && defined(_DEBUG)
#    include <crtdbg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

const TCHAR *
get_dynamorio_home(void);

const TCHAR *
get_dynamorio_logdir(void);

bool
file_exists(const TCHAR *fn);

void
set_dr_platform(dr_platform_t platform);

/* return DR_PLATFORM_32BIT or DR_PLATFORM_64BIT */
dr_platform_t
get_dr_platform(void);

/* XXX: Rest are unported. */
#ifdef WINDOWS

void
wcstolower(WCHAR *str);

WCHAR *
get_exename_from_path(const WCHAR *path);

BOOL
get_commandline_qualifier(const WCHAR *command_line, WCHAR *derived_name,
                          UINT max_derived_length /* in elements */, BOOL no_strip);

DWORD
acquire_shutdown_privilege();

DWORD
reboot_system();

/* grokked from the core.
 * FIXME: shareme!
 * if NULL is passed for directory, then it is ignored and no directory
 *  check is done, and filename_base is assumed to be absolute.
 * TODO: make this a proactive check: make sure the file can be
 *  opened, eg, do a create/delete on the filename to be returned.
 */
BOOL
get_unique_filename(const WCHAR *directory, const WCHAR *filename_base,
                    const WCHAR *file_type, WCHAR *filename_buffer, UINT maxlen);

DWORD
delete_file_rename_in_use(WCHAR *filename);

DWORD
delete_file_on_boot(WCHAR *filename);

DWORD
delete_tree(const WCHAR *path);

DWORD
copy_file_permissions(WCHAR *filedst, WCHAR *filesrc);

DWORD
get_platform(DWORD *platform);

BOOL
is_wow64(HANDLE hProcess);

BOOL
using_system32_for_preinject(const WCHAR *preinject);

DWORD
get_preinject_path(WCHAR *buf, int nchars, BOOL force_local_path, BOOL short_path);

DWORD
get_preinject_name(WCHAR *buf, int nchars);

/* PR 244206: these flags should be used for all Reg{Create,Open,Delete}KeyEx calls
 * for HKLM\Software keys
 */
DWORD
platform_key_flags();

/* PR 244206: use this instead of RegDeleteKey for HKLM\Software keys */
DWORD
delete_product_key(HKEY hkey, LPCWSTR subkey);

DWORD
create_root_key();

DWORD
destroy_root_key();

/* note that this checks the opstring against the
 *  version of core that matches this build, NOT the version
 *  of the core that's actually installed! */
BOOL
check_opstring(const WCHAR *opstring);

/* acquires the privileges necessary to perform tasks like detach,
 *  nudge, etc. */
DWORD
acquire_privileges();

/* said privileges should always be released after usage */
DWORD
release_privileges();

void
ensure_directory_exists_for_file(WCHAR *filename);

void
mkdir_with_parents(const WCHAR *dirname);

DWORD
write_file_contents(WCHAR *path, char *contents, BOOL overwrite);

DWORD
write_file_contents_if_different(WCHAR *path, char *contents, BOOL *changed);

DWORD
read_file_contents(WCHAR *path, char *contents, SIZE_T maxchars, SIZE_T *needed);

DWORD
setup_installation(const WCHAR *path, BOOL overwrite);

DWORD
setup_cache_shared_directories(const WCHAR *cache_root);
DWORD
setup_cache_shared_registry(const WCHAR *cache_root, ConfigGroup *policy);

DWORD
set_registry_permissions_for_user(WCHAR *hklm_keyname, WCHAR *user);

/* used by get_violation_info() */
typedef struct _VIOLATION_INFO {
    DWORD flags;         /* IN, NYI */
    const WCHAR *report; /* OUT, Filename of generated report file. NULL if error.*/
    WCHAR buf[MAX_PATH]; /* space for filename */
} VIOLATION_INFO;

/* Takes in a MSG_SEC_FORENSICS event log record pevlr and generates a report file
 * (info->report) as specified by the flags field in info.  On success returns
 * ERROR_SUCCESS, else returns a failure code. */
DWORD
get_violation_info(EVENTLOGRECORD *pevlr, /* INOUT */ VIOLATION_INFO *info);

/* The final canary code is (FAIL_CODE << 16) | (TEST_TYPE && 0xffff) */
#    define CANARY_UNABLE_TO_TEST 1
#    define CANARY_SUCCESS 0
#    define CANARY_FAIL_HUNG -1
#    define CANARY_FAIL_CRASH -2
#    define CANARY_FAIL_VIOLATION -3
#    define CANARY_FAIL_DR_ERROR -4
#    define CANARY_FAIL_APP_INIT_INJECTION -5
#    define CANARY_FAIL_EARLY_INJECTION -6
/* future error codes */
#    define CANARY_TEST_TYPE_NATIVE 1
#    define CANARY_TEST_TYPE_THIN_CLIENT 2
#    define CANARY_TEST_TYPE_CLIENT 3
#    define CANARY_TEST_TYPE_MF 4
/* future types */
#    define GET_CANARY_CODE(test_type, fail_code) \
        ((fail_code << 16) | (test_type && 0xffff))

#    define CANARY_RUN_NO_REQUIRE_PASS(run) (run | (run << 16))
#    define CANARY_RUN_REQUIRES_PASS(run, flags) (((flags >> 16) & run) == 0)
#    define CANARY_RUN_NATIVE 0x0001
#    define CANARY_RUN_THIN_CLIENT_INJECT 0x0002
#    define CANARY_RUN_THIN_CLIENT 0x0004
#    define CANARY_RUN_CLIENT 0x0008
#    define CANARY_RUN_MF 0x0010

#    define CANARY_RUN_FLAGS_DEFAULT                                                  \
        (CANARY_RUN_NATIVE | CANARY_RUN_THIN_CLIENT_INJECT | CANARY_RUN_THIN_CLIENT | \
         CANARY_RUN_CLIENT | CANARY_RUN_NO_REQUIRE_PASS(CANARY_RUN_MF))
/* NYI */
#    define CANARY_INFO_FLAGS_DEFAULT 0

#    define CANARY_URL_SIZE 20       /* NYI so arbitrary */
#    define CANARY_MESSAGE_SIZE 1024 /* NYI so arbitrary */
typedef struct _CANARY_INFO {
    DWORD run_flags;                /* tests to run */
    DWORD info_flags;               /* info to gather, NYI */
    int canary_code;                /* canary return code, like an NTSTATUS */
    const WCHAR *report;            /* OUT, filename of generated report file */
    const WCHAR *url;               /* OUT, url string to use for querying determina */
    const WCHAR *msg;               /* OUT, msg to display to user */
    WCHAR buf_report[MAX_PATH];     /* space for report filename */
    WCHAR buf_url[CANARY_URL_SIZE]; /* space for url */
    WCHAR buf_message[CANARY_MESSAGE_SIZE]; /* space for use message */
    /* Used by DRcontrol to inject faults, FIXME get rid of these and the flags and
     * go to a more data driven model. Other users should set fault_run to 0. */
    DWORD fault_run;
    WCHAR *canary_fault_args;
} CANARY_INFO;
/* NOTE - arbitrary value, but shouldn't be -1 (core kill_proc value) or overlapping
 * an NTSTATUS (such as ACCESS_DENIED etc.). */
#    define CANARY_PROCESS_EXP_EXIT_CODE 0

/* Returns TRUE if the canary tests succeeded and protection should be enabled. Returns
 * FALSE if one of the canary tests failed and therefore protection should not be
 * enabled. Flags fields of info are used to specify which tests to run and what
 * information to gather, other fields are used to return additional information
 * to the caller. */
BOOL
run_canary_test(/* INOUT */ CANARY_INFO *info, WCHAR *version_msg);

/* Like run_canary_test(), except takes in as args what run_canary_test() implicitly
 * finds via the registry for greater customization. */
BOOL
run_canary_test_ex(FILE *file, /* INOUT */ CANARY_INFO *info, const WCHAR *scratch_folder,
                   const WCHAR *canary_process);

#endif /* WINDOWS */

#ifdef DEBUG

extern int debuglevel;
extern int abortlevel;

void
set_debuglevel(int level);

void
set_debugfile(FILE *fp);

void
set_abortlevel(int level);

#    define DL_FATAL 0
#    define DL_ERROR 2
#    define DL_WARN 4
#    define DL_INFO 6
#    define DL_VERB 8
#    define DL_FINEST 10

#    ifdef WINDOWS
#        pragma warning(disable : 4127)
#    endif

#    ifndef EXIT_ON_ASSERT
#        define EXIT_ON_ASSERT true
#    endif

#    ifndef ASSERTION_EXPRESSION
#        ifdef _DEBUG
#            define ASSERTION_EXPRESSION(msg) _ASSERTE(0)
#        else
#            define ASSERTION_EXPRESSION(msg)
#        endif
#    endif

#    define DO_ASSERT_EXPR(msg, expr, handle, handler)                                  \
        {                                                                               \
            if (!(expr)) {                                                              \
                char ___buf[MAXIMUM_PATH];                                              \
                _snprintf(___buf, MAXIMUM_PATH, "%s:%d [%s]", __FILE__, __LINE__, msg); \
                if (handle) {                                                           \
                    handler                                                             \
                } else if (EXIT_ON_ASSERT) {                                            \
                    fprintf(stderr, "ASSERT: %s\n", ___buf);                            \
                    fflush(stderr);                                                     \
                    exit(-1);                                                           \
                } else {                                                                \
                    ASSERTION_EXPRESSION(msg);                                          \
                }                                                                       \
            }                                                                           \
        }

#    define NULL_HANDLER ;

#    define DO_ASSERT(expr) DO_ASSERT_EXPR(#    expr, expr, 0, NULL_HANDLER)

#    define DO_ASSERT_HANDLE(expr, handler) DO_ASSERT_EXPR(#    expr, expr, 1, handler)

#    define DO_DEBUG(l, expr)                                        \
        {                                                            \
            if ((l) <= debuglevel) {                                 \
                expr                                                 \
            }                                                        \
            if ((l) <= abortlevel) {                                 \
                DO_ASSERT_EXPR("DEBUG failure", 0, 0, NULL_HANDLER); \
            }                                                        \
            fflush(stdout);                                          \
            fflush(stderr);                                          \
        }

#    define DO_ASSERT_WSTR_EQ(s1, s2) \
        DO_ASSERT(s1 != NULL);        \
        DO_ASSERT(s2 != NULL);        \
        DO_ASSERT(0 == wcscmp(s1, s2))

#    define DO_ASSERT_STR_EQ(s1, s2)        \
        DO_ASSERT(s1 != NULL);              \
        DO_ASSERT(s2 != NULL);              \
        if (s1 != NULL && s2 != NULL) {     \
            DO_ASSERT(0 == strcmp(s1, s2)); \
        }

#    define CHECKED_OPERATION(expr)          \
        {                                    \
            DWORD res = expr;                \
            if (res != ERROR_SUCCESS)        \
                printf("res=%d\n", res);     \
            DO_ASSERT(res == ERROR_SUCCESS); \
        }

#    define LAUNCH_APP_WAIT_HANDLE(relpath, pidvar, wait, handle)                       \
        {                                                                               \
            STARTUPINFO si = { 0 };                                                     \
            PROCESS_INFORMATION pi = { 0 };                                             \
            WCHAR cmdl[MAX_PATH];                                                       \
            _snwprintf(cmdl, MAX_PATH, L"%s", relpath);                                 \
            DO_ASSERT(                                                                  \
                CreateProcess(NULL, cmdl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)); \
            *pidvar = pi.dwProcessId;                                                   \
            DO_DEBUG(DL_VERB, printf("Launched %d=%S\n", *pidvar, cmdl););              \
            if (wait) {                                                                 \
                DWORD waitres = WaitForSingleObject(pi.hProcess, 5000);                 \
                DO_ASSERT(waitres == WAIT_OBJECT_0);                                    \
            } else {                                                                    \
                Sleep(100);                                                             \
            }                                                                           \
            if (handle == NULL) {                                                       \
                CloseHandle(pi.hThread);                                                \
                CloseHandle(pi.hProcess);                                               \
            } else {                                                                    \
                *handle = pi.hProcess;                                                  \
            }                                                                           \
        }

#    define LAUNCH_APP_HANDLE(relpath, pidvar, handle) \
        LAUNCH_APP_WAIT_HANDLE(relpath, pidvar, FALSE, handle)
#    define LAUNCH_APP_WAIT(relpath, pidvar, wait) \
        LAUNCH_APP_WAIT_HANDLE(relpath, pidvar, wait, dummy)
#    define LAUNCH_APP(relpath, pidvar) LAUNCH_APP_WAIT(relpath, pidvar, FALSE)
#    define LAUNCH_APP_AND_WAIT(relpath, pidvar) LAUNCH_APP_WAIT(relpath, pidvar, TRUE)

#    define TERMINATE_PROCESS(pid)                               \
        {                                                        \
            DO_DEBUG(DL_VERB, printf("terminating %d\n", pid);); \
            terminate_process(pid);                              \
            Sleep(100);                                          \
        }

#    define VERIFY_UNDER_DR(pid)                                \
        {                                                       \
            int stat = under_dynamorio(pid);                    \
            DO_ASSERT(stat != DLL_NONE && stat != DLL_UNKNOWN); \
        }

#    define VERIFY_NOT_UNDER_DR(pid)         \
        {                                    \
            int stat = under_dynamorio(pid); \
            DO_ASSERT(stat == DLL_NONE);     \
        }

#    ifdef WINDOWS
DWORD
load_test_config(const char *snippet, BOOL use_hotpatch_defs);

void
get_testdir(WCHAR *buf, UINT maxchars);

/* quick helpers for unit tests */
BOOL
check_for_event(DWORD type, WCHAR *exename, ULONG pid, WCHAR *s3, WCHAR *s4,
                UINT maxchars);
void
reset_last_event();

void
show_all_events();
#    endif

#else //#ifdef DEBUG

#    define DO_DEBUG(l, x)
#    define DO_ASSERT(x)

#    define CHECKED_OPERATION(expr)   \
        {                             \
            DWORD res = expr;         \
            if (res != ERROR_SUCCESS) \
                return res;           \
        }

#endif /* DEBUG */

/* alignment helpers, alignment must be power of 2 */
#define ALIGNED(x, alignment) ((((INT_PTR)x) & ((alignment)-1)) == 0)
#define ALIGN_BACKWARD(x, alignment) (((INT_PTR)x) & (~((alignment)-1)))
#define ALIGN_FORWARD(x, alignment) \
    ((((INT_PTR)x) + ((alignment)-1)) & (~((alignment)-1)))

#ifdef __cplusplus
}
#endif

#endif
