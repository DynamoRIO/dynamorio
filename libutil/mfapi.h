/* **********************************************************
 * Copyright (c) 2014-2022 Google, Inc.  All rights reserved.
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

/*
 * mfapi.h
 *
 * Interface for controlling Determina protection on a local machine
 *
 * (assumes a proper installation exists)
 *
 */

#ifndef _DETERMINA_MFAPI_H_
#define _DETERMINA_MFAPI_H_

#ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <wchar.h>

/* we don't want to include core headers here */
typedef UINT_PTR process_id_t;

#ifdef __cplusplus
extern "C" {
#endif

/* custom Win32-style error codes; using "high" constants so that
 *   it's obvious it's not win32. */
#define ERROR_OPTION_NOT_FOUND 0xffffffff
#define ERROR_NOT_INITIALIZED 0xfffffffe
#define ERROR_UNKNOWN_ENTRY 0xfffffffd
#define ERROR_DETACH_NOT_ALLOWED 0xfffffffc
#define ERROR_UNSUPPORTED_OS 0xfffffffb
#define ERROR_LIST_VIOLATION 0xfffffffa
#define ERROR_LENGTH_VIOLATION 0xfffffff9
#define ERROR_DETACH_ERROR 0xfffffff8
#define ERROR_PARSE_ERROR 0xfffffff7
#define ERROR_DRMARKER_ERROR 0xfffffff6

/************************
 * policy import/export *
 ************************/

/*
 *
 * policy definition format specification
 * --------------------------------------
 *
 *
 * the syntax of the policy definition message is as follows:

 * <policy_message> ::==
 *  POLICY_VERSION=30000
 *  APPINITFLAGS=<string>
 *  APPINITALLOWLIST=<string>
 *  APPINITBLOCKLIST=<string>
 *  GLOBAL_PROTECT=<boolean>
 *  <application_block>*
 *
 * <application_block> ::==
 *  BEGIN_BLOCK
 *  <app_specifier>
 *  [ <hot_patch_block> ]
 *  <dynamorio_option_line>*
 *  END_BLOCK
 *
 * <app_specifier> ::== [ GLOBAL | APP_NAME=<string> ]
 *
 * <dynamorio_option_line> ::== <dynamorio_option>=<string>
 *
 * <dynamorio_option> ::==
 *  [ DYNAMORIO_OPTIONS |
 *    DYNAMORIO_AUTOINJECT |
 *    DYNAMORIO_RUNUNDER |
 *    DYNAMORIO_HOT_PATCH_MODES |
 *    DYNAMORIO_HOT_PATCH_POLICIES ]
 *
 * <hot_patch_block> ::==
 *  BEGIN_MP_MODES
 *  <num_hot_patch_lines>
 *  <hot_patch_line>*
 *  END_MP_MODES
 *
 * <num_hot_patch_lines> ::== (int)
 *
 * <hot_patch_line> ::==
 *  <patch_id>:<patch_mode>
 *
 * <patch_mode> ::== [ 0 | 1 | 2 ]
 *
 * <patch_id>: provided by Determina
 *
 * <boolean> ::== [ 0 | 1 ]
 *
 *
 *
 * details:
 *
 * (1) the APPINITFLAGS, together with the APPINITBLOCKLIST and
 * APPINITALLOWLIST, controls how our bootstrap dll is added to the
 * AppInit_DLLs registry key. the value of the flags should be a sum
 * of the APPINIT_* flags as defined in share/config.h
 *
 * The APPINITBLOCKLIST and APPINITALLOWLIST values are only used if
 * specified by the flags. we'll provide the allowlist/blocklist.
 *
 *
 * (2) GLOBAL_PROTECT: OPTIONAL: if this is 0, then protection is
 * disabled (and all application blocks are optional). in normal
 * situations this will be 1. if this is not specified, then the
 * current setting is kept (see also enable_protection)
 *
 *
 * (3) there must be a GLOBAL block, which should come first, and it
 * must have DYNAMORIO_RUNUNDER=1 set as an option. for consistency it
 * should set DYNAMORIO_OPTIONS to blank (DYNAMORIO_OPTIONS=).
 *
 *
 * (4) the APP_NAME must be a valid "application id". this is usually
 * the .exe filename, but can also be "qualified". we will provide a
 * complete list of supported applications.
 *
 *
 * (5) some applications will also require special values for
 * DYNAMORIO_RUNUNDER; we will provide this information as well.
 *
 *
 * (6) DYNAMORIO_AUTOINJECT must be specified for every non-global
 * application block, and it should be of the format
 * "\lib\NNNNN\dynamorio.dll", where NNNNN is the version number of
 * the core dll being used. in general, any value that starts with a
 * "\" will have the local installation path prepended when using the
 * actual value (eg, it indicates a relative installation path).
 *
 *
 * (7) DYNAMORIO_OPTIONS is a string consisting of protection options
 * to our core. the variability in this will depend on the level of
 * configurability desired in your UI; but the majority of the options
 * will not need to change. probably the only configuration necessary
 * will be whether an app should be on or off; and this should be
 * controlled by including or excluding the corresponding app block.
 *
 *
 * (8) to enable hot patching, there are three additional
 *     requirements:
 *    (a) the global DYNAMORIO_HOT_PATCH_DEFS option should be
 *        declared, and point to the relative subpath of the hot patch
 *        definitions file (provided with the update package). Typically
 *        this should just be "\config" -- the engine version and
 *        file name is standard. Of course this file must exist in the
 *        proper place: "\config\30000\hotp-defs.cfg".
 *    (b) similary, each app must provide the DYNAMORIO_HOT_PATCH_MODES
 *        subpath for the hotp modes file. Typically this will be the
 *        config directory followed by the app name, e.g.,
 *        "\config\detertest.exe". The modes files are created by the
 *        import process and should NOT be created manually.
 *    (c) Each app that requires patches must specify a list of patch
 *        id's and their "modes".  The mode codes are 0 = Off, 1 =
 *        Detect Only, 2 = Protect.  Typically only modes 0 and 2 will
 *        be used. As indicated above, the first line must be the number
 *        of patches specified. The list of patch id's and their
 *        descriptions is provided by Determina in the patch update
 *        packages.
 *
 * See sample.mfp for an example policy string.
 *
 */

/* import the specified configuration, thus enabling Determina
 *  protection for all newly launched processes. note that, according
 *  to the policy definition (GLOBAL_PROTECT), this will either call
 *  enable_protection or disable_protection.
 *
 * if synchronize_system, then any processes were under Determina
 *  but are now configured to be off will be detached. otherwise,
 *  no changes will be made to any running processes.
 *
 * if inject_flag is not NULL, it will return whether or not injection
 *  should be turned on, e.g., with the enable_protection()
 *  method. this is mainly to allow clients to call
 *  enable_protection_ex() with various custom flags. if inject_flag
 *  is NULL, the import method will take care of enabling or disabling
 *  protection based on the policy.
 *
 * if warning is not NULL, it will be set to an error
 *   code indicating if a non-critical error (such as detach) was
 *   encountered.
 *
 * note that unlike other methods, the policy definition is a char
 *   (not WCHAR) -- this is because (presumably) the policy
 *   definition is being read from an ASCII file or a network
 *   connection. */
DWORD
policy_import(char *policy_definition, BOOL synchronize_system, BOOL *inject_flag,
              DWORD *warning);

/* returns a policy definition string based on the current registry
 *  configuration.
 * may return ERROR_MORE_DATA, in which case the call should
 *  be re-tried with a larger buffer.
 * again note char buffer. */
DWORD
policy_export(char *policy_buffer, SIZE_T maxchars, SIZE_T *needed);

/* convenience method for loading from a file */
DWORD
load_policy(WCHAR *filename, BOOL synchronize_system, DWORD *warning);

/* convenience method for storing to a file */
DWORD
save_policy(WCHAR *filename);

/* returns ERROR_SUCCESS unless the policy_buffer is invalid. */
DWORD
validate_policy(char *policy_buffer);

/* writes an empty policy to the registry (so that no apps will be
 *  protected). distinct from disable_protection, which just clears
 *  the AppInit_DLLs key. */
DWORD
clear_policy();

/************************
 * AppInit_DLLs setting *
 ************************/

/* enable Determina protection on the system. note that this is not
 *  required if implicitly invoked in policy_import. */
DWORD
enable_protection();

/* checks the current protection status. */
BOOL
is_protection_enabled();

/* disable Determina protection on the system. */
DWORD
disable_protection();

/*
 * For more detailed control over the AppInit_DLLs key, use
 *  the following function.
 *
 * AppInit_DLLs flags:
 *   (for enable_protection_ex())
 *
 * APPINIT_FORCE_TO_FRONT
 *   Forces preinject dll to the front of the AppInit_DLLs list.
 *
 * APPINIT_FORCE_TO_BACK
 *   Forces preinject dll to the back of the AppInit_DLLs list.
 *
 * APPINIT_USE_BLOCKLIST
 *   Will read blocklist from blocklist config parameter and
 *     validate against it.
 *
 * APPINIT_USE_ALLOWLIST
 *   Will read allowlist from allowlist config parameter and
 *     validate against it.
 *
 * APPINIT_CHECK_LISTS_ONLY
 *   If set, the lists are only checked, not enforced; intended
 *     for use with at least one of the two flags below.
 *
 * APPINIT_WARN_ON_LIST_VIOLATION
 *   Whether to generate a warning if a allowlist/blocklist
 *    violation was detected.
 *
 * APPINIT_BAIL_ON_LIST_VIOLATION
 *   If set, the preinjector will not be added to the
 *     list, thus effectively disabling Determina.
 *
 *
 * The following flags apply only for platforms which
 *   require preinject to be in SYSTEM32 (ie, NT4 for now).
 *
 * APPINIT_SYS32_USE_LENGTH_WORKAROUND
 *   Not yet supported -- must be zero. Placeholder
 *     for using some alternate technique to handle
 *     other dlls which may be in the list.
 *
 * APPINIT_SYS32_FAIL_ON_LENGTH_ERROR
 *   If existing entries + preinject name
 *    exceeds the 31-character limit, leave key
 *    untouched and report an error.
 *
 * APPINIT_SYS32_CLEAR_OTHERS
 *   If set, any other appinit entries are
 *    cleared, regardless of length. otherwise,
 *    all entries are left intact, and an
 *    operation that results in too long of
 *    a string will be truncated but succeed
 *    unless APPINIT_SYS32_FAIL_ON_LENGTH_ERROR
 *    is set.
 *
 * APPINIT_SYS32_TRUNCATE
 *   If set, appinit is truncated to 31 chars
 *    before writing; otherwise, the entire
 *    string is written, even if over the limit.
 *
 */

#define APPINIT_FORCE_TO_FRONT 0x1
#define APPINIT_FORCE_TO_BACK 0x2
#define APPINIT_USE_BLOCKLIST 0x4
#define APPINIT_USE_ALLOWLIST 0x8
#define APPINIT_CHECK_LISTS_ONLY 0x10
#define APPINIT_WARN_ON_LIST_VIOLATION 0x20
#define APPINIT_BAIL_ON_LIST_VIOLATION 0x40
#define APPINIT_SYS32_USE_LENGTH_WORKAROUND 0x100
#define APPINIT_SYS32_FAIL_ON_LENGTH_ERROR 0x200
#define APPINIT_SYS32_CLEAR_OTHERS 0x400
#define APPINIT_SYS32_TRUNCATE 0x800
#define APPINIT_OVERWRITE 0x1000

/* The flags, blocklist, allowlist, list_error parameters are as
 *  described above.
 * The inject parameter controls whether protection is enabled or
 *  disabled. If disabled all other parameters are ignored.
 * The custom_preinject_name allows a non-standard DLL to be used;
 *  generally this should be NULL.
 * The current_list buffer (of size maxchars) is optional; if provided
 *  it will contain the contents of the AppInit_DLLs key before any
 *  changes are made; this is useful in the case that list_error
 *  violation is reported.
 */
DWORD
enable_protection_ex(BOOL inject, DWORD flags, const WCHAR *blocklist,
                     const WCHAR *allowlist, DWORD *list_error,
                     const WCHAR *custom_preinject_name, WCHAR *current_list,
                     SIZE_T maxchars);

/*************************
 *    process status     *
 *************************/

/* generic process walk callback routine; the callback should
 *  return FALSE to abort the walk. optionally takes a parameter
 *  which can be passed to the enumerate_processes method. */
typedef BOOL (*process_callback)(ULONG pid, WCHAR *process_name, void **param);

/* helper method: for each running processes on the system,
 *  executes the callback with the specified info. (everything is
 *  synchronous here.)
 * note that this is much cleaner than the Win32 api's tlhelp32
 *  interface, which should NEVER be used! */
DWORD
enumerate_processes(process_callback pcb, void **param);

/* process status codes */
#define INJECT_STATUS_PROTECTED 1
#define INJECT_STATUS_NATIVE 2
#define INJECT_STATUS_UNKNOWN 3

/* returns one of above status definitions in the status pointer.
 * if the build pointer is non-NULL, returns the build number of the
 *   core (if the process is protected).*/
DWORD
inject_status(process_id_t pid, DWORD *status, DWORD *build);

/* returns TRUE if the indicated process is configured for
 *  protection, yet is not running under Determina (this means it
 *  must be restarted before protection will take effect).
 * returns FALSE if the process is under Determina or not configured
 *  for protection. (or in the case of any error.)
 * note this is fairly expensive because it needs to load the current
 *  policy from the registry and check against it. a more efficient
 *  API may be exposed in the future.
 */
BOOL
is_process_pending_restart(process_id_t pid);

/* returns TRUE if any process is_pending_restart */
BOOL
is_any_process_pending_restart();

/********************
 *  detach / nudge  *
 ********************/

/* in ms */
#define DETACH_RECOMMENDED_TIMEOUT 60000
#define NUDGE_NO_DELAY 0

/* this is recommended for all hotp notification methods. */
#define NUDGE_RECOMMENDED_PAUSE 100

/* detach from indicated process -- if allow_upgraded_perms, then
 *  attempt to acquire necessary privileges (usually necessary) */
DWORD
detach(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms);

/* detach from all running processes (with timeout).
 * timeout is per process */
DWORD
detach_all(DWORD timeout);

/* detach from all processes with the matching exe name */
DWORD
detach_exe(WCHAR *exename, DWORD timeout_ms);

/* make sure that only configured processes are running under
 *   Determina */
DWORD
consistency_detach(DWORD timeout);

/* use this to notify a process to use new configuration information
 *  in the memory patch "modes" file. */
DWORD
hotp_notify_modes_update(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms);

/* use this to notify a process to use new configuration information
 *  in the memory patch "definitions" file -- note that since new
 *  policy definitions will be re-loaded, this will also cause the
 *  modes information to be re-read as well. */
DWORD
hotp_notify_defs_update(process_id_t pid, BOOL allow_upgraded_perms, DWORD timeout_ms);

/* get modes update for all running processes. timeout is per process */
DWORD
hotp_notify_all_modes_update(DWORD timeout_ms);

/* get defs update for all running processes. timeout is per process */
DWORD
hotp_notify_all_defs_update(DWORD timeout_ms);

/* get modes update for all running processes matching exe name.
 *  timeout is per process */
DWORD
hotp_notify_exe_modes_update(WCHAR *exename, DWORD timeout_ms);

/************************
 *      event log       *
 ************************/

/*
 * start_eventlog_monitor:
 * spawns a new thread which monitors the Araksha event log for
 *  new messages. the eventlog_callback is called for each new
 *  event with parameters as described above. any warnings, errors,
 *  failurse, etc. trigger the cb_err callback.
 * returns a handle to the listener thread.
 *
 * use stop_eventlog_monitor to cleanly destroy the listener thread
 */

/* monitor error codes */
#define ELM_ERR_FATAL 1
#define ELM_ERR_WARN 2
#define ELM_ERR_CLEARED 3

typedef void (*eventlog_formatted_callback)(unsigned int mID, unsigned int type,
                                            WCHAR *message, DWORD timestamp);

typedef void (*eventlog_raw_callback)(EVENTLOGRECORD *record);

typedef void (*eventlog_error_callback)(unsigned int errcode, WCHAR *message);

/* last_record must be a valid eventlog record number!!
 * this can be obtained from the "raw" callback by the RecordNumber
 *   value of the EVENTLOGRECORD struct.
 * pass -1 to retrieve all event records. */
DWORD
start_eventlog_monitor(BOOL use_formatted_callback, eventlog_formatted_callback cb_format,
                       eventlog_raw_callback cb_raw, eventlog_error_callback cb_err,
                       DWORD next_record);

void
stop_eventlog_monitor();

/* removes all events from the Determina event log */
DWORD
clear_eventlog();

HANDLE
get_eventlog_monitor_thread_handle();

BOOL
is_violation_event(DWORD eventType);

/* helper method for parsing message strings in an EVENTLOGRECORD */
const WCHAR *
next_message_string(const WCHAR *prev_string);

/* helper method for getting the (pathless) executable name out
 *  of an EVENTLOGRECORD */
const WCHAR *
get_event_exename(EVENTLOGRECORD *pevlr);

/* helper method for getting the PID out of an EVENTLOGRECORD */
UINT
get_event_pid(EVENTLOGRECORD *pevlr);

/* helper method for getting the Threat ID out of an EVENTLOGRECORD.
 * If the event type does not have a Threat ID parameter (e.g., for
 * information events), then this will return NULL. */
const WCHAR *
get_event_threatid(EVENTLOGRECORD *pevlr);

/* helper method for getting the formatted text from an EVENTLOGRECORD
 * structure. */
DWORD
get_formatted_message(EVENTLOGRECORD *pevlr, WCHAR *buf, DWORD maxchars);

/***************************************
 *  system information / installation  *
 ***************************************/

/* supported platform identifiers */
#define PLATFORM_UNKNOWN 0
#define PLATFORM_WIN_2000 100
#define PLATFORM_WIN_XP 110
#define PLATFORM_WIN_2003 120
#define PLATFORM_WIN_NT_4 130
#define PLATFORM_VISTA 140
#define PLATFORM_WIN_7 150
#define PLATFORM_WIN_8 160
#define PLATFORM_WIN_8_1 170
#define PLATFORM_WIN_10 180
#define PLATFORM_WIN_10_1511 190
#define PLATFORM_WIN_10_1607 200
#define PLATFORM_WIN_10_1703 210
#define PLATFORM_WIN_10_1709 220
#define PLATFORM_WIN_10_1803 230

DWORD
get_platform(DWORD *platform);

const WCHAR *
get_installation_path();

const WCHAR *
get_product_name();

#ifdef __cplusplus
}
#endif

#endif
