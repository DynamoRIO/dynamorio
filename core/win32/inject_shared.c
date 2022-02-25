/* **********************************************************
 * Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
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
 * inject_shared.c
 *
 * Facilities shared between the core library, the preinject library
 * and the drinject executable.
 *
 * getting parameters from registry keys
 * rununderdr/runall logic
 * and some debugging support
 *
 */

/* for policy.dll linkage of qualified name utils */
#include "configure.h"
#ifndef NOT_DYNAMORIO_CORE

#    include "../globals.h"

#    include <windows.h>
#    include <tchar.h>
#    include <stdio.h>
#    include "ntdll.h" /* should link with ntdll.o */
#    ifdef DEBUG
#        include "../moduledb.h" /* for macros for 9252 fix. */
#    endif

/* for asserts, copied from utils.h */
#    ifdef assert
#        undef assert
#    endif
/* avoid mistake of lower-case assert */
#    define assert assert_no_good_use_ASSERT_instead
extern void
d_r_internal_error(const char *file, int line, const char *msg);
#    ifdef DEBUG
extern void
display_error(char *msg);
#        ifdef NOT_DYNAMORIO_CORE_PROPER /* Part of case 9252 fix. */
#            define display_warning display_error
#            ifdef ASSERT
#                undef ASSERT
#            endif
#            ifdef INTERNAL
#                define ASSERT(x) \
                    if (!(x))     \
                    d_r_internal_error(__FILE__, __LINE__, #x)
#            else
#                define ASSERT(x) \
                    if (!(x))     \
                    d_r_internal_error(__FILE__, __LINE__, "")
#            endif /* INTERNAL */
#        elif !defined(NOT_DYNAMORIO_CORE)
#            define display_warning SYSLOG_INTERNAL_WARNING
#        endif /* !NOT_DYNAMORIO_CORE_PROPER */
#    else
#        define ASSERT(x) ((void)0)
#        define display_error(msg)
#        define display_warning(msg)
#    endif

#    define MAX_RUNVALUE_LENGTH 12 /* 1 for sign, 10 digits and a \0 */
#    ifdef NOT_DYNAMORIO_CORE_PROPER
#        define VERBOSE 0
#    else
#        define VERBOSE 0
#    endif

typedef enum {
    REGISTRY_DEFAULT,
    /* These apply only to 64-bit Windows and only matter for WOW64 */
    REGISTRY_32, /* Look in 32-bit WOW64 registry settings */
    REGISTRY_64, /* Look in 64-bit registry settings */
} reg_platform_t;

#    if defined(NOT_DYNAMORIO_CORE_PROPER) && defined(DEBUG)
/* for ASSERT_CURIOSITY as defined in utils.h */
bool
ignore_assert(const char *assert_stmt, const char *expr)
{
    return false;
}

void
report_dynamorio_problem(dcontext_t *unused_dcontext, uint unused_dumpcore_flag,
                         app_pc unused_exception_addr, app_pc unused_report_ebp,
                         const char *unused_fmt, ...)
{
    /* FIXME: not supporting here - cannot print the message but
     * rather its format string */
    display_error("ASSERT_CURIOSITY hit - attach a debugger\n");
}
#    endif

#    if VERBOSE
/* display_verbose_message also used by pre_inject */

/* Have to reinvent the wheel, unfortunately, since pre_inject does not link
 * utils.c.  Only need this to be able to get the app name for the
 * display_verbose_message title, but that's nice to have.
 */
static void
notcore_mutex_lock(int *thelock)
{
    while (_InterlockedExchange((LONG *)thelock, 1) != 0) {
        _mm_pause();
    }
}

static void
notcore_mutex_unlock(int *thelock)
{
    *thelock = 0;
}

void
display_verbose_message(char *format, ...)
{
    char char_msg_buf[512];
    wchar_t msg_buf[512];
    static bool title_set = false;
    static bool title_set_in_progress = false;
    static wchar_t title_buf[MAX_PATH + 64];
    static int title_lock;
    static uint title_sz;
    wchar_t *title = title_buf;
    uint msg_sz;
    size_t written;
    va_list ap;
    va_start(ap, format);
    if (!title_set) {
        /* avoid recursive infinite loop of get_application_name()'s registry
         * reads calling this routine
         */
        if (title_set_in_progress) {
            /* we ourselves hold the lock, just use const for this msg */
            title = L"<title set in progress>";
        } else {
            notcore_mutex_lock(&title_lock);
            if (!title_set) { /* handle race */
                title_set_in_progress = true;
                title_sz = _snwprintf(title_buf, BUFFER_SIZE_ELEMENTS(title_buf),
                                      L_PRODUCT_NAME L" Notice: %hs(%hs)",
                                      get_application_name(), get_application_pid());
                NULL_TERMINATE_BUFFER(title_buf);
                if (title_sz < 0) /* at max count: print all but NULL to stderr */
                    title_sz = BUFFER_SIZE_ELEMENTS(title_buf) - 1;
                title_set_in_progress = false;
                title_set = true;
            }
            notcore_mutex_unlock(&title_lock);
        }
    }
    vsnprintf(char_msg_buf, BUFFER_SIZE_ELEMENTS(char_msg_buf), format, ap);
    NULL_TERMINATE_BUFFER(char_msg_buf);
    msg_sz = _snwprintf(msg_buf, BUFFER_SIZE_ELEMENTS(msg_buf), L"%hs", char_msg_buf);
    NULL_TERMINATE_BUFFER(msg_buf);
    if (msg_sz < 0) /* at max count: print all but NULL to stderr */
        msg_sz = BUFFER_SIZE_ELEMENTS(msg_buf) - 1;
    /* stderr and messagebox */
    write_file(STDERR, title_buf, title_sz * sizeof(title_buf[0]), NULL, &written);
    write_file(STDERR, msg_buf, msg_sz * sizeof(msg_buf[0]), NULL, &written);
    write_file(STDERR, "\n", sizeof("\n"), NULL, &written);
    nt_messagebox(msg_buf, title);
}
#    endif

/* returns pointer to last char of string that matches either c1 or c2
 * or NULL if can't find */
const char *
double_strrchr(const char *string, char c1, char c2)
{
    const char *ret = NULL;
    while (*string != '\0') {
        if (*string == c1 || *string == c2) {
            ret = string;
        }
        string++;
    }
    return ret;
}

int
wchar_to_char(char *cdst, size_t buflen, PCWSTR wide_src, size_t bytelen)
{
    int res;
    ssize_t wlen = utf16_to_utf8_size(wide_src, buflen, NULL);
    if (wlen < 0 || (size_t)wlen >= buflen) { /* wide string length + NULL */
        cdst[0] = '\0';
        return 0;
    }

    res = snprintf(cdst, buflen, "%.*ls", wlen, wide_src);
    cdst[wlen] = '\0';             /* always NULL terminate */
    ASSERT(strlen(cdst) < buflen); /* off by one, lets us see if we're pushing it */
    return res;
}

/* Description: Sets the value data for a value name that belongs to a given
 *              registry key.
 * Input:       keyname - fully qualified name of the registry key to
 *                        which valuename belongs to.
 *              valuename - name of the value for which the data is to be set.
 *              value - data that is to be set for valuename.
 * Output:      None.
 * Return value:SET_PARAMETER_SUCCESS if valuename is changed/created.
 *              SET_PARAMETER_FAILURE if keyname is invalid or if valuename
 *              can't be changed or created.
 * Notes:       If valuename doesn't exist, it will be created with value as
 *              data.  Implemented as part of case 3702.
 */
static int
set_registry_parameter(const wchar_t *keyname, const wchar_t *valuename,
                       const char *value)
{
    HANDLE hkey;
    int size, res;

    wchar_t wvalue[MAX_REGISTRY_PARAMETER];

    size = _snwprintf(wvalue, BUFFER_SIZE_ELEMENTS(wvalue), L"%S", value);
    NULL_TERMINATE_BUFFER(wvalue);
    ASSERT(size >= 0 && size < BUFFER_SIZE_ELEMENTS(wvalue));

    hkey = reg_open_key(keyname, KEY_SET_VALUE);
    if (NULL != hkey) {
        res = reg_set_key_value(hkey, valuename, wvalue);
        if (res) {
            /* Need to flush registry writes to disk, otherwise a
             * power cycle will throw out the changes.  See case 4138.
             */
            reg_flush_key(hkey);
            reg_close_key(hkey);
            return SET_PARAMETER_SUCCESS;
        }
        reg_close_key(hkey);
    }
    return SET_PARAMETER_FAILURE;
}

static int
get_registry_parameter(PCWSTR keyname, PCWSTR valuename, char *value, /* OUT */
                       int maxlen /* up to MAX_REGISTRY_PARAMETER */,
                       reg_platform_t whichreg)
{
    int retval = GET_PARAMETER_FAILURE;
    KEY_VALUE_PARTIAL_INFORMATION *kvpi;
    reg_query_value_result_t result;
    /* we could probably get rid of this buffer by using the callers buffer
     * though would be kind of ugly for the caller */
    char buf_array[sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                   sizeof(wchar_t) * (MAX_REGISTRY_PARAMETER + 1)]; // wide

    /* For injectors and for all core registry reads (except process control
     * hash lists) use the local array; for process control hash lists use a
     * dynamic buffer.  This messy code was needed to handle case 9252.
     */
    char *buf = (char *)&buf_array;
    int alloc_size = sizeof(buf_array);
#    if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    /* Only for core; injectors shouldn't use this and don't have heap mgt.
     * Even if core is using, can't use this till heap is initialized. */
    if (maxlen > MAX_REGISTRY_PARAMETER && dynamo_heap_initialized) {
        /* Only process control hashlist reads may read more chars than
         * MAX_REGISTRY_PARAMETER.  Case 9252.
         */
#        ifdef PROCESS_CONTROL
        ASSERT(IS_PROCESS_CONTROL_ON() &&
               maxlen == (int)(DYNAMO_OPTION(pc_num_hashes) * (MD5_STRING_LENGTH + 1)));
#        endif
        /* Registry takes wchar buf so can't use maxlen directly to allocate. */
        alloc_size =
            sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(wchar_t) * (maxlen + 1);
        buf = heap_alloc(GLOBAL_DCONTEXT, alloc_size HEAPACCT(ACCT_OTHER));
    }
#    else  /* NOT_DYNAMORIO_CORE_PROPER || NOT_DYNAMORIO_CORE */
    ASSERT(maxlen <= MAX_REGISTRY_PARAMETER);
#    endif /* !NOT_DYNAMORIO_CORE_PROPER && !NOT_DYNAMORIO_CORE */

    kvpi = (KEY_VALUE_PARTIAL_INFORMATION *)buf;

    result = reg_query_value(
        keyname, valuename, KeyValuePartialInformation, kvpi, alloc_size,
        whichreg == REGISTRY_64 ? KEY_WOW64_64KEY
                                : (whichreg == REGISTRY_32 ? KEY_WOW64_32KEY : 0));
    if (result == REG_QUERY_SUCCESS) {
        snprintf(value, maxlen - 1, "%*ls", kvpi->DataLength / sizeof(wchar_t) - 1,
                 (wchar_t *)kvpi->Data);
        value[maxlen - 1] = '\0'; /* make sure it is terminated */
#    if VERBOSE
        display_verbose_message("got registry value of %hs for value %ls in key %ls",
                                value, valuename, keyname);
#    endif VERBOSE
        retval = GET_PARAMETER_SUCCESS;
    }
#    if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    else if (result == REG_QUERY_BUFFER_TOO_SMALL) {
        /* reuse buf */
        snprintf(buf, alloc_size, "%ls - %ls", keyname, valuename);
        buf[(alloc_size / sizeof(wchar_t)) - 1] = '\0';
        retval = GET_PARAMETER_BUF_TOO_SMALL;
        /* we might be reading the option string right now so don't synch */
        SYSLOG_NO_OPTION_SYNCH(SYSLOG_ERROR, ERROR_REGISTRY_PARAMETER_TOO_LONG, 3,
                               get_application_name(), get_application_pid(), buf);
    }
#    endif

#    if VERBOSE
    if (IS_GET_PARAMETER_FAILURE(retval)) {
        display_verbose_message("didn't get registry value %ls in key %ls", valuename,
                                keyname);
    }
#    endif /* VERBOSE */

#    if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
    if (maxlen > MAX_REGISTRY_PARAMETER && dynamo_heap_initialized) /* case 9252 */
        heap_free(GLOBAL_DCONTEXT, buf, alloc_size HEAPACCT(ACCT_OTHER));
#    endif /* !NOT_DYNAMORIO_CORE_PROPER && !NOT_DYNAMORIO_CORE */
    return retval;
}

