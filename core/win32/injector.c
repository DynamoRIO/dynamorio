/* **********************************************************
 * Copyright (c) 2002-2009 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * injector.c - standalone injector for win32
 */

/* FIXME: Unicode support?!?! case 61 */

/* Disable warnings from vc8 include files */
/* buildtools\VC\8.0\dist\VC\PlatformSDK\Include\prsht.h(201)
 * "nonstandard extension used : nameless struct/union"
 */
#pragma warning( disable : 4201 )
/* buildtools\VC\8.0\dist\VC\PlatformSDK\Include\wintrust.h(459)
 * "named type definition in parentheses"
 */
#pragma warning( disable : 4115 )

#include "../globals.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <imagehlp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>             /* memset */
#include <time.h>
#include <tchar.h>

#include "globals_shared.h"
#include "ntdll.h"
#include "inject_shared.h"
#include "os_private.h"

#define VERBOSE 0
#if VERBOSE
# define DO_VERBOSE(x) x
# undef printf
# define VERBOSE_PRINT(x) printf x
#else
# define DO_VERBOSE(x) 
# define VERBOSE_PRINT(x)
#endif
#define FP stderr

/* FIXME: case 64 would like ^C to kill child process, it doesn't.
 * also, child process seems able to read stdin but not to write
 * to stdout or stderr (in fact it dies if it tries)
 * I think Ctrl-C is delivered only if you have a debugger(windbg) attached.
 */
#define HANDLE_CONTROL_C 0

/* global vars */
static int limit; /* in seconds */
static BOOL showmem;
static BOOL showstats;
static BOOL inject;
static BOOL force_injection;
static BOOL use_environment = TRUE; /* FIXME : for now default to using
                                     * the environment, below we check and
                                     * never use the environment if using
                                     * debug injection.  Revisit.
                                     */
static const char *ops_param;
static double wallclock; /* in seconds */

/* FIXME : assert stuff, internal error, display_message duplicated from 
 * pre_inject, share? */

/* for asserts, copied from utils.h */
#ifdef assert
# undef assert
#endif
/* avoid mistake of lower-case assert */
#define assert assert_no_good_use_ASSERT_instead
void internal_error(char *file, int line, char *msg);
#undef ASSERT
#ifdef DEBUG
void display_error(char *msg);
# ifdef INTERNAL
#   define ASSERT(x)         if (!(x)) internal_error(__FILE__, __LINE__, #x)
# else 
#   define ASSERT(x)         if (!(x)) internal_error(__FILE__, __LINE__, "")
# endif /* INTERNAL */
#else
# define display_error(msg) ((void) 0)
# define ASSERT(x)         ((void) 0)
#endif /* DEBUG */

/* in ntdll.c */
extern char *get_application_name(void);
extern char *get_application_pid(void);

static void
display_error_helper(wchar_t *msg) 
{
    wchar_t title_buf[MAX_PATH + 64];
    _snwprintf(title_buf, BUFFER_SIZE_ELEMENTS(title_buf), 
              L_PRODUCT_NAME L" Notice: %hs(%hs)", 
              get_application_name(), get_application_pid());
    NULL_TERMINATE_BUFFER(title_buf);

    /* for unit tests: assume that if a limit is set, we are in a
     *  script so it's ok to just display to stderr. avoids hangs when
     *  an error is encountered. */
    if (limit <= 0)
        nt_messagebox(msg, title_buf);
    else {
        fprintf(FP, "\n\n%ls\n%ls\n\n", title_buf, msg);
        fflush(FP);
    }
}

void
internal_error(char *file, int line, char *expr)
{
#ifdef INTERNAL
# define FILENAME_LENGTH L""
#else 
/* truncate file name to first character */
# define FILENAME_LENGTH L".1"   
#endif
    wchar_t buf[512];
    _snwprintf(buf, BUFFER_SIZE_ELEMENTS(buf), 
              L"Injector Error %" FILENAME_LENGTH L"hs:%d %hs\n", 
              file, line, expr);
    NULL_TERMINATE_BUFFER(buf);
    display_error_helper(buf);
    TerminateProcess(GetCurrentProcess(), (uint)-1);
}

