/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
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
 *
 * configuartion interface definitions
 *
 *
 * this is intended to serve as a replacement for the cluttered
 *  policy.c code for reading and writing core parameters. OOP
 *  style has been kept in mind to allow easy porting to c++
 *  if/when we decide to make that switch.
 *
 * usage:
 *  a ConfigGroup is more or less equivalent to a registry key: it can
 *  hold name-value pairs and 'children' ConfigGroups. ConfigGroup
 *  paths are ":" separated, and for registry purposes are assumed to
 *  be rooted at HKLM/Software/Determina. the idea is that if we move
 *  away from the registry (eg, due to moving to another platform or
 *  to config files for core params), this interface should still be
 *  usable, and the only change would be in {read,write}_config_group.
 *
 *  there are also direct-access single-parameter config
 *  functions; these allow for arbitray registry read/write. however,
 *  unless otherwise specified with the 'absolute' parameter, these
 *  are still based at the determina key.
 *
 *
 */

#ifndef _DETERMINA_CONFIG_H_
#define _DETERMINA_CONFIG_H_

#include <windows.h>
#include "globals_shared.h" /* for process_id_t */

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARAM_LEN 1024
#define CONFIG_PATH_SEPARATOR L':'
#define LIST_SEPARATOR_CHAR L';'
#define APPINIT_SEPARATOR_CHAR L','
#define CONFIGURATION_ROOT_REGISTRY_KEY L"Software\\" L_EXPAND_LEVEL(COMPANY_NAME)

/* this provides a hook for forced parameter deletion, even if
 * "should_clear" is FALSE. */
#define L_DELETE_PARAMETER_KEY L"__DELETE_PARAMETER_KEY"

/* note this does NOT include the null terminator: the limit
 *  is 31 chars. */
#define APPINIT_SYSTEM32_LENGTH_LIMIT 31

typedef struct NameValuePairNode_ {
    struct NameValuePairNode_ *next;
    WCHAR *name;
    WCHAR *value;
} NameValuePairNode;

typedef struct ConfigGroup_ {
    struct ConfigGroup_ *next;
    struct ConfigGroup_ *children;
    /* used internally only for efficiency */
    struct ConfigGroup_ *lastchild;
    NameValuePairNode *params;
    BOOL should_clear;
    WCHAR *name;
} ConfigGroup;

/* group config management */

DWORD
get_key_handle(HKEY *key, HKEY parent, const WCHAR *path, BOOL absolute, DWORD flags);

DWORD
recursive_delete_key(HKEY parent, const WCHAR *keyname, ConfigGroup *filter);

ConfigGroup *
new_config_group(const WCHAR *name);

ConfigGroup *
copy_config_group(ConfigGroup *config, BOOL deep);

/* name is the subkey name from the system registry root (e.g.,
 *  HKLM/Software/Determina). */
DWORD
read_config_group(ConfigGroup **configptr, const WCHAR *name,
                  BOOL read_children_recursively);

void
set_should_clear(ConfigGroup *config, BOOL should_clear);

void
remove_children(ConfigGroup *config);

void
remove_child(const WCHAR *child, ConfigGroup *config);

WCHAR *
get_config_group_parameter(ConfigGroup *config, const WCHAR *name);

void
set_config_group_parameter(ConfigGroup *config, const WCHAR *name, const WCHAR *value);

void
remove_config_group_parameter(ConfigGroup *config, const WCHAR *name);

void
add_config_group(ConfigGroup *parent, ConfigGroup *new_child);

ConfigGroup *
get_child(const WCHAR *name, ConfigGroup *c);

DWORD
write_config_group(ConfigGroup *config);

void
free_config_group(ConfigGroup *configptr);

void
dump_nvp(NameValuePairNode *nvpn);

void
dump_config_group(char *prefix, char *incr, ConfigGroup *c, BOOL traverse);

BOOL
get_config_group_parameter_bool(ConfigGroup *config, const WCHAR *name);

int
get_config_group_parameter_int(ConfigGroup *config, const WCHAR *name);