/* reads remote process PEB.Ldr field
 * returns
 *   -1 if can't read remote process,
 *    0 if PEB.Ldr is NULL
 *    1 if PEB.Ldr has been initialized
 */
int
get_remote_process_ldr_status(HANDLE process_handle)
{
    size_t nbytes;
    size_t res;
    PEB peb;

    LPVOID peb_base = get_peb(process_handle);

    /* Read process PEB */
    res = nt_read_virtual_memory(process_handle, (LPVOID)peb_base, &peb, sizeof(peb),
                                 &nbytes);
    if (!res) {
        /* xref case 9800 - the app handle may not always have sufficient rights
         * FIXME - could dup the handle and retry */
        return -1;
    }

    if (peb.LoaderData != NULL) {
        return 1; /* already created process */
    } else {
        return 0; /* new process */
    }
}

static bool
is_windows_version_vista_plus(void); /* forward decl */

/*
 * this assumes it will be called on process initialization, when
 *  the PEB apparently uses offsets in the ProcessParameters block
 *  instead of direct pointers.
 * image name and cmdline combined into one call to reduce
 *  read process memory calls (whether this is actually true depends on
 *  usage)
 * Handles both 32-bit and 64-bit remote processes.
 */
void
get_process_imgname_cmdline(HANDLE process_handle, wchar_t *image_name,
                            int max_image_wchars, wchar_t *command_line,
                            int max_cmdl_wchars)
{
    size_t nbytes;
    int res;
    int len;
    /* For a 64-bit parent querying a 32-bit remote we assume we'll get back the
     * 64-bit WOW64 PEB.
     */
    uint64 peb_base = get_peb_maybe64(process_handle);
    union {
        uint64 params_ptr_64;
        uint params_ptr_32;
    } params_ptr;
    bool peb_is_32 = IF_X64_ELSE(false, is_32bit_process(process_handle));
    uint64 param_location;
    /* It is supposed to be at process_parameters.ImagePathName.Buffer */
    RTL_USER_PROCESS_PARAMETERS params = { 0 };
#    ifndef X64
    RTL_USER_PROCESS_PARAMETERS_64 params64 = { 0 };
#    endif

    if (image_name != NULL)
        image_name[0] = L'\0';
    if (command_line != NULL)
        command_line[0] = L'\0';

    /* Read process PEB */
    res = read_remote_memory_maybe64(
        process_handle,
        peb_base +
            (peb_is_32 ? X86_PROCESS_PARAM_PEB_OFFSET : X64_PROCESS_PARAM_PEB_OFFSET),
        &params_ptr, sizeof(params_ptr), &nbytes);
    if (!res || nbytes != sizeof(params_ptr)) {
        display_error("Warning: could not read process memory!");
        return;
    }

    /* Follow on to process parameters */
    uint64 params_base = peb_is_32 ? params_ptr.params_ptr_32 : params_ptr.params_ptr_64;
    res = read_remote_memory_maybe64(
        process_handle, params_base,
        IF_NOT_X64(!peb_is_32 ? (void *)&params64 :)(void *) & params,
        IF_NOT_X64(!peb_is_32 ? sizeof(params64) :) sizeof(params), &nbytes);
    if (!res || nbytes != (IF_NOT_X64(!peb_is_32 ? sizeof(params64) :) sizeof(params))) {
        display_error("Warning: could not read process memory!");
        return;
    }

    /* apparently {ImagePathName,CommandLine}.Buffer contains the offset
     * from the beginning of the ProcessParameters structure during
     * process init on os versions prior to Vista */

    if (image_name) {
        if (is_windows_version_vista_plus()) {
            param_location = IF_NOT_X64(!peb_is_32 ? params64.ImagePathName.u.Buffer64
                                                   :)(uint64) params.ImagePathName.Buffer;
        } else {
            param_location = IF_NOT_X64(!peb_is_32 ? params64.ImagePathName.u.Buffer64 :)(
                (uint64)params.ImagePathName.Buffer + params_base);
        }

        len = IF_NOT_X64(!peb_is_32 ? params64.ImagePathName.Length :)
                  params.ImagePathName.Length;
        if (len > 2 * (max_image_wchars - 1))
            len = 2 * (max_image_wchars - 1);

        /* Read the image file name in our memory too */
        res = read_remote_memory_maybe64(process_handle, param_location, image_name, len,
                                         &nbytes);
        if (!res) {
            len = 0;
            display_warning("Warning: could not read image name from PEB");
        }
        image_name[len / 2] = 0;
    }

    if (command_line) {
        if (is_windows_version_vista_plus()) {
            param_location = IF_NOT_X64(!peb_is_32 ? params64.CommandLine.u.Buffer64
                                                   :)(uint64) params.CommandLine.Buffer;
        } else {
            param_location = IF_NOT_X64(!peb_is_32 ? params64.CommandLine.u.Buffer64 :)(
                uint64)(params.CommandLine.Buffer + params_base);
        }

        len = IF_NOT_X64(!peb_is_32 ? params64.CommandLine.Length :)
                  params.CommandLine.Length;
        if (len > 2 * (max_cmdl_wchars - 1))
            len = 2 * (max_cmdl_wchars - 1);

        /* Read the image file name in our memory too */
        res = read_remote_memory_maybe64(process_handle, param_location, command_line,
                                         len, &nbytes);
        if (!res) {
            len = 0;
            display_warning("Warning: could not read cmdline from PEB");
        }
        command_line[len / 2] = 0;
    }

    return;
}

