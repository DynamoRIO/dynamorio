/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
 * Copyright (c) 2001-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2001 Hewlett-Packard Company */

/*
 * pre_inject.c - small shared library injected into every process
 *   checks to see if dynamo should be loaded in to take control
 *
 * DYNAMORIO_AUTOINJECT:
 *   points to the dynamorio.dll library to load
 *   (uses this instead of fixed path off DYNAMORIO_HOME to make
 *   it easy to switch between libraries used systemwide)
 *
 * see inject_shared.c for discussion of variables used to
 *  determine injection
 */

/*
 * N.B.: if using the user32 registry key to inject systemwide,
 * only routines from kernel32.dll may be called (not even ones from
 * user32.dll will work)
 */

#include "configure.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
/* we share ntdll.c w/ the main library */
#include "globals_shared.h"
#include "ntdll.h"
#include "inject_shared.h"
#include "drmarker.h"
#include "../config.h"

/* case 191729:
 * Avoid warning C4996: '_snwprintf' was declared deprecated
 * We do not want to use the suggested replacement _snwprintf_s as it comes
 * from msvcr80.dll (though we could statically link w/ the libc version
 * I suppose).
 */
#pragma warning(disable : 4996)

/* allow converting between data and function pointers */
#pragma warning(disable : 4055)

/* FIXME : assert stuff, internal error, display_message duplicated from
 * pre_inject, share? */

/* for asserts, copied from utils.h */
#ifdef assert
#    undef assert
#endif
/* avoid mistake of lower-case assert */
#define assert assert_no_good_use_ASSERT_instead
void
d_r_internal_error(char *file, int line, char *msg);
#ifdef DEBUG
#    ifdef INTERNAL
#        define ASSERT(x)                                   \
            if (!(x)) {                                     \
                d_r_internal_error(__FILE__, __LINE__, #x); \
            }
#    else
#        define ASSERT(x)                                   \
            if (!(x)) {                                     \
                d_r_internal_error(__FILE__, __LINE__, ""); \
            }
#    endif /* INTERNAL */
#else
#    define ASSERT(x) ((void)0)
#endif

/* in ntdll.c */
extern char *
get_application_name(void);
extern char *
get_application_pid(void);

/* for convenience, duplicated from utils.h, FIXME share */
#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

/* can only set to 1 for debug builds, unless also set in inject_shared.c */
/* must turn on VERBOSE in inject_shared.c as well since we're now
 * using display_verbose_message() -- FIXME: link them automatically */
#define VERBOSE 0

#if VERBOSE
/* in inject_shared.c: must turn on VERBOSE=1 there as well */
void
display_verbose_message(char *format, ...);
#    define VERBOSE_MESSAGE(...) display_verbose_message(__VA_ARGS__)
#else
#    define VERBOSE_MESSAGE(...)
#endif

static void
display_error_helper(wchar_t *msg)
{
    wchar_t title_buf[MAX_PATH + 64];
    _snwprintf(title_buf, BUFFER_SIZE_ELEMENTS(title_buf),
               L_PRODUCT_NAME L" Notice: %hs(%hs)", get_application_name(),
               get_application_pid());
    NULL_TERMINATE_BUFFER(title_buf);
    nt_messagebox(msg, title_buf);
}

void
d_r_internal_error(char *file, int line, char *expr)
{
#ifdef INTERNAL
#    define FILENAME_LENGTH L""
#else
    /* truncate file name to first character */
#    define FILENAME_LENGTH L".1"
#endif
    wchar_t buf[512];
    _snwprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
               L"Preinject Error %" FILENAME_LENGTH L"hs:%d %hs\n", file, line, expr);
    NULL_TERMINATE_BUFFER(buf);
    display_error_helper(buf);
    TerminateProcess(GetCurrentProcess(), (uint)-1);
}

