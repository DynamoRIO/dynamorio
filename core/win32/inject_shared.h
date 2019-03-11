/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/*
 * inject_shared.h
 *
 * Prototypes of facilities shared between the core library, the preinject library
 * and the drinject executable.
 */

#ifndef INJECT_SHARED_H
#define INJECT_SHARED_H

/* return codes for systemwide_should_inject, see notes there for more info */
typedef enum {
    INJECT_FALSE = 0,
    INJECT_TRUE = 1,
    INJECT_EXCLUDED = 2,
    INJECT_EXPLICIT = 4
} inject_setting_mask_t;

/***************************************************************************/
#ifdef PARAMS_IN_REGISTRY
/* We've replaced the registry w/ config files (i#265/PR 486139, i#85/PR 212034)
 * but when PARAMS_IN_REGISTRY is defined we support the old registry scheme
 */

/* value is a buffer allocated by the caller to hold the resulting value.
 *
 * The same parameter is looked up first in the application specific registry
 * subtree and then in the global registry tree.
 */
int
get_process_parameter(HANDLE phandle, const wchar_t *name, char *value, int maxlen);

/* Only the process specific registry value is set.  */
int
set_process_parameter(HANDLE phandle, const wchar_t *name, const char *value);

/* Get parameter for current process, using registry view matching the
 * build of DR (so 64-bit DR looks at 64-bit registry, even for WO64 process).
 * If return value is not GET_PARAMETER_SUCCESS leaves original buffer contents
 * intact (may not be true if GET_PARAMETER_BUF_TOO_SMALL is returned).
 */
int
d_r_get_parameter(const wchar_t *name, char *value, int maxlen);

/* Identical to get_parameter: for compatibility w/ non-PARAMS_IN_REGISTRY */
int
get_parameter_ex(const wchar_t *name, char *value, int maxlen, bool ignore_cache);

#    ifdef X64
/* Get parameter for current process name from 32-bit registry view.
 * If return value is not GET_PARAMETER_SUCCESS leaves original buffer contents
 * intact (may not be true if GET_PARAMETER_BUF_TOO_SMALL is returned).
 */
int
get_parameter_32(const wchar_t *name, char *value, int maxlen);
#    else
/* Get parameter for current process name from 64-bit registry view.
 * If return value is not GET_PARAMETER_SUCCESS leaves original buffer contents
 * intact (may not be true if GET_PARAMETER_BUF_TOO_SMALL is returned).
 */
int
get_parameter_64(const wchar_t *name, char *value, int maxlen);
#    endif

/* Get parameter for current processes root app key (not qualified app key).
 * For ex. would get parameter from svchost.exe instead of svchost.exe-netsvc.
 * If return value is not GET_PARAMETER_SUCCESS leaves original buffer contents
 * intact (may not be true if GET_PARAMETER_BUF_TOO_SMALL is returned).
 */
int
get_unqualified_parameter(const wchar_t *name, char *value, int maxlen);

/***************************************************************************/
#else
int
get_parameter_from_registry(const wchar_t *name, char *value, /* OUT */
                            int maxlen /* up to MAX_REGISTRY_PARAMETER */);

#    ifndef NOT_DYNAMORIO_CORE
int
get_process_parameter(HANDLE phandle, const char *name, char *value, int maxlen);
#    endif

#    ifndef X64
int
get_parameter_64(const char *name, char *value, int maxlen);
#    endif
#endif /* PARAMS_IN_REGISTRY */
/***************************************************************************/

/* get_own_*_name routines cache their values and are primed by d_r_os_init() */
const wchar_t *
get_own_qualified_name(void);

const wchar_t *
get_own_unqualified_name(void);

const wchar_t *
get_own_short_qualified_name(void);

const wchar_t *
get_own_short_unqualified_name(void);

bool
is_safe_mode(void);

bool
systemwide_inject_enabled(void);

void
check_for_run_once(HANDLE process, int rununder_mask);

#ifndef X64
inject_setting_mask_t
systemwide_should_preinject_64(HANDLE process, int *mask);
#endif

/* this is being used to mean "should_inject", ignoring systemwide */
inject_setting_mask_t
systemwide_should_inject(HANDLE process, int *mask);

/* this ignores 1config files */
inject_setting_mask_t
systemwide_should_preinject(HANDLE process, int *mask);

int
get_remote_process_ldr_status(HANDLE process_handle);

/* utility functions */
const char *
double_strrchr(const char *string, char c1, char c2);

const wchar_t *
double_wcsrchr(const wchar_t *string, wchar_t c1, wchar_t c2);

const wchar_t *
w_get_short_name(const wchar_t *exename);

/* in case we ever want to build as unicode app, normally set by the compiler,
 * if we want to build unicode we will have to set, however, since we
 * preprocess with gcc, this is the define used in the windows headers */
#ifdef _UNICODE
#    define double_tcsrchr double_wcsrchr
#else
#    define double_tcsrchr double_strrchr
#endif

int
wchar_to_char(char *cdst, size_t buflen, PCWSTR wide_src, size_t bytelen);

void
display_message(char *msg, int wait_for_user);

#endif