static inline int
get_rununder_value(const char *runvalue)
{
    /* For now we allow only decimal, but with more flags it will be
       easier to work on hex.
       FIXME: share the logic in parse_uint() from after options.c -r 1.4
       to allow both hex and decimal values
    */
    return atoi(runvalue);
}

#else /* !defined(NOT_DYNAMORIO_CORE) */

/* need some definitions for get_commandline_qualifier below */
#    include <windows.h>
#    include <globals_shared.h>

#    define DIRSEP '\\'
#    define ALT_DIRSEP '/'
#endif

/* shared utilities */
/* unicode version of double_strrchr */
const wchar_t *
double_wcsrchr(const wchar_t *string, wchar_t c1, wchar_t c2)
{
    const wchar_t *ret = NULL;
    while (*string != L'\0') {
        if (*string == c1 || *string == c2) {
            ret = string;
        }
        string++;
    }
    return ret;
}

const wchar_t *
w_get_short_name(const wchar_t *exename)
{
    const wchar_t *exe;
    exe = double_wcsrchr(exename, L_EXPAND_LEVEL(DIRSEP), L_EXPAND_LEVEL(ALT_DIRSEP));
    if (exe == NULL)
        exe = exename;
    else
        exe++; /* skip (back)slash */
    return exe;
}

/*
   We derive an application specific name to differentiate instances
   based on the canonicalized commandline of the process.
   Originally we did that only for svchost, but now it is for anything
     marked with RUNUNDER_COMMANDLINE_DISPATCH.
   The current scheme asks for adding all alphanumeric characters from
   the original commandline after skipping the executable name itself.

   If no_strip (e.g. RUNUNDER_COMMANDLINE_NO_STRIP is set) then the
   first argument on the commandline is not stripped.  This is for
   backwards compatibility where we stripped the -k argument in the
   svchost groups, e.g. 'svchost -k rpcss'

   max_derived_length is in number of elements, so most callers should
   simply use BUFFER_SIZE_ELEMENTS on the buffer passed as derived_name

    Returns 1 if group command line qualifier present
      (normally should be 1 if called for matching executables,
      but on an empty commandline we do return 0)
*/
bool
get_commandline_qualifier(const wchar_t *command_line, wchar_t *derived_name,
                          uint max_derived_length /* elements */, bool no_strip)
{
    wchar_t *derived_ptr = derived_name;
    wchar_t *derived_end = derived_name + max_derived_length - 1; /* last usable char */

    /* Find last piece of the executable name  */
    const wchar_t *cmdptr;

    /* Long paths (that may have spaces) are assumed to be in quotes on command line */
    if (command_line[0] == L'"') {
        cmdptr = wcschr(command_line + 1, L'"');
        if (cmdptr)
            cmdptr++;
    } else {
        cmdptr = wcschr(command_line, L' ');
    }

    if (!cmdptr) {
        *derived_name = L'\0';
        return 0;
    }

    do {
        /* Skip any leading delimiters before each argument, e.g.
         * "svchost.exe   -k          netsvcs"
         */
        while (*cmdptr && !iswalnum(*cmdptr))
            cmdptr++;
        if (!*cmdptr)
            break;

        /* Skip sequence of alphanums unless no_strip */
        if (!no_strip) {
            while (*cmdptr && iswalnum(*cmdptr))
                cmdptr++;
            no_strip = true;
            if (!*cmdptr)
                break;
        }

        /* copy out all valid characters */
        while (*cmdptr && iswalnum(*cmdptr)) {
            if (derived_ptr == derived_end)
                goto out;
            *derived_ptr++ = *cmdptr++;
        }

        /* We do not add any normalized delimiters, e.g. "/t /e /st" is the same as
         * "/test", since currently there is no need to be that punctual.
         */
    } while (*cmdptr);
out:
    *derived_ptr = L'\0'; /* NULL terminate */
    if (derived_ptr == derived_name)
        return 0; /* no commandline given */
    else
        return 1;
}