#ifdef DEBUG
void
display_error(char *msg)
{
    wchar_t buf[512];
    _snwprintf(buf, BUFFER_SIZE_ELEMENTS(buf), L"%hs", msg);
    NULL_TERMINATE_BUFFER(buf);
    display_error_helper(buf);
}
#endif /* DEBUG */

typedef int (*int_func_t)();
typedef void (*void_func_t)();

/* in drlibc_x86.asm */
extern int
switch_modes_and_call(void_func_t func, void *arg1, void *arg2, void *arg3, void *arg4,
                      void *arg5, void *arg6);

static bool load_dynamorio_lib(IF_NOT_X64(bool x64_in_wow64))
{
    HMODULE dll = NULL;
    char path[MAX_PATH];
#ifdef DEBUG
    char msg[3 * MAX_PATH];
#endif
    int retval = -1; /* failure */
#ifndef X64
    bool wow64 = is_wow64_process(NT_CURRENT_PROCESS);
    if (x64_in_wow64) {
        ASSERT(wow64);
        retval = get_parameter_64(PARAM_STR(DYNAMORIO_VAR_AUTOINJECT), path, MAX_PATH);
    } else
#endif
        retval = d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_AUTOINJECT), path, MAX_PATH);
    if (IS_GET_PARAMETER_SUCCESS(retval)) {
        dr_marker_t mark;
        VERBOSE_MESSAGE("Loading \"%hs\"", path);
        /* The read_and_verify_dr_marker is the canonical check for dr in a
         * process, we double check against GetModuleHandle here just to be
         * extra safe (in case dr failed to initialize before).  Note that
         * GetModuleHandle won't find dr's dll if we implement certian -hide
         * or early_injection proposals. */
        if (read_and_verify_dr_marker(GetCurrentProcess(), &mark) != DR_MARKER_FOUND &&
            GetModuleHandle(DYNAMORIO_LIBRARY_NAME) == NULL
#ifndef X64    /* these ifdefs are rather ugly: just export all routines in x64 builds? */
            && /* check for 64-bit as well */
            (!wow64 ||
             read_and_verify_dr_marker_64(GetCurrentProcess(), &mark) != DR_MARKER_FOUND)
        /* FIXME PR 251677: need 64-bit early injection to fully test
         * read_and_verify_dr_marker_64
         */
#endif
        ) {
            /* OK really going to load dr now, verify that we are injecting
             * early enough (i.e. user32.dll is statically linked).  This
             * presumes preinject is only used with app_init injection which is
             * currently the case. FIXME - should we also check_sole_thread
             * here?  We can't really handle more then one thread when dr is
             * loading, but this can happen with early remote injected threads
             * many of which (CTRL) are relatively harmless.
             */
            LDR_MODULE *mod = get_ldr_module_by_name(L"user32.dll");
            ASSERT(mod != NULL);
            if (ldr_module_statically_linked(mod)) {
#ifndef X64
                if (x64_in_wow64)
                    dll = load_library_64(path);
                else
#endif
                    dll = LoadLibrary(path);
            } else {
                /* FIXME - would be really nice to communicate this back to
                 * the controller. */
#ifdef DEBUG
                _snprintf(msg, BUFFER_SIZE_ELEMENTS(msg),
                          PRODUCT_NAME
                          " Error: improper injection - " PRODUCT_NAME
                          " (%s) can't inject into process %s (%s) (user32.dll "
                          "not statically linked)\n",
                          path, get_application_name(), get_application_pid());
                NULL_TERMINATE_BUFFER(msg);
                display_error(msg);
#endif
            }
        } else {
            /* notify failure only in debug builds, otherwise just return */
#ifdef DEBUG
            /* with early injection this becomes even more likely */
            if (read_and_verify_dr_marker(GetCurrentProcess(), &mark) == DR_MARKER_FOUND
#    ifndef X64
                || (wow64 &&
                    read_and_verify_dr_marker_64(GetCurrentProcess(), &mark) ==
                        DR_MARKER_FOUND)
#    endif
            ) {
                /* ok, early injection should always beat this */
#    if VERBOSE
                /* can't readily tell what was expected */
                _snprintf(msg, BUFFER_SIZE_ELEMENTS(msg),
                          PRODUCT_NAME " ok if early injection, otherwise ERROR: "
                                       "double injection, " PRODUCT_NAME
                                       " (%s) is already loaded "
                                       "in process %s (%s), continuing\n",
                          path, get_application_name(), get_application_pid());
                NULL_TERMINATE_BUFFER(msg);
                display_error(msg);
#    endif /* VERBOSE */
            } else {
                /* if GetModuleHandle finds us but we don't have a marker
                 * we may have failed somehow */
                _snprintf(msg, BUFFER_SIZE_ELEMENTS(msg),
                          PRODUCT_NAME
                          " Error: failed injection, " PRODUCT_NAME " (%s) is "
                          "loaded but not initialized in process %s (%s), continuing\n",
                          path, get_application_name(), get_application_pid());
                NULL_TERMINATE_BUFFER(msg);
                display_error(msg);
            }
#endif /* DEBUG */
            return false;
        }
    } else
        path[0] = 0;
    if (dll == NULL) {
#ifdef DEBUG
        int err = GetLastError();
        _snprintf(msg, BUFFER_SIZE_ELEMENTS(msg), PRODUCT_NAME " Error %d loading %s\n",
                  err, path);
        NULL_TERMINATE_BUFFER(msg);
        display_error(msg);
#endif
        return false;
    } else {
        int_func_t init_func;
        void_func_t take_over_func;
        int res;
#ifndef X64
        if (x64_in_wow64) {
            init_func = (int_func_t)(ptr_uint_t) /*we know <4GB*/
                get_proc_address_64((uint64)dll, "dynamorio_app_init");
            take_over_func = (void_func_t)(ptr_uint_t) /*we know <4GB*/
                get_proc_address_64((uint64)dll, "dynamorio_app_take_over");
            VERBOSE_MESSAGE("dynamorio_app_init: 0x%08x; dynamorio_app_take_over: "
                            "0x%08x\n",
                            init_func, take_over_func);
        } else {
#endif
            init_func = (int_func_t)GetProcAddress(dll, "dynamorio_app_init");
            take_over_func = (void_func_t)GetProcAddress(dll, "dynamorio_app_take_over");
#ifndef X64
        }
#endif
        if (init_func == NULL || take_over_func == NULL) {
            /* unload the library so that it's clear DR is not in control
             * (o/w the DR library is in the process and it's not clear
             * what's going on)
             */
#ifndef X64
            if (x64_in_wow64) {
#    ifdef DEBUG
                bool ok =
#    endif
                    free_library_64(dll);
                ASSERT(ok);
            } else
#endif
                FreeLibrary(dll);
#ifdef DEBUG
            display_error("Error getting " PRODUCT_NAME " functions\n");
#endif
            return false;
        }
        VERBOSE_MESSAGE("about to inject dynamorio");
#ifndef X64
        if (x64_in_wow64)
            res = switch_modes_and_call(init_func, NULL, NULL, NULL, NULL, NULL, NULL);
        else
#endif
            res = (*init_func)();
        VERBOSE_MESSAGE("dynamorio_app_init() returned %d\n", res);
#ifndef X64
        if (x64_in_wow64)
            switch_modes_and_call(take_over_func, NULL, NULL, NULL, NULL, NULL, NULL);
        else
#endif
            (*take_over_func)();
        VERBOSE_MESSAGE("inside " PRODUCT_NAME " now\n");
    }
    return true;
}