#ifdef DEBUG
void 
display_error(char *msg)
{
# ifdef EXTERNAL_INJECTOR
    fprintf(FP, msg);
# else
    wchar_t buf[512];
    _snwprintf(buf, BUFFER_SIZE_ELEMENTS(buf), L"%hs", msg);
    NULL_TERMINATE_BUFFER(buf);
    display_error_helper(buf);
# endif
}
#endif /* DEBUG */

#if HANDLE_CONTROL_C
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType   //  control signal type
                           )
{
    printf("Inside HandlerRoutine!\n");
    fflush(stdout);
    /*    GenerateConsoleCtrlEvent(dwCtrlType, phandle);*/
    return TRUE;
}
#endif 

static void
print_mem_stats(HANDLE h)
{
    VM_COUNTERS mem;
    /* not in DR library - floating point use is OK */
    int secs = (int) wallclock;

    if (!get_process_mem_stats(h, &mem)) {
        /* failed */
        memset(&mem, 0, sizeof(VM_COUNTERS));
    }

    if (showstats) {
        int cpu = get_process_load(h);
        /* Elapsed real (wall clock) time.  */
        if (secs >= 3600) {     /* One hour -> h:m:s.  */
            fprintf(FP, "%ld:%02ld:%02ldelapsed ",
                    secs / 3600,
                    (secs % 3600) / 60,
                    secs % 60);
        } else {
            fprintf(FP, "%ld:%02ld.%02ldelapsed ",      /* -> m:s.  */
                    secs / 60,
                    secs % 60,
                    0 /* for now */);
        }
        fprintf(FP, "%d%%CPU \n", cpu);
        fprintf(FP, "(%lu tot, %lu RSS, %lu paged, %lu non, %lu swap)k\n",
                mem.PeakVirtualSize/1024,
                mem.PeakWorkingSetSize/1024,
                mem.QuotaPeakPagedPoolUsage/1024,
                mem.QuotaPeakNonPagedPoolUsage/1024,
                mem.PeakPagefileUsage/1024);
    }
    if (showmem) {
        fprintf(FP, "Process Memory Statistics:\n");
        fprintf(FP, "\tPeak virtual size:         %6d KB\n",
               mem.PeakVirtualSize/1024);
        fprintf(FP, "\tPeak working set size:     %6d KB\n",
               mem.PeakWorkingSetSize/1024);
        fprintf(FP, "\tPeak paged pool usage:     %6d KB\n",
               mem.QuotaPeakPagedPoolUsage/1024);
        fprintf(FP, "\tPeak non-paged pool usage: %6d KB\n",
               mem.QuotaPeakNonPagedPoolUsage/1024);
        fprintf(FP, "\tPeak pagefile usage:       %6d KB\n",
               mem.PeakPagefileUsage/1024);
    }
}

/*************************************************************************/
/* Following code handles the copying of environment variables to the 
 * registry (the -env option, default on) and unsetting them later
 *
 * FIXME : race conditions with someone else modifing this registry key,
 *         doesn't restore registry if -no_wait
 * NOTE : doesn't propagate if using debug injection method (by design)
 *         
 */

static BOOL created_product_reg_key;
static BOOL created_image_reg_key;
static HKEY image_name_key;
static HKEY product_name_key;

/* use macros to avoid duplication */
/* This check is neccesary since we don't have 
 * preprocessor warnings enabled on windows */
#if defined(TEMP_CMD) || defined(DO_CMD)
#error TEMP_CMD or DO_ENV_VARS macro already declared!
#endif
/* if need to add or remove environment variables looked for, do it here */
#define DO_ENV_VARS()                             \
 TEMP_CMD(options, OPTIONS);                      \
 TEMP_CMD(logdir, LOGDIR);                        \
 TEMP_CMD(unsupported, UNSUPPORTED);              \
 TEMP_CMD(rununder, RUNUNDER);                    \
 TEMP_CMD(autoinject, AUTOINJECT);                \
 TEMP_CMD(cache_root, CACHE_ROOT);                \
 TEMP_CMD(cache_shared, CACHE_SHARED)

#define TEMP_CMD(name, NAME)                               \
 static BOOL overwrote_##name##_value;                     \
 static BOOL created_##name##_value;                       \
 static TCHAR old_##name##_value[MAX_REGISTRY_PARAMETER]