/* for policy.dll linkage of qualified name utils */
#ifndef NOT_DYNAMORIO_CORE

/* types for get_process_qualified_name results */
typedef enum {
    QUALIFIED_FULL_NAME,
    QUALIFIED_SHORT_NAME,
    UNQUALIFIED_FULL_NAME,
    UNQUALIFIED_SHORT_NAME,
} qualified_name_type_t;

#    define NAME_TYPE_IS_UNQUALIFIED(name_type) \
        (name_type == UNQUALIFIED_FULL_NAME || name_type == UNQUALIFIED_SHORT_NAME)
#    define NAME_TYPE_IS_SHORT(name_type) \
        (name_type == QUALIFIED_SHORT_NAME || name_type == UNQUALIFIED_SHORT_NAME)

/*
  We test the rununder_mask of a process and if it specifies that a
  fully qualified name is needed we lookup at the fully qualified
  location first.

  See comments in get_commandline_qualifier() and case 1324 for a list
  of executables which we further distinguish based on commandline.

  If we hardcode only a few more entries we could then avoid this
  buffer and registry access altogether, yet for generality sake
  we'll do this.

  FIXME[inefficiency]: The systemwide_should_inject() thus ends up
     checking twice for RUNUNDERDR, but the OS is good at caching this.
*/
static uint
commandline_qualifier_needed(const wchar_t *process_short_name, reg_platform_t whichreg)
{
    char runvalue[MAX_RUNVALUE_LENGTH];
    uint ret_val = 0;
    int res;

    wchar_t app_specific_base[MAXIMUM_PATH] = DYNAMORIO_REGISTRY_BASE L"\\";
    /* FIXME: this extra buffer has exactly the same contents as that
       passed by get_subkey_parameter(,,QUALIFIED_SHORT_NAME) and in
       fact have the same contents as we'll now prepare in a different buffer.
       Other callers of get_process_qualified_name() do not have an
       extra buffer information prepared. For now we'll allocate another buffer.
     */

    /* We now need to use direct registry access to get RUNUNDER flags */
    wcsncat(app_specific_base, process_short_name, BUFFER_ROOM_LEFT_W(app_specific_base));
    NULL_TERMINATE_BUFFER(app_specific_base);
    res = get_registry_parameter(app_specific_base, L_DYNAMORIO_VAR_RUNUNDER, runvalue,
                                 sizeof(runvalue), whichreg);
    if (IS_GET_PARAMETER_SUCCESS(res)) {
        ret_val = get_rununder_value(runvalue) &
            (RUNUNDER_COMMANDLINE_DISPATCH | RUNUNDER_COMMANDLINE_NO_STRIP);
    }

    return ret_val;
}

/* Returns the executable image path appended with the command line qualifier (if
 * requested in name_type) into the user provided buffer.  If short is requested in
 * name_type only the executable name portion is added, otherwise the full path is used.
 *
 * max_exename_length is in number of elements, so most callers should
 * simply use BUFFER_SIZE_ELEMENTS on the buffer passed as w_exename
 *
 * If process_handle is NULL we read from the local PEB entries. */
static void
get_process_qualified_name(HANDLE process_handle, wchar_t *w_exename,
                           size_t max_exename_length, qualified_name_type_t name_type,
                           reg_platform_t whichreg)
{
    const wchar_t *full_name;
    const wchar_t *short_name;
    uint commandline_dispatch = 0;

    PEB *own_peb;
    wchar_t other_process_img_or_cmd[MAXIMUM_PATH];

    /* FIXME: This buffer is only needed for reading other process
       data, we need to check stack depths for the follow children
       case.  Although not needed when reading current process this
       function should be called only at startup with known stack layout.
    */

    if (process_handle == NULL) {
        /* get our own subkey */
        own_peb = get_own_peb();
        ASSERT(own_peb && own_peb->ProcessParameters);
        ASSERT(own_peb->ProcessParameters->ImagePathName.Buffer);
        full_name = get_process_param_buf(
            own_peb->ProcessParameters, own_peb->ProcessParameters->ImagePathName.Buffer);
    } else {
        own_peb = NULL;
        /* get foreign process subkey */
        /* to avoid another buffer and save stack space, we do this in stages */
        /* just get image name first */
        get_process_imgname_cmdline(
            process_handle, other_process_img_or_cmd /* image name */,
            BUFFER_SIZE_ELEMENTS(other_process_img_or_cmd), NULL, 0);
        full_name = other_process_img_or_cmd;
    }

    /* CHECK: can we safely assume that all UNICODE_STRINGs we read do have a final 0? */
    short_name = w_get_short_name(full_name);
    wcsncpy(w_exename, (NAME_TYPE_IS_SHORT(name_type)) ? short_name : full_name,
            max_exename_length);
    w_exename[max_exename_length - 1] = 0; /* always NULL terminate */

    if (NAME_TYPE_IS_UNQUALIFIED(name_type)) {
        /* off by one, lets us see if we're pushing it */
        ASSERT_CURIOSITY(wcslen(w_exename) < max_exename_length - 1);
        return;
    }

    commandline_dispatch = commandline_qualifier_needed(short_name, whichreg);
    if (TEST(RUNUNDER_COMMANDLINE_DISPATCH, commandline_dispatch)) {
        wchar_t cmdline_qualifier[MAXIMUM_PATH] = L"-";
        /* FIXME: we could do all this processing in w_exename so that
           no other buffer is needed at all, but for the sake of
           readability keeping this extra */
        const wchar_t *process_commandline;

        if (process_handle == NULL) {
            /* get our own commandline */
            ASSERT(own_peb->ProcessParameters->CommandLine.Buffer);
            process_commandline =
                get_process_param_buf(own_peb->ProcessParameters,
                                      own_peb->ProcessParameters->CommandLine.Buffer);
        } else {
            /* get only command line from other process */
            get_process_imgname_cmdline(process_handle, NULL, 0, other_process_img_or_cmd,
                                        BUFFER_SIZE_ELEMENTS(other_process_img_or_cmd));
            process_commandline = other_process_img_or_cmd;
        }

        get_commandline_qualifier(
            process_commandline, cmdline_qualifier + 1, /* skip the '-' */
            BUFFER_SIZE_ELEMENTS(cmdline_qualifier) - 1,
            TEST(RUNUNDER_COMMANDLINE_NO_STRIP, commandline_dispatch));

        /* append "qualifier" which already has a '-' and may in fact be only '-' if
         * no qualifier was found (we still want the '-' to separate out the registry
         * entries xref 9119) */
        wcsncat(w_exename, cmdline_qualifier, max_exename_length - wcslen(w_exename) - 1);
    }
    w_exename[max_exename_length - 1] = 0; /* always NULL terminate */
    /* off by one, lets us see if we're pushing it */
    ASSERT_CURIOSITY(wcslen(w_exename) < max_exename_length - 1);
}

/* NOTE - get_own_*_name routines cache their values and are primed by d_r_os_init() since
 * it might not be safe to read the process parameters later. */

/* Returns the cached full path of the image, including the command line qualifier when
 * necessary */