void
get_config_group_parameter_scrambled(ConfigGroup *config, const WCHAR *name,
                                     WCHAR *buffer, UINT maxchars);

void
set_config_group_parameter_bool(ConfigGroup *config, const WCHAR *name, BOOL value);

void
set_config_group_parameter_int(ConfigGroup *config, const WCHAR *name, int value);

void
set_config_group_parameter_ascii(ConfigGroup *config, const WCHAR *name, char *value);

void
set_config_group_parameter_scrambled(ConfigGroup *config, const WCHAR *name,
                                     const WCHAR *value);

/* single parameter config functions */

DWORD
set_config_parameter(const WCHAR *path, BOOL absolute, const WCHAR *name,
                     const WCHAR *value);

DWORD
get_config_parameter(const WCHAR *path, BOOL absolute, const WCHAR *name, WCHAR *value,
                     int maxlen);

DWORD
read_reg_string(HKEY subkey, const WCHAR *keyname, WCHAR *value, int valchars);

/* if value is NULL, the value will be deleted */
DWORD
write_reg_string(HKEY subkey, const WCHAR *keyname, const WCHAR *value);

/* identifies processes relative to a ConfigGroup */
ConfigGroup *
get_process_config_group(ConfigGroup *config, process_id_t pid);

BOOL
is_parent_of_qualified_config_group(ConfigGroup *config);

/* tries both with and without no_strip */
ConfigGroup *
get_qualified_config_group(ConfigGroup *config, const WCHAR *exename,
                           const WCHAR *cmdline);

/* some list management and utility routines */
/* all lists are ;-separated */
/* comparisons are case insensitive */
/* filename comparisons are independent of path */

WCHAR *
new_file_list(SIZE_T initial_chars);

void
free_file_list(WCHAR *list);

/*
 * given a ;-separated list and a filename, return a pointer to
 *  the filename in the list, if it appears. comparisons are
 *  case insensitive and independent of path; eg,
 *     get_entry_location("c:\\foo\\bar.dll;blah;...", "D:\\Bar.DLL")
 *  would return a pointer to the beginning of the list.
 */
WCHAR *
get_entry_location(const WCHAR *list, const WCHAR *filename, WCHAR separator);

BOOL
is_in_file_list(const WCHAR *list, const WCHAR *filename, WCHAR separator);

/* frees the old list and returns a newly alloc'ed one */
WCHAR *
add_to_file_list(WCHAR *list, const WCHAR *filename, BOOL check_for_duplicates,
                 BOOL add_to_front, BOOL overwrite_existing, WCHAR separator);

void
remove_from_file_list(WCHAR *list, const WCHAR *filename, WCHAR separator);

BOOL
blocklist_filter(WCHAR *list, const WCHAR *blocklist, BOOL check_only, WCHAR separator);

BOOL
allowlist_filter(WCHAR *list, const WCHAR *allowlist, BOOL check_only, WCHAR separator);

DWORD
set_autoinjection_ex(BOOL inject, DWORD flags, const WCHAR *blocklist,
                     const WCHAR *allowlist, DWORD *list_error,
                     const WCHAR *custom_preinject_name, WCHAR *current_list,
                     SIZE_T maxchars);

DWORD
set_custom_autoinjection(const WCHAR *preinject, DWORD flags);

DWORD
set_autoinjection();

DWORD
unset_custom_autoinjection(const WCHAR *preinject, DWORD flags);

DWORD
unset_autoinjection();

BOOL
is_autoinjection_set();

BOOL
is_custom_autoinjection_set(const WCHAR *preinject);

DWORD
set_loadappinit();

DWORD
unset_loadappinit();

BOOL
is_loadappinit_set();

DWORD
create_eventlog(const WCHAR *dll_path);

DWORD
destroy_eventlog(void);

BOOL
is_vista(void);

BOOL
is_win7(void);

DWORD
copy_earlyhelper_dlls(const WCHAR *dir);

#ifdef __cplusplus
}
#endif

#endif _DETERMINA_CONFIG_H_