/* Note - leading spacs is to avoid make ugly rule on zero argument function
 * declerations not using void - xref PR 215100 */
 DO_ENV_VARS();

static void
set_registry_from_env(const TCHAR *image_name, const TCHAR *dll_path) 
{
#undef TEMP_CMD
#define TEMP_CMD(name, NAME)                     \
 BOOL do_##name;                                 \
 TCHAR name##_value[MAX_REGISTRY_PARAMETER]

    DO_ENV_VARS();

    DWORD disp, size, type;
    int res, len;
    int rununder_int_value;

    /* get environment variable values if they are set */
#undef TEMP_CMD    
#define TEMP_CMD(name, NAME)                                                  \
 name##_value[0] = '\0';  /* to be pedantic */                                \
 len = GetEnvironmentVariable(_TEXT(DYNAMORIO_VAR_##NAME), name##_value,      \
                              BUFFER_SIZE_ELEMENTS(name##_value));            \
 do_##name = (use_environment &&                                              \
              (len > 0 ||                                                     \
              (len == 0 && GetLastError() != ERROR_ENVVAR_NOT_FOUND)));       \
 ASSERT(len < BUFFER_SIZE_ELEMENTS(name##_value));                            \
 VERBOSE_PRINT(("Environment var %s for %s, value = %s\n",                    \
        do_##name ? "set" : "not set", #name, name##_value));

    DO_ENV_VARS();

    if (ops_param != NULL) {
        /* -ops overrides env var */
        strncpy(options_value, ops_param, BUFFER_SIZE_ELEMENTS(options_value));
        NULL_TERMINATE_BUFFER(options_value);
        do_options = TRUE;
    }

    /* we always want to set the rununder to make sure RUNUNDER_ON is on
     * to support following children; we set RUNUNDER_EXPLICIT to allow
     * injecting even when preinject is configured. */
    /* FIXME: we read only decimal */
    rununder_int_value = _ttoi(rununder_value);
    rununder_int_value |= RUNUNDER_ON | RUNUNDER_EXPLICIT;

    do_rununder = true;
    _itot(rununder_int_value, rununder_value, 
          10 /* FIXME : is the radix abstracted somewhere */);

    /* for follow_children, we set DYNAMORIO_AUTOINJECT (unless
     * overridden by env var: then child will use env value, while
     * parent uses cmdline path) */
    if (!do_autoinject && dll_path != NULL) {
        _tcsncpy(autoinject_value, dll_path, BUFFER_SIZE_ELEMENTS(autoinject_value));
        do_autoinject = true;
    }

    /* FIXME : doesn't support svchost-* yet */
    ASSERT(_tcsicmp(_TEXT(SVCHOST_EXE_NAME), image_name));
    res = RegCreateKeyEx(DYNAMORIO_REGISTRY_HIVE, 
                         _TEXT(DYNAMORIO_REGISTRY_KEY), 0,  NULL, 
                         REG_OPTION_NON_VOLATILE, KEY_CREATE_SUB_KEY, NULL,
                         &product_name_key, &disp);
    ASSERT(res == ERROR_SUCCESS);
    if (disp == REG_CREATED_NEW_KEY) {
        created_product_reg_key = TRUE;
    }
    res = RegCreateKeyEx(product_name_key, image_name, 0, NULL, 
                         REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE|KEY_SET_VALUE, 
                         NULL, &image_name_key, &disp);
    ASSERT(res == ERROR_SUCCESS);
    if (disp == REG_CREATED_NEW_KEY) {
        created_image_reg_key = TRUE;
    }
    
    DO_VERBOSE({
        printf("created product key? %s\ncreated image key? %s\n",
               created_product_reg_key ? "yes" : "no", 
               created_image_reg_key ? "yes" : "no");
        fflush(stdout);
    });
 
    /* Now set values */
#undef TEMP_CMD
#define TEMP_CMD(name, NAME)                                                  \
 if (do_##name) {                                                             \
    size = BUFFER_SIZE_BYTES(old_##name##_value);                             \
    res = RegQueryValueEx(image_name_key, _TEXT(DYNAMORIO_VAR_##NAME),        \
                          NULL, &type, (BYTE *) old_##name##_value, &size);   \
    ASSERT(size <= BUFFER_SIZE_BYTES(old_##name##_value));                    \
    if (res == ERROR_SUCCESS) {                                               \
        overwrote_##name##_value = TRUE;                                      \
        ASSERT(type == REG_SZ);                                               \
    } else {                                                                  \
        created_##name##_value = TRUE;                                        \
    }                                                                         \
    res = RegSetValueEx(image_name_key, _TEXT(DYNAMORIO_VAR_##NAME),          \
                        0, REG_SZ, (BYTE *) name##_value,                     \
                        (DWORD) (_tcslen(name##_value)+1)*sizeof(TCHAR));     \
    ASSERT(res == ERROR_SUCCESS);                                             \
    VERBOSE_PRINT(("%s %s registry value with \"%s\" replacing \"%s\"\n",     \
           overwrote_##name##_value  ? "overwrote" : "created", #name,        \
           name##_value, overwrote_##name##_value ? old_##name##_value : ""));\
 }
    
    DO_ENV_VARS();

    DO_VERBOSE({fflush(stdout);});
}

static void
unset_registry_from_env(const TCHAR *image_name)
{
    int res;

    VERBOSE_PRINT(("Restoring registry configuration\n"));

    /* restore registry values */
#undef TEMP_CMD
#define TEMP_CMD(name, NAME)                                                 \
 if (overwrote_##name##_value) {                                             \
     res = RegSetValueEx(image_name_key, _TEXT(DYNAMORIO_VAR_##NAME), 0,     \
                         REG_SZ /* FIXME : abstracted somewhere? */,         \
                         (BYTE *) old_##name##_value,                        \
                         (DWORD)(_tcslen(old_##name##_value)+1) * sizeof(TCHAR));\
     ASSERT(res == ERROR_SUCCESS);                                           \
     VERBOSE_PRINT(("Restored %s value to %s\n", #name, old_##name##_value));\
 } else if (created_##name##_value) {                                        \
     res = RegDeleteValue(image_name_key, _TEXT(DYNAMORIO_VAR_##NAME));      \
     ASSERT(res == ERROR_SUCCESS);                                           \
     VERBOSE_PRINT(("Deleted %s value\n", #name));                           \
 }

    DO_ENV_VARS();

    /* delete keys if created them */
    if (created_image_reg_key) {
        res = RegDeleteKey(product_name_key, image_name);
        ASSERT(res == ERROR_SUCCESS);
        VERBOSE_PRINT(("deleted image reg key\n"));
    }
    if (created_product_reg_key) {
        res = RegDeleteKey(DYNAMORIO_REGISTRY_HIVE, 
                           _TEXT(DYNAMORIO_REGISTRY_KEY));
        ASSERT(res == ERROR_SUCCESS);
        VERBOSE_PRINT(("deleted product reg key\n"));
    }
    DO_VERBOSE({fflush(stdout);});
}

#undef DO_ENV_VARS
#undef TEMP_CMD

/************************** end environment->registry **********************/

/***************************************************************************/
/* The following code handles checking for, setting and unsetting of the
 * debug key injection method
 *
 * This whole section can go away once we have our own version of create 
 * process that doesn't check the debugger key FIXME
 */

static HKEY debugger_key;
static TCHAR debugger_key_full_name[MAX_PATH];
static TCHAR debugger_key_value[3*MAX_PATH];
static DWORD debugger_key_value_size = BUFFER_SIZE_BYTES(debugger_key_value);
static BOOL (*debug_stop_function)(int); 

static BOOL
using_debugger_key_injection(const TCHAR *image_name)
{
    int res;

    debug_stop_function = (BOOL (*)(int))
        (GetProcAddress(GetModuleHandle(TEXT("Kernel32")), 
                        "DebugActiveProcessStop"));
    
    _sntprintf(debugger_key_full_name, 
               BUFFER_SIZE_ELEMENTS(debugger_key_full_name), _TEXT("%hs\\%s"),
               DEBUGGER_INJECTION_KEY, image_name);
    NULL_TERMINATE_BUFFER(debugger_key_full_name);

    VERBOSE_PRINT(("debugger key %s\n", debugger_key_full_name));

    res = RegOpenKeyEx(DEBUGGER_INJECTION_HIVE, debugger_key_full_name, 
                       0, KEY_QUERY_VALUE + KEY_SET_VALUE, &debugger_key);
    if (ERROR_SUCCESS != res) 
        return FALSE;
    res = RegQueryValueEx(debugger_key, _TEXT(DEBUGGER_INJECTION_VALUE_NAME),
                          NULL, NULL, (BYTE *) debugger_key_value, 
                          &debugger_key_value_size);
    if (ERROR_SUCCESS != res || 
        /* FIXME : it would be better to check if our commandline matched 
         * what was in the registry value, instead of looking for drinject */
        _tcsstr(debugger_key_value, _TEXT(DRINJECT_NAME)) == 0) {
        RegCloseKey(debugger_key);
        return FALSE;
    }

    /* since returning TRUE, we don't close the debugger_key (it will be 
     * needed by the unset and restore functions). The restore function will
     * close it */

    return TRUE;
}

static
void unset_debugger_key_injection()
{
    if (debug_stop_function == NULL) {
        int res = RegDeleteValue(debugger_key, 
                                 _TEXT(DEBUGGER_INJECTION_VALUE_NAME));
        VERBOSE_PRINT(("Succesfully deleted debugger registry value? %s\n", 
                       (ERROR_SUCCESS == res) ? "yes" : "no"));
        if (ERROR_SUCCESS != res) {
            ASSERT(FALSE);
            /* prevent infinite recursion, die now */
            abort();
        }
    }
}

static
void restore_debugger_key_injection(int id, BOOL started)
{
    int res;
    if (debug_stop_function == NULL) {
        res = RegSetValueEx(debugger_key, 
                            _TEXT(DEBUGGER_INJECTION_VALUE_NAME), 0, REG_SZ, 
                            (BYTE *) debugger_key_value, debugger_key_value_size);
        VERBOSE_PRINT(("Succesfully restored debugger registry value? %s\n", 
                       (ERROR_SUCCESS == res) ? "yes" : "no"));
    } else {
        if (started) {
            res = (*debug_stop_function)(id);
            VERBOSE_PRINT(("Succesfully detached from debugging process? %s\n",
                           res ? "yes" : "no"));
        }
    }
    /* close the global debugger key */
    RegCloseKey(debugger_key);
}

/***************************** end debug key injection ********************/

static const TCHAR*
get_image_name(TCHAR *app_name)
{
    const TCHAR *name_start = double_tcsrchr(app_name, _TEXT('\\'), _TEXT('/'));
    if (name_start == NULL) 
        name_start = app_name;
    else
        name_start++;
    return name_start;
}

static char *
strip_quotes(char *str)
{
    if (str[0] == '\"' || str[0] == '\'' || str[0] == '`') {
        /* remove quotes */
        str++;
        str[strlen(str)-1] = '\0';
    }
    return str;
}

int usage(char *us)
{
#ifdef EXTERNAL_INJECTOR
    fprintf(FP,
            "Usage : %s [-s limit_sec | -m limit_min | -h limit_hr | -no_wait] \n"
            "        [-noinject] <application> [args...]\n"
            "    -no_wait   don't wait for application to exit before returning\n"
            "    -s -m -h   kill the application if runs longer than specified limit\n"
            "    -noinject  just launch the process without injecting\n",
            us);
#else
    fprintf(FP, "Usage: %s [-s limit_sec | -m limit_min | -h limit_hr | -no_wait] [-stats]\n"
            "      [-mem] [-no_env | -env] [-ops <options>] [-force]\n"
            "      -noinject|<dll_to_inject> <program> <args...>\n", us);
#endif
    return 0;
}

#define MAX_CMDLINE 2048

/* we must be a console application in order to have the process
 * we launch NOT get a brand new console window!
 */
int __cdecl
main(int argc, char *argv[], char *envp[])
{
    LPTSTR              app_name = NULL;
    TCHAR               full_app_name[2*MAX_PATH];
    TCHAR               app_cmdline[MAX_CMDLINE];
    LPTSTR              ch;
    const TCHAR*        image_name;
    int                 i;
    LPTSTR              pszCmdLine = GetCommandLine();
    LPTSTR              DYNAMORIO_PATH = NULL;
    PLOADED_IMAGE       li;
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    CONTEXT             cxt;
    HANDLE              phandle;
    BOOL                success;
    BOOL                res;
    BYTE *              app_entry;
    DWORD               wait_result;
    int                 arg_offs = 1;
    time_t              start_time, end_time;
    BOOL                debugger_key_injection;
    BOOL                limit_default = TRUE;
#ifdef EXTERNAL_INJECTOR
    char                library_path_buf[MAXIMUM_PATH];
#endif

    STARTUPINFO mysi;
    GetStartupInfo(&mysi);

    /* parse args */
    showmem = FALSE;
    inject = TRUE;

#ifdef EXTERNAL_INJECTOR
    if (argc < 2) {
        return usage(argv[0]);
    }
#else
    if (argc < 3) {
        return usage(argv[0]);
    }
#endif
    while (argv[arg_offs][0] == '-') {
        if (strcmp(argv[arg_offs], "-s") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1]);
            limit_default = FALSE;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-m") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1])*60;
            limit_default = FALSE;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-h") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            limit = atoi(argv[arg_offs+1])*3600;
            limit_default = FALSE;
            arg_offs += 2;
        } else if (strcmp(argv[arg_offs], "-no_wait") == 0) {
            limit = -1;
            limit_default = FALSE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-noinject") == 0) {
            inject = FALSE;
            arg_offs += 1;
        }
#ifndef EXTERNAL_INJECTOR
        else if (strcmp(argv[arg_offs], "-v") == 0) {
            /* just ignore -v, only here for compatibility with texec */
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-mem") == 0) {
            showmem = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-stats") == 0) {
            showstats = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-force") == 0) {
            VERBOSE_PRINT(("Forcing injection\n"));
            /* note this overrides INJECT_EXCLUDED */
            force_injection = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-no_env") == 0) {
            use_environment = FALSE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-env") == 0) {
            use_environment = TRUE;
            arg_offs += 1;
        } else if (strcmp(argv[arg_offs], "-ops") == 0) {
            if (argc <= arg_offs+1)
                return usage(argv[0]);
            ops_param = argv[arg_offs+1];
            arg_offs += 2;
        }
#endif /* ! EXTERNAL_INJECTOR */
        else {
            return usage(argv[0]);
        }
        if (limit < -1 && !limit_default) {
            return usage(argv[0]);
        }
        if (argc - arg_offs < 1) {
            return usage(argv[0]);
        }
    }

#ifndef EXTERNAL_INJECTOR
    if (inject) {
        if (argc - arg_offs < 2) {
            return usage(argv[0]);
        }
        /* we don't want quotes included in our dll path string or our
         * application path string (app_name), but we do want to put
         * quotes around every member of the command line
         */
        DYNAMORIO_PATH = strip_quotes(argv[arg_offs]);
        arg_offs += 1;
    }
#endif

    /* we don't want quotes included in our
     * application path string (app_name), but we do want to put
     * quotes around every member of the command line
     */
    app_name = strip_quotes(argv[arg_offs]);
    arg_offs += 1;

    /* note that we want target app name as part of cmd line
     * (FYI: if we were using WinMain, the pzsCmdLine passed in
     *  does not have our own app name in it)
     * since cygwin shell ends up putting extra quote characters, etc.
     * in pszCmdLine ('"foo"' => "\"foo\""), we can't easily walk past
     * the 1st 2 args (our path and the dll path), so we reconstruct
     * the cmd line from scratch:
     */
    ch = app_cmdline;
    ch += _snprintf(app_cmdline, BUFFER_SIZE_ELEMENTS(app_cmdline), "\"%s\"", 
                    app_name);
    for (i = arg_offs; i < argc; i++)
        ch += _snprintf(ch, 
                        BUFFER_SIZE_ELEMENTS(app_cmdline) - (ch - app_cmdline),
                        " \"%s\"", argv[i]);
    NULL_TERMINATE_BUFFER(app_cmdline);
    ASSERT(ch - app_cmdline < BUFFER_SIZE_ELEMENTS(app_cmdline));

#if HANDLE_CONTROL_C
    /* Note that one must call SetConsoleCtrlHandler(NULL, FALSE) to enable
     * ^C event reception: but under bash/rxvt it still won't work
     */
    if (!SetConsoleCtrlHandler(NULL, FALSE) ||
        !SetConsoleCtrlHandler(HandlerRoutine, TRUE)) {
        display_error("SetConsoleCtrlHandler failed");
        return 1;
    }
    {
        int flags;
        /* FIXME: hStdInput apparently is not the right handle...how do I get
         * the right handle?!?
         */
        if (!GetConsoleMode(mysi.hStdInput, &flags)) {
            display_error("GetConsoleMode failed");
            return 1;
        }
        printf("console mode flags are: "PFX"\n", flags);
    }
#endif

    _tsearchenv(app_name, _TEXT("PATH"), full_app_name);
    if (full_app_name[0] == _TEXT('\0')) {
        /* may need to append .exe, FIXME : other executable types */
        TCHAR tmp_buf[BUFFER_SIZE_ELEMENTS(app_name)+5];
        _sntprintf(tmp_buf, BUFFER_SIZE_ELEMENTS(tmp_buf), 
                   _TEXT("%s%s"), app_name, _TEXT(".exe"));
        NULL_TERMINATE_BUFFER(tmp_buf);
        VERBOSE_PRINT(("app name with exe %s\n", tmp_buf));
        _tsearchenv(tmp_buf, _TEXT("PATH"), full_app_name);
    }
    VERBOSE_PRINT(("app name %s, full app name %s\n", app_name, 
                   full_app_name));
    app_name = full_app_name;

    if (NULL != (li = ImageLoad(app_name, NULL))) {
        app_entry = (BYTE *)
            (li->MappedAddress +
             li->FileHeader->OptionalHeader.AddressOfEntryPoint);
        ImageUnload(li);
    } else {
        _snprintf(app_cmdline, BUFFER_SIZE_ELEMENTS(app_cmdline), 
                  "Failed to load executable image \"%s\"", app_name);
        NULL_TERMINATE_BUFFER(app_cmdline);
        display_error(app_cmdline);
        return 1;
    }

    DO_VERBOSE({
        printf("Running %s with cmdline \"%s\"\n", app_name, app_cmdline);
        fflush(stdout);
    });
    
    /* Launch the application process. */
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = mysi.hStdInput;
    si.hStdOutput = mysi.hStdOutput;
    si.hStdError = mysi.hStdError;

    image_name = get_image_name(app_name);

    /* FIXME, won't need to check this, or unset/restore debugger_key_injection
     * if we have our own version of CreateProcess that doesn't check the 
     * debugger key */
    debugger_key_injection = using_debugger_key_injection(image_name);

    /* set defaults for limit */
    if (limit_default) {
        if (debugger_key_injection)
            limit = -1; /* no wait */
        else 
            limit = 0; /* infinite wait */
    }
    
    DO_VERBOSE({
        printf("Using debug method? %s, debug_stop_function available %s\n", 
               debugger_key_injection ? "Yes" : "No", 
               debug_stop_function ? "Yes" : "No");
        fflush(stdout);
    });

#ifndef EXTERNAL_INJECTOR
    /* don't set registry from environment if using debug key */
    if (!debugger_key_injection) {
        set_registry_from_env(image_name, DYNAMORIO_PATH);
    }
#endif

    if (debugger_key_injection) {
        unset_debugger_key_injection();
    }

    DO_VERBOSE({
        printf("starting process\n");
        fflush(stdout);
    });

    /* Must specify TRUE for bInheritHandles so child inherits stdin! */
    res = CreateProcess(app_name, app_cmdline, NULL, NULL, TRUE,
                        CREATE_SUSPENDED + 
                        ((debug_stop_function && 
                          debugger_key_injection) ? DEBUG_PROCESS | 
                                                    DEBUG_ONLY_THIS_PROCESS 
                                                  : 0),
                        NULL, NULL, &si, &pi);

    DO_VERBOSE({
        printf("done starting process\n");
        fflush(stdout);
    });

    if (debugger_key_injection) {
        restore_debugger_key_injection(pi.dwProcessId, res);
    }
    
    if (!res) {
        display_error("Failed to launch application");
        goto error;
    }

    if ((phandle = OpenProcess(PROCESS_ALL_ACCESS,
                               FALSE, pi.dwProcessId)) == NULL) {
        display_error("Cannot open application process");
        TerminateProcess(pi.hProcess, 0);
        goto error;
    }

    /* force_injection prevents overriding of inject based on registry */
    if (inject && !force_injection) {
        int inject_flags = systemwide_should_inject(phandle, NULL);
        bool syswide_will_inject = systemwide_inject_enabled() && 
            TEST(INJECT_TRUE, inject_flags) && 
            !TEST(INJECT_EXPLICIT, inject_flags);
        bool should_not_takeover = TEST(INJECT_EXCLUDED, inject_flags) && 
            debugger_key_injection;
        /* case 10794: to support follow_children we inject even if
         * syswide_will_inject.  we use RUNUNDER_EXPLICIT to avoid
         * user32 injection from happening, to get consistent injection.
         * (if we didn't things would work but we'd have 
         * a warning "<Blocking load of module dynamorio.dll>" on the 2nd
         * injection)
         */
        inject = !should_not_takeover;
        if (!inject) {
            /* we should always be injecting (we set the registry above) 
             * unless we are using debugger_key_injection, in which 
             * case we use what is in the registry (whoever wrote the registry 
             * should take care of late or nonexistent user32 loading in the 
             * rununder value) */
            ASSERT(debugger_key_injection);
            display_error("application is excluded from injection\n");
        } else {
            /* Don't inject if in safe mode */
            if (is_safe_mode()) {
                inject = false;
                display_error("System is in safe mode, not injecting\n");
            }
        }
    }

    VERBOSE_PRINT(("%s in %s (%s)\n", inject ? "Injecting" : "Native", 
                   app_name, argv[0]));

#ifdef EXTERNAL_INJECTOR
    if (inject) {
        int err = get_process_parameter(phandle, L_DYNAMORIO_VAR_AUTOINJECT,
                                        library_path_buf, sizeof(library_path_buf));
        if (err != GET_PARAMETER_SUCCESS && err != GET_PARAMETER_NOAPPSPECIFIC) {
            inject = false;
            display_error("application not properly configured\n"
                          "use drdeploy.exe to configure the application first");
        }
        NULL_TERMINATE_BUFFER(library_path_buf);
        DYNAMORIO_PATH = library_path_buf;
    }
#endif

    if (inject) {
        inject_init();
        if (!inject_into_thread(phandle, &cxt, pi.hThread, DYNAMORIO_PATH)) {
            display_error("injection failed");
            close_handle(phandle);
            TerminateProcess(pi.hProcess, 0);
            goto error;
        }
        DO_VERBOSE({
            printf("Successful at inserting dynamo\n");
            fflush(stdout);
        });
    }

    success = FALSE;
    /* FIXME: indentation left from removing a try block */
        start_time = time(NULL);

        /* resume the suspended app process so its main thread can run */
        ResumeThread(pi.hThread);
    
        /* detach from the app process */
        close_handle(pi.hThread);
        close_handle(pi.hProcess);

        if (limit >= 0) {
            VERBOSE_PRINT(("Waiting for app to finish\n"));
            /* now wait for app process to finish */
            wait_result = WaitForSingleObject(phandle, (limit==0) ? INFINITE : (limit*1000));
            end_time = time(NULL);
            wallclock = difftime(end_time, start_time);
#ifndef EXTERNAL_INJECTOR
            if (!debugger_key_injection) {
                unset_registry_from_env(image_name);
            }
#endif
            if (wait_result == WAIT_OBJECT_0)
                success = TRUE;
            else
                fprintf(FP, "Timeout after %d seconds\n", limit);
        } else {
            /* if we are using env -> registry our changes won't get undone!
             * we can't unset now, the app may still reference them */
            success = TRUE;
        }

        /* FIXME: this is my attempt to get ^C, but it doesn't work... */
#if HANDLE_CONTROL_C
        /* should we register a control handler for this to work? */
        if (!success) {
            printf("Terminating child process!\n");
            fflush(stdout);
            TerminateProcess(phandle, 0);
        } else
            printf("Injector exiting peacefully\n");
#endif
#ifndef EXTERNAL_INJECTOR
        if (showmem || showstats)
            print_mem_stats(phandle);
#endif
        if (!success) {
            /* kill the child */
            TerminateProcess(phandle, 0);
        }
        close_handle(phandle);
        DO_VERBOSE({
            printf("All done\n");
            fflush(stdout);
        });

    return 0;

 error:
#ifndef EXTERNAL_INJECTOR
    if (!debugger_key_injection) {
        unset_registry_from_env(image_name);
    }
#endif
    return 1;
}