const wchar_t *
get_own_qualified_name()
{
    static wchar_t full_qualified_name[MAXIMUM_PATH];

    if (full_qualified_name[0] == L'\0') {
        get_process_qualified_name(NULL, full_qualified_name,
                                   BUFFER_SIZE_ELEMENTS(full_qualified_name),
                                   QUALIFIED_FULL_NAME, REGISTRY_DEFAULT);
        ASSERT(full_qualified_name[0] != L'\0');
    }
    return full_qualified_name;
}

/* Returns the cached full path of the image with no qualifiers */
const wchar_t *
get_own_unqualified_name()
{
    static wchar_t full_unqualified_name[MAXIMUM_PATH];

    if (full_unqualified_name[0] == L'\0') {
        get_process_qualified_name(NULL, full_unqualified_name,
                                   BUFFER_SIZE_ELEMENTS(full_unqualified_name),
                                   UNQUALIFIED_FULL_NAME, REGISTRY_DEFAULT);
        ASSERT(full_unqualified_name[0] != L'\0');
    }
    return full_unqualified_name;
}

/* Returns the cached short image name, including the command line qualifier when
 * necessary */
const wchar_t *
get_own_short_qualified_name()
{
    static wchar_t short_qualified_name[MAXIMUM_PATH];

    if (!short_qualified_name[0]) {
        get_process_qualified_name(NULL, short_qualified_name,
                                   BUFFER_SIZE_ELEMENTS(short_qualified_name),
                                   QUALIFIED_SHORT_NAME, REGISTRY_DEFAULT);
        ASSERT(short_qualified_name[0]);
    }
    return short_qualified_name;
}

/* Returns the cached short image name with no qualifiers */
const wchar_t *
get_own_short_unqualified_name()
{
    static wchar_t short_unqualified_name[MAXIMUM_PATH];

    if (!short_unqualified_name[0]) {
        get_process_qualified_name(NULL, short_unqualified_name,
                                   BUFFER_SIZE_ELEMENTS(short_unqualified_name),
                                   UNQUALIFIED_SHORT_NAME, REGISTRY_DEFAULT);
        ASSERT(short_unqualified_name[0]);
    }
    return short_unqualified_name;
}

/***************************************************************************/
#    ifdef PARAMS_IN_REGISTRY
/* We've replaced the registry w/ config files (i#265/PR 486139, i#85/PR 212034)
 * but when PARAMS_IN_REGISTRY is defined we support the old registry scheme
 */

static int
get_subkey_parameter(HANDLE process_handle, const wchar_t *uname, char *value, int maxlen,
                     bool use_qualified, reg_platform_t whichreg)
{
    int retval;
    wchar_t app_specific_base[MAXIMUM_PATH] = DYNAMORIO_REGISTRY_BASE L"\\";
    /* DYNAMORIO_REGISTRY_BASE is not user controlled, ASSERT only */
    ASSERT(wcslen(app_specific_base) < BUFFER_SIZE_ELEMENTS(app_specific_base));

    if (process_handle == NULL) {
        wcsncat(app_specific_base,
                use_qualified ? get_own_short_qualified_name()
                              : get_own_short_unqualified_name(),
                BUFFER_ROOM_LEFT_W(app_specific_base));
    } else {
        /* instead of using another buffer for the temporary,
           we just append to the current one */
        get_process_qualified_name(
            process_handle, app_specific_base + wcslen(app_specific_base),
            BUFFER_ROOM_LEFT_W(app_specific_base),
            use_qualified ? QUALIFIED_SHORT_NAME : UNQUALIFIED_SHORT_NAME, whichreg);
    }
    NULL_TERMINATE_BUFFER(app_specific_base);

    retval = get_registry_parameter(app_specific_base, uname, value, maxlen, whichreg);
#        if VERBOSE
    display_verbose_message("gskp: %ls -- %ls\n\"%hs\"", app_specific_base, uname,
                            IS_GET_PARAMETER_SUCCESS(retval) ? value : "");
#        endif

    if (IS_GET_PARAMETER_FAILURE(retval)) {
        HANDLE hkey = reg_open_key(app_specific_base, KEY_READ);
        if (hkey == NULL) {
            retval = GET_PARAMETER_NOAPPSPECIFIC;
        }
        reg_close_key(hkey);
    }
    return retval;
}

/* value is a buffer allocated by the caller to hold the
 * resulting value. If not successful leaves original buffer contents intact.
 *
 * The same parameter is looked up first in the application specific registry
 * subtree and then in the global registry tree.  We no longer look for
 * environment variables.
 */
static int
get_process_parameter_internal(HANDLE phandle, const wchar_t *name, char *value,
                               int maxlen, bool use_qualified, reg_platform_t whichreg)
{
    int err, err2;

#        if VERBOSE
    display_verbose_message("get_parameter:%ls", name);
#        endif VERBOSE

    /* first check app specific options */
    err = get_subkey_parameter(phandle, name, value, maxlen, use_qualified, whichreg);

    if (err != GET_PARAMETER_SUCCESS) {
        err2 = get_registry_parameter(DYNAMORIO_REGISTRY_BASE, name, value, maxlen,
                                      whichreg);
        if (IS_GET_PARAMETER_SUCCESS(err2)) {
            /* if there's no app-specific but there is a global, return
             * GET_PARAMETER_NOAPPSPECIFIC; otherwise, if there's a
             * global, return success. */
            if (err != GET_PARAMETER_NOAPPSPECIFIC)
                err = GET_PARAMETER_SUCCESS;
        } else if (err == GET_PARAMETER_BUF_TOO_SMALL ||
                   err2 == GET_PARAMETER_BUF_TOO_SMALL) {
            /* On error, buffer too small takes precedence. */
            err = GET_PARAMETER_BUF_TOO_SMALL;
        } else
            err = GET_PARAMETER_FAILURE;
    }
    return err;
}

/* get parameter for a different process */
int
get_process_parameter(HANDLE phandle, const wchar_t *name, char *value, int maxlen)
{
    return get_process_parameter_internal(phandle, name, value, maxlen, true /*qual*/,
                                          REGISTRY_DEFAULT);
}

/* get parameter for current process */
int
d_r_get_parameter(const wchar_t *name, char *value, int maxlen)
{
    return get_process_parameter_internal(NULL, name, value, maxlen, true /*qual*/,
                                          REGISTRY_DEFAULT);
}

/* Identical to get_parameter: for compatibility w/ non-PARAMS_IN_REGISTRY */
int
get_parameter_ex(const wchar_t *name, char *value, int maxlen, bool ignore_cache)
{
    return d_r_get_parameter(name, value, maxlen);
}

#        ifdef X64
/* get parameter for current process name using 32-bit registry key */
int
get_parameter_32(const wchar_t *name, char *value, int maxlen)
{
    return get_process_parameter_internal(NULL, name, value, maxlen, true /*qual*/,
                                          REGISTRY_32);
}
#        else
/* get parameter for current process name using 64-bit registry key */
int
get_parameter_64(const wchar_t *name, char *value, int maxlen)
{
    return get_process_parameter_internal(NULL, name, value, maxlen, true /*qual*/,
                                          REGISTRY_64);
}
#        endif

/* get parameter for current processes root app key (not qualified app key)
 * for ex. would get parameter from svchost.exe instead of svchost.exe-netsvc */