static int parameters_present(IF_NOT_X64(bool x64_in_wow64))
{
    char path[MAX_PATH];
    int retval;

    /* We should do some sanity checking on our parameters,
       to make sure we can really inject in applications.
       War story: When renaming the product from DynamoRIO to SecureCore
       we'd start injecting and then failing to load a dll for all apps.
    */
#ifndef X64
    if (x64_in_wow64) {
        ASSERT(is_wow64_process(NT_CURRENT_PROCESS));
        retval = get_parameter_64(PARAM_STR(DYNAMORIO_VAR_AUTOINJECT), path, MAX_PATH);
    } else
#endif
        retval = d_r_get_parameter(PARAM_STR(DYNAMORIO_VAR_AUTOINJECT), path, MAX_PATH);
    if (IS_GET_PARAMETER_SUCCESS(retval)) {
        return 1;
    } else {
        return 0;
    }
}

/* forward declaration */
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved);

static bool
running_on_win8_or_later(void)
{
    PEB *peb = get_own_peb();
    return (peb->OSMajorVersion > 6 ||
            (peb->OSMajorVersion == 6 && peb->OSMinorVersion >= 2));
}

bool
process_attach()
{
    int rununder_mask;
    int should_inject;
    bool takeover = true;
#if VERBOSE
    int len;
    char exename[MAX_PATH];
#endif
    /* FIXME: append to event log to indicate we're in the address space */
    VERBOSE_MESSAGE("inside preinject dll\n");

    ntdll_init();
#ifndef PARAMS_IN_REGISTRY
    /* i#85/PR 212034: use config files */
    d_r_config_init();
#endif

#if VERBOSE
    len = GetModuleFileName(NULL, exename, MAX_PATH);
    ASSERT(len > 0);
#endif
#if 0 /* PR 314367: re-enable once it all works */
#    ifndef X64
    /* PR 253431: one method of injecting 64-bit DR into a WOW64 process is
     * via 32-bit AppInit drpreinject.
     * x64 configuration takes precedence over wow64.
     */
    if (is_wow64_process(NT_CURRENT_PROCESS)) {
        should_inject = systemwide_should_preinject_64(NULL, &rununder_mask);
        if (((INJECT_TRUE & should_inject) != 0) &&
            ((INJECT_EXPLICIT & should_inject) == 0) &&
            !is_safe_mode() &&
            parameters_present(true)) {
            VERBOSE_MESSAGE("<"PRODUCT_NAME" is taking over process %d (%s) as x64>\n",
                            GetCurrentProcessId(), exename);
            check_for_run_once(NULL, rununder_mask);
            /* we commit to x64 takeover based on there being a positive
             * rununder setting and an AUTOINJECT entry.  if the AUTOINJECT
             * turns out to be invalid, we'll try the 32-bit.
             */
            takeover = !load_dynamorio_lib(true);
        }
    }
#    endif
#endif /* 0 */
    if (takeover) {
        should_inject = systemwide_should_preinject(NULL, &rununder_mask);
        if (((INJECT_TRUE & should_inject) == 0) ||
            ((INJECT_EXPLICIT & should_inject) != 0) || is_safe_mode() ||
            !parameters_present(IF_NOT_X64(false))) {
            /* not taking over */
            VERBOSE_MESSAGE(PRODUCT_NAME " is NOT taking over process %d (%s)\n",
                            GetCurrentProcessId(), exename);
        } else {
            /* yes, load in dynamo to take over! */
            VERBOSE_MESSAGE("<" PRODUCT_NAME " is taking over process %d (%s)>\n",
                            GetCurrentProcessId(), exename);
            check_for_run_once(NULL, rununder_mask);
            load_dynamorio_lib(IF_NOT_X64(false));
        }
    }
    ntdll_exit();
    /* i#1522: self-unloading messes up the win8+ loader so we return false instead */
    if (running_on_win8_or_later())
        return false;
    else
        return true;
}

/* DLL entry point is in arch/pre_inject.asm */
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD reason_for_call, LPVOID Reserved);

/* A dummy exported routine just so the linker will give us an export directory
 * in the pe.  An export directory is needed to find the pe_name for a dll and
 * it's nice to be able to do so for at least our own dlls. This doesn't
 * increase the size of drpreinject.dll. */
__declspec(dllexport) void dr_dummy_function()
{
    /* nothing */
}