int
get_unqualified_parameter(const wchar_t *name, char *value, int maxlen)
{
    return get_process_parameter_internal(NULL, name, value, maxlen, false /*!qual*/,
                                          REGISTRY_DEFAULT);
}

/* Description: Modifies the value name corresponding to a DR parameter.  This
 *              value name should belong to the registry key associated with the
 *              executable for the given process under DYNAMORIO_REGISTRY_BASE.
 * Input:       phandle - handle for the process whose registry value is to be
 *                        changed.
 *              name - value name for the corresponding DR parameter.
 *              value - value to which the registry parameter should be set to.
 * Output:      None.
 * Return value:SET_PARAMETER_SUCCESS if name is changed.
 *              SET_PARAMETER_FAILURE if not.
 * Notes:       If called with an incorrect parameter name, a value with that
 *              parameter name will be created in the registry for the current
 *              executable name under DYNAMORIO_REGISTRY_BASE.  Implemented as
 *              part of case 3702.
 */
int
set_process_parameter(HANDLE phandle, const wchar_t *name, const char *value)
{
    wchar_t app_specific_base[MAXIMUM_PATH] = DYNAMORIO_REGISTRY_BASE L"\\";

    /* Even though DYNAMORIO_REGISTRY_BASE is a constant, we need to null
     * terminate because windows compiler doesn't complain or null terminate
     * the array if the constant is longer than the array size!
     */
    NULL_TERMINATE_BUFFER(app_specific_base);

    /* DYNAMORIO_REGISTRY_BASE is not user controlled, ASSERT only */
    ASSERT(wcslen(app_specific_base) < BUFFER_SIZE_ELEMENTS(app_specific_base) - 1);

    if (phandle == NULL) {
        wcsncat(app_specific_base, get_own_short_qualified_name(),
                BUFFER_ROOM_LEFT_W(app_specific_base));
    } else {
        /* instead of using another buffer for the temporary,
           we just append to the current one */
        get_process_qualified_name(phandle, app_specific_base + wcslen(app_specific_base),
                                   BUFFER_ROOM_LEFT_W(app_specific_base),
                                   QUALIFIED_SHORT_NAME,
                                   false /*no cross-arch set needed yet*/);
    }
    NULL_TERMINATE_BUFFER(app_specific_base);
    ASSERT(wcslen(app_specific_base) < BUFFER_SIZE_ELEMENTS(app_specific_base) - 1);

    return set_registry_parameter(app_specific_base, name, value);
}
/***************************************************************************/
#    else /* PARAMS_IN_REGISTRY */
int
get_parameter_from_registry(const wchar_t *name, char *value, /* OUT */
                            int maxlen /* up to MAX_REGISTRY_PARAMETER */)
{
    return get_registry_parameter(DYNAMORIO_REGISTRY_BASE, name, value, maxlen,
                                  REGISTRY_DEFAULT);
}

#        ifndef NOT_DYNAMORIO_CORE
/* get parameter for a different process */
static int
get_process_parameter_ex(HANDLE phandle, const char *name, char *value, int maxlen,
                         bool consider_1config)
{
    wchar_t short_unqual_name[MAXIMUM_PATH];
    char appname[MAXIMUM_PATH];
    bool app_specific, from_1config;
    process_id_t pid;
    if (phandle == NULL) {
#            if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
        return d_r_get_parameter(name, value, maxlen);
#            else
        pid = process_id_from_handle(NT_CURRENT_PROCESS);
#            endif
    } else
        pid = process_id_from_handle(phandle);
    get_process_qualified_name(phandle, short_unqual_name,
                               BUFFER_SIZE_ELEMENTS(short_unqual_name),
                               UNQUALIFIED_SHORT_NAME, REGISTRY_DEFAULT);
    NULL_TERMINATE_BUFFER(short_unqual_name);
    snprintf(appname, BUFFER_SIZE_ELEMENTS(appname), "%ls", short_unqual_name);
    NULL_TERMINATE_BUFFER(appname);
    if (!get_config_val_other_app(appname, pid, DR_PLATFORM_DEFAULT, name, value, maxlen,
                                  &app_specific, NULL, &from_1config) ||
        (!consider_1config && from_1config))
        return GET_PARAMETER_FAILURE;
    if (!app_specific)
        return GET_PARAMETER_NOAPPSPECIFIC;
    return GET_PARAMETER_SUCCESS;
}

int
get_process_parameter(HANDLE phandle, const char *name, char *value, int maxlen)
{
    return get_process_parameter_ex(phandle, name, value, maxlen, true);
}
#        endif /* NOT_DYNAMORIO_CORE */

#        ifndef X64
int
get_parameter_64(const char *name, char *value, int maxlen)
{
    return get_config_val_other_arch(name, value, maxlen, NULL, NULL, NULL);
}
#        endif

#    endif /* PARAMS_IN_REGISTRY */
/***************************************************************************/

/* on NT */
static bool
is_nt_or_custom_safe_mode()
{
    char start_options[MAX_REGISTRY_PARAMETER];
    int retval;

    retval = get_registry_parameter(L"\\Registry\\Machine\\System\\CurrentControlSet"
                                    L"\\Control",
                                    L"SystemStartOptions", start_options,
                                    sizeof(start_options), REGISTRY_DEFAULT);
    if (IS_GET_PARAMETER_SUCCESS(retval)) {
        /* FIXME: should do only when non empty start options given */
        /* let's see if we have an override */
        char safemarker_override_buf[MAX_PARAMNAME_LENGTH];
        char *safemarker = "SOS";
        /* Currently doing only on NT, otherwise to preserve the
         * distinction in is_safe_mode() between MINIMAL and NETWORK
         * we'd need to check for SAFEBOOT:MINIMAL, since SOS will be
         * set for SAFEBOOT:NETWORK as well.
         * Note: There is no app specific override for safe boot, just global.
         */
        retval = get_registry_parameter(
            DYNAMORIO_REGISTRY_BASE, L_DYNAMORIO_VAR_SAFEMARKER, safemarker_override_buf,
            sizeof(safemarker_override_buf),
            /* currently only called on NT where there
             * is no wow64 */
            REGISTRY_DEFAULT);
        if (IS_GET_PARAMETER_SUCCESS(retval))
            safemarker = safemarker_override_buf;

        /* note that match is case sensitive, yet ntldr always
         * converts the boot.ini options in all CAPS, so the value in
         * DYNAMORIO_SAFEMARKER should always be all CAPS as well
         */
        if (strstr(start_options, safemarker)) {
            return true;
        }
    }
    return false;
}

/* note that windows_version_init does a lot more checks and messages
 * which we cannot use in drpreinject.dll
 * otherwise this should be equivalent to get_os_version() == WINDOWS_VERSION_NT
 */
static inline bool
is_windows_version_nt()
{
    PEB *peb = get_own_peb();
    /* we won't work on any other anyways */
    ASSERT(peb->OSPlatformId == VER_PLATFORM_WIN32_NT);
    return (peb->OSMajorVersion == 4);
}

/* see comments at is_windows_version_nt() */
static bool
is_windows_version_vista_plus()
{
    PEB *peb = get_own_peb();
    /* we won't work on any other anyways */
    ASSERT(peb->OSPlatformId == VER_PLATFORM_WIN32_NT);
    return (peb->OSMajorVersion >= 6);
}

/* verify safe mode registry key on Win2000+ */
bool
is_safe_mode()
{
    char buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(uint)];
    KEY_VALUE_PARTIAL_INFORMATION *kvpi = (KEY_VALUE_PARTIAL_INFORMATION *)buf;

    enum {
        MINIMAL = 1,
        NETWORK = 2,
    };
    /* On safe mode boot we should override all current settings and Run Nothing. */
    /* If the value is MINIMAL we do not inject.  Note we still inject
       when running with == NETWORK, we don't want to expose network
       listening services to risks, and to disable us administrators
       only need the local console */

    if (reg_query_value(L"\\Registry\\Machine\\System\\CurrentControlSet"
                        L"\\Control\\SafeBoot\\Option",
                        L"OptionValue", KeyValuePartialInformation, kvpi, sizeof(buf),
                        0) == REG_QUERY_SUCCESS) {
        if (*(uint *)(kvpi->Data) == MINIMAL) {
            return true;
        }
    }

    /* FIXME: case 5307: based on some other our registry key value we should
     * allow this even on Win2000 so that /DISABLESC can be passed on
     * the command line.
     */
    if (is_windows_version_nt()) {
        return is_nt_or_custom_safe_mode();
    }

    return false;
}

/* check AppInit key of current architecture (so if currently wow64,
 * checks wow64; if x64, checks x64.  we don't support cross-arch follow-children
 * (PR 254193).
 */
bool
systemwide_inject_enabled()
{
    /* FIXME: is it better to memoize the result for multiple uses in os.c? */
    /* There is always going to be a TOCTOU race condition anyways. */
    char appinit[MAXIMUM_PATH];
    int retval;

    retval = get_registry_parameter(
        INJECT_ALL_HIVE_L INJECT_ALL_KEY_L, INJECT_ALL_SUBKEY_L, appinit, sizeof(appinit),
        REGISTRY_DEFAULT /*no cross-arch support: PR 254193*/);
    if (IS_GET_PARAMETER_SUCCESS(retval)) {
        // assumption: nobody else would use this name!
        if ((strstr(appinit, INJECT_DLL_NAME) != NULL ||
             strstr(appinit, INJECT_DLL_8_3_NAME) != NULL))
            return 1;
    }
    return 0;
}

#    ifdef PARAMS_IN_REGISTRY /* config files don't support cmdline match */
/* returns true if the process commandline matches the string in
 * the DYNAMORIO_VAR_CMDLINE parameter.
 * if callers need REGISTRY_{32,64} they should add that parameter: not needed currently.
 */
static int
check_commandline_match(HANDLE process)
{
    char process_cmdline[MAX_PATH];
    wchar_t w_process_cmdline[MAX_PATH];

    if (process == NULL) {
        /* get our own cmdline */
        PEB *peb = get_own_peb();
        ASSERT(peb && peb->ProcessParameters);
        ASSERT(peb->ProcessParameters->CommandLine.Buffer);
        wcsncpy(w_process_cmdline, peb->ProcessParameters->CommandLine.Buffer, MAX_PATH);
        w_process_cmdline[MAX_PATH - 1] = L'\0';
    } else {
        get_process_imgname_cmdline(process, NULL, 0, w_process_cmdline, MAX_PATH);
        w_process_cmdline[MAX_PATH - 1] = L'\0';
    }
    wchar_to_char(process_cmdline, sizeof(process_cmdline), w_process_cmdline,
                  (wcslen(w_process_cmdline) + 1) * sizeof(wchar_t));

    {
        /* share buffer between cmd_line_to_match and w_process_cmdline to save
         * stack space */
        char *cmdline_to_match = (char *)w_process_cmdline;
        /* we expect an app-specific parameter only */
        if ((GET_PARAMETER_SUCCESS ==
             get_subkey_parameter(process, L_DYNAMORIO_VAR_CMDLINE, cmdline_to_match,
                                  MAX_PATH, true, REGISTRY_DEFAULT)) &&
            NULL != strstr(process_cmdline, cmdline_to_match))
            return 1;
        return 0;
    }
}
#    endif

/* look up RUNUNDER param.
 * if it's defined in app-specific key, check against RUNUNDER_ON
 *    if RUNUNDER_ON is set check against RUNUNDER_EXPLICIT
 *        if set return INJECT_TRUE|INJECT_EXPLICIT else return INJECT_TRUE
 *    if RUNUNDER_ON is not set return INJECT_EXCLUDED
 *
 * if no app-specific key, check global key against RUNUNDER_ALL
 *    if set return INJECT_TRUE else return INJECT_FALSE
 *
 * if no app-specific key and no global key return INJECT_FALSE
 *
 * if mask isn't NULL, the DYNAMORIO_RUNUNDER mask is returned in it.
 *
 * NOTE - if return INJECT_TRUE and !INJECT_EXPLICIT then preinjector should inject if
 *   systemwide_inject_enabled()
 */
static inject_setting_mask_t
systemwide_should_inject_common(HANDLE process, int *mask, reg_platform_t whichreg,
                                bool consider_1config)
{
    char runvalue[MAX_RUNVALUE_LENGTH];
#    ifdef PARAMS_IN_REGISTRY
    int retval;
#    endif
    int rununder_mask, err;

#    if VERBOSE
    display_verbose_message("systemwide_should_inject");
#    endif

#    ifdef PARAMS_IN_REGISTRY
    /* get_process_parameter properly terminates short buffer */
    err = get_process_parameter_internal(process, L_DYNAMORIO_VAR_RUNUNDER, runvalue,
                                         sizeof(runvalue), true /*qual*/, whichreg);
    if (IS_GET_PARAMETER_FAILURE(err))
        return INJECT_FALSE;
#    else
    /* Instead of a new GET_PARAMETER_PID_SPECIFIC success value which would require
     * changing several get_process_parameter callers who check specific return values,
     * we add a new _ex() routine that allows excluding 1config files.
     * For syswide we do NOT want to inject if there is a 1config file, to avoid
     * double injection.
     */
    err = get_process_parameter_ex(process, DYNAMORIO_VAR_RUNUNDER, runvalue,
                                   sizeof(runvalue), consider_1config);
    if (IS_GET_PARAMETER_FAILURE(err))
        return INJECT_FALSE;
#    endif

    rununder_mask = get_rununder_value(runvalue);
    if (NULL != mask) {
        if (IS_GET_PARAMETER_SUCCESS(err))
            *mask = rununder_mask;
        else
            *mask = 0;
    }

    /* if there is no app-specific subkey, then we should compare
       against runall */
    if (err == GET_PARAMETER_NOAPPSPECIFIC) {
        if (rununder_mask & RUNUNDER_ALL)
            return INJECT_TRUE;
        else
            return INJECT_FALSE;
    } else { /* err == GET_PARAMETER_SUCCESS */
        if (!(rununder_mask & RUNUNDER_ON))
            return INJECT_EXCLUDED;
        else {
            /* now there is the possibility of needing to use an alternate
             *  injection technique. */

            int inject_mask = INJECT_FALSE;

            if (rununder_mask & RUNUNDER_EXPLICIT)
                inject_mask |= INJECT_EXPLICIT;

#    ifdef PARAMS_IN_REGISTRY /* config files don't support cmdline match */
            if (rununder_mask & RUNUNDER_COMMANDLINE_MATCH) {

                /* if the commandline matches, return INJECT_TRUE
                 * if the commandline doesn't match and runall is on,
                 *  return INJECT_TRUE
                 * else return INJECT_FALSE */

                if (check_commandline_match(process)) {
                    inject_mask |= INJECT_TRUE;
                } else {
                    /* no match; check global runall */
                    retval = get_registry_parameter(DYNAMORIO_REGISTRY_BASE,
                                                    L_DYNAMORIO_VAR_RUNUNDER, runvalue,
                                                    sizeof(runvalue), whichreg);
                    if (IS_GET_PARAMETER_SUCCESS(retval)) {
                        if (RUNUNDER_ALL == get_rununder_value(runvalue))
                            inject_mask |= INJECT_TRUE;
                    }
                }

                return inject_mask;

            } else /* just normal injection */
#    endif
                return (inject_mask | INJECT_TRUE);
        }
    }
}

#    ifndef X64
inject_setting_mask_t
systemwide_should_preinject_64(HANDLE process, int *mask)
{
    return systemwide_should_inject_common(process, mask, REGISTRY_64, false);
}
#    endif

inject_setting_mask_t
systemwide_should_inject(HANDLE process, int *mask)
{
    return systemwide_should_inject_common(process, mask, REGISTRY_DEFAULT, true);
}

inject_setting_mask_t
systemwide_should_preinject(HANDLE process, int *mask)
{
    return systemwide_should_inject_common(process, mask, REGISTRY_DEFAULT, false);
}

/* Description: If RUNUNDER_ONCE flag exists in the given mask, the RUNUNDER_ON
 *              flag for the registry value DYNAMORIO_RUNUNDER is cleared for
 *              the given process, so that the application won't start under
 *              DR during the next invocation.
 * Input:       process - handle of the process whose registry value should be
 *                        modified.
 *              rununder_mask - contains the process' DYNAMORIO_RUNUNDER mask.
 * Output:      None.
 * Return value:None.
 * Notes:       See case 3702.
 */
void
check_for_run_once(HANDLE process, int rununder_mask)
{
#    ifdef PARAMS_IN_REGISTRY
    int size;
    char mask_string[MAX_RUNVALUE_LENGTH];

    if (TEST(rununder_mask, RUNUNDER_ONCE)) {
        rununder_mask &= ~RUNUNDER_ON;
        size =
            snprintf(mask_string, BUFFER_SIZE_ELEMENTS(mask_string), "%d", rununder_mask);
        NULL_TERMINATE_BUFFER(mask_string);
        ASSERT(size >= 0 && size < BUFFER_SIZE_ELEMENTS(mask_string) - 1);

        /* All registry keys set up by our product are writable only by
         * SYSTEM and Admin users.  If another user runs an executable with
         * RUNUNDER_ONCE, the core won't turn of RUNUNDER_ON because registry
         * write will fail.  This is an EV limitation: RUNUNDER_ONCE won't work
         * for non privileged processes.  Will be fixed in 2.5.  See case 4249.
         */
        if (set_process_parameter(process, L_DYNAMORIO_VAR_RUNUNDER, mask_string) !=
            SET_PARAMETER_SUCCESS) {
            /* FIXME: Till 2.5 ASSERT_NOT_REACHED/display_error should actually
             * be ASSERT_CURIOSITY.  Defining ASSERT_CURIOSITY for core,
             * drinject.exe and drpreinject.dll is an ugly redefinition hack;
             * better not do it just for case 4249.
             */
            display_error("Can't enforce RUNUNDER_ONCE.");
            ASSERT_NOT_REACHED();
        }
    }
#    else
    /* no support for RUNUNDER_ONCE for config files: use .1config32 instead */
#    endif
}

#endif /* !defined(NOT_DYNAMORIO_CORE) */

#ifdef UNIT_TEST

int
test(char *name, char *filter, int expected)
{
    int res = check_filter(name, filter);
    printf("check_filter(\"%s\", \"%s\") = %s\n", name, filter, res ? "true" : "false");
    if (res != expected)
        printf("FAILURE!\n");
}

int
nametest(const char *commandline, const char *expect, )
{
    wchar_t wcommandline[2048];
    wchar_t wexpect[2048];
    wchar_t derived[MAX_PATH];
    int ret;
    _snwprintf(wcommandline, sizeof(wcommandline), L"%S", commandline);
    _snwprintf(wexpect, sizeof(wexpect), L"%S", expect);

    ret = get_commandline_qualifier(wcommandline, derived, BUFFER_SIZE_ELEMENTS(derived),
                                    true);
    printf("get_commandline_qualifier(\"%s\") => \"%ls\" [%d]\n", commandline, derived,
           ret);

    if (wcscmp(derived, expect))
        printf("FAILED!\n");

    ret = get_commandline_qualifier(wcommandline, derived, BUFFER_SIZE_ELEMENTS(derived),
                                    REGISTRY_DEFAULT);
    printf("get_commandline_qualifier(\"%s\") => \"%ls\" [%d]\n", commandline, derived,
           ret);
}

int
main()
{
    char *filter = "calc.exe;notepad.exe";

    test("notepad.exe", filter, 1);
    test("calc.exe", filter, 1);
    test("write.exe", filter, 0);
    test("test.exe", filter, 0);

    filter = NULL;
    test("notepad.exe", filter, 0);
    test("calc.exe", filter, 0);
    test("write.exe", filter, 0);
    test("test.exe", filter, 0);

    nametest("C:\WINNT\System32\dllhost.exe /Processid:{"
             "3D14228D-FBE1-11D0-995D-00C04FD919C1}",
             "");

    /* short name lots of spaces */
    nametest("svchost.exe   -k    netsvc    ", "");

    /* a real example: in fact the service executable name for RpcSs doesn't have .exe */
    nametest("system32\svchost -k rpcss    ", "");

    /* spaces in long name actually require " */
    nametest("\"c:\program files\test\my test\sqlserver.exe\"   -s uddi    ", "");
    /* spaces in long name and no .exe */
    nametest("\"c:\program files\test\my test\sqlserver\" -s uddi    ", "");

    /* capital .EXE */
    nametest("c:\program files\test\my test\sqlserver.EXE   -s uddi    ", "");
    /* capital .EXE and a backslashes in case trying using short_name  */
    nametest("c:\program files\test\my test\sqlserver.EXE   -s uddi -u test\test    ",
             "");
    nametest("C:\WINNT\System32\dllhost.exe "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1} "
             "/Processid:{3D14228D-FBE1-11D0-995D-00C04FD919C1}",
             "");

    nametest("dllhost.exe ////// /P#%@#$%-k    netsvc    ", "");

    /* test that snprintf/snwprintf are respecting bounds */
    /* FIXME : move to some place shared with linux and expand to more
     * cases (and add vs* functions) */
    {
        char buf[20];
        memset(buf, 0x1, sizeof(buf));
        snprintf(buf, 10, "0123456789%dfoo", 555);
        if (buf[10] != 0x1 || buf[9] != '5')
            printf("FAILED snprintf safety check\n");
    }
    {
        wchar_t buf[20];
        memset(buf, 0x1, sizeof(buf));
        snwprintf(buf, 10, "0123456789%dfoo", 555);
        if (buf[10] != 0x11 || buf[9] != L'5')
            printf("FAILED snprintf safety check\n");
    }
    return 0;
}

#endif /* UNIT_TEST */
