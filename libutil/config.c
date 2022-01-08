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
 *
 * config.c: general configuration interface
 *
 */

#include "share.h"
#include "config.h"
#include "processes.h"
#include "string.h"
#include "parser.h"
#include <stdio.h>

#ifndef UNIT_TEST

void
configpath_to_registry_path(WCHAR *path)
{
    UINT i = 0;

    if (NULL == path)
        return;

    for (i = 0; i < wcslen(path); i++)
        if (CONFIG_PATH_SEPARATOR == path[i])
            path[i] = L'\\';
}

DWORD
get_key_handle(HKEY *key, HKEY parent, const WCHAR *path, BOOL absolute, DWORD flags)
{
    WCHAR keyname[MAX_PATH];

    DO_ASSERT(key != NULL);

    if (path == NULL) {
        wcsncpy(keyname, CONFIGURATION_ROOT_REGISTRY_KEY, MAX_PATH);
    } else if (parent == NULL && !absolute) {
        _snwprintf(keyname, MAX_PATH, L"%s\\%s", CONFIGURATION_ROOT_REGISTRY_KEY, path);
    } else {
        wcsncpy(keyname, path, MAX_PATH);
    }

    DO_DEBUG(DL_VERB, printf("get_key_handle using %S as keyname\n", keyname););

    configpath_to_registry_path(keyname);

    return RegCreateKeyEx(parent == NULL ? DYNAMORIO_REGISTRY_HIVE : parent, keyname, 0,
                          NULL, REG_OPTION_NON_VOLATILE, platform_key_flags() | flags,
                          NULL, key, NULL);
}

ConfigGroup *
get_child(const WCHAR *name, ConfigGroup *c)
{
    ConfigGroup *cur = NULL;

    if (c == NULL)
        return NULL;

    for (cur = c->children; NULL != cur; cur = cur->next) {
        if (0 == wcsicmp(name, cur->name))
            return cur;
    }

    return NULL;
}

void
remove_child(const WCHAR *child, ConfigGroup *config)
{
    ConfigGroup *cur = NULL, *prev = NULL;

    if (NULL == config)
        return;

    for (cur = config->children; NULL != cur; cur = cur->next) {
        if (0 == wcsicmp(child, cur->name)) {
            if (NULL == prev)
                config->children = cur->next;
            else
                prev->next = cur->next;

            if (config->lastchild == cur)
                config->lastchild = prev;

            free_config_group(cur);

            return;
        }
        prev = cur;
    }
}

BOOL
count_params(ConfigGroup *c)
{
    int i = 0;
    NameValuePairNode *cur;
    for (cur = c->params; NULL != cur; cur = cur->next)
        i++;
    return i;
}

BOOL
is_param(const WCHAR *name, ConfigGroup *c)
{
    NameValuePairNode *cur;

    if (c == NULL)
        return FALSE;

    for (cur = c->params; NULL != cur; cur = cur->next) {
        if (0 == wcsicmp(name, cur->name))
            return TRUE;
    }

    return FALSE;
}

ConfigGroup *
new_config_group(const WCHAR *name)
{
    ConfigGroup *c = (ConfigGroup *)malloc(sizeof(ConfigGroup));
    c->next = NULL;
    c->children = NULL;
    c->lastchild = NULL;
    c->params = NULL;
    c->should_clear = FALSE;
    c->name = (NULL == name) ? NULL : wcsdup(name);

    return c;
}

ConfigGroup *
copy_config_group(ConfigGroup *config, BOOL deep)
{
    NameValuePairNode *tmpnvp;
    ConfigGroup *tmpcfg;

    ConfigGroup *c = new_config_group(config->name);
    c->should_clear = config->should_clear;
    tmpnvp = config->params;
    while (tmpnvp != NULL) {
        set_config_group_parameter(c, tmpnvp->name, tmpnvp->value);
        tmpnvp = tmpnvp->next;
    }

    if (deep) {
        tmpcfg = config->children;
        while (tmpcfg != NULL) {
            add_config_group(c, copy_config_group(tmpcfg, deep));
            tmpcfg = tmpcfg->next;
        }
    }

    return c;
}

void
remove_children(ConfigGroup *config)
{
    ConfigGroup *cur = config->children, *prev;

    while (NULL != cur) {
        prev = cur;
        cur = cur->next;
        free_config_group(prev);
    }

    config->children = NULL;
    config->lastchild = NULL;
}

NameValuePairNode *
new_nvp_node(const WCHAR *name)
{
    NameValuePairNode *nvp = (NameValuePairNode *)malloc(sizeof(NameValuePairNode));
    nvp->next = NULL;
    nvp->name = wcsdup(name);
    nvp->value = NULL;

    return nvp;
}

void
free_nvp(NameValuePairNode *nvpn)
{
    if (nvpn->name != NULL)
        free(nvpn->name);

    if (nvpn->value != NULL)
        free(nvpn->value);

    free(nvpn);
}

void
free_config_group(ConfigGroup *config)
{
    NameValuePairNode *nvpn = config->params, *tmp;

    remove_children(config);

    while (nvpn != NULL) {
        tmp = nvpn->next;
        free_nvp(nvpn);
        nvpn = tmp;
    }

    if (config->name != NULL)
        free(config->name);

    free(config);
}

NameValuePairNode *
get_nvp_node(ConfigGroup *config, const WCHAR *name)
{
    NameValuePairNode *nvpn = config->params;
    while (NULL != nvpn && 0 != wcsicmp(nvpn->name, name))
        nvpn = nvpn->next;
    return nvpn;
}

WCHAR *
get_config_group_parameter(ConfigGroup *config, const WCHAR *name)
{
    NameValuePairNode *nvpn = get_nvp_node(config, name);
    return (NULL == nvpn) ? NULL : nvpn->value;
}

NameValuePairNode *
add_nvp_node(ConfigGroup *config, const WCHAR *name)
{
    NameValuePairNode *nvpn = new_nvp_node(name);
    nvpn->next = config->params;
    config->params = nvpn;
    return nvpn;
}

void
set_config_group_parameter(ConfigGroup *config, const WCHAR *name, const WCHAR *value)
{
    NameValuePairNode *nvpn = get_nvp_node(config, name);

    if (NULL == nvpn) {
        nvpn = add_nvp_node(config, name);
    }

    if (NULL != nvpn->value) {
        free(nvpn->value);
        nvpn->value = NULL;
    }

    nvpn->value = (NULL == value) ? NULL : wcsdup(value);
}

void
remove_config_group_parameter(ConfigGroup *config, const WCHAR *name)
{
    NameValuePairNode *cur = NULL, *prev = NULL;

    if (NULL == config || NULL == name)
        return;

    for (cur = config->params; NULL != cur; cur = cur->next) {
        if (0 == wcsicmp(cur->name, name)) {
            if (NULL == prev)
                config->params = cur->next;
            else
                prev->next = cur->next;

            free_nvp(cur);

            return;
        }
        prev = cur;
    }
}

BOOL
get_config_group_parameter_bool(ConfigGroup *config, const WCHAR *name)
{
    WCHAR *val = get_config_group_parameter(config, name);
    return (val != NULL && 0 == wcscmp(val, L"TRUE"));
}

int
get_config_group_parameter_int(ConfigGroup *config, const WCHAR *name)
{
    WCHAR *val = get_config_group_parameter(config, name);
    return val == NULL ? 0 : _wtoi(val);
}

void
set_config_group_parameter_bool(ConfigGroup *config, const WCHAR *name, BOOL value)
{
    set_config_group_parameter(config, name, value ? L"TRUE" : L"FALSE");
}

void
set_config_group_parameter_int(ConfigGroup *config, const WCHAR *name, int value)
{
    WCHAR buf[MAX_PATH];
    _snwprintf(buf, MAX_PATH, L"%d", value);
    set_config_group_parameter(config, name, buf);
}

void
set_config_group_parameter_ascii(ConfigGroup *config, const WCHAR *name, char *value)
{
    WCHAR buf[MAX_PATH];
    _snwprintf(buf, MAX_PATH, L"%S", value);
    buf[MAX_PATH - 1] = L'\0';
    set_config_group_parameter(config, name, buf);
}

void
set_config_group_parameter_scrambled(ConfigGroup *config, const WCHAR *name,
                                     const WCHAR *value)
{
    /* this swaps high and low bytes of WCHAR. obviously not strong,
     *  just meant to be something other than plaintext. */
    WCHAR buf[MAX_PATH];
    UINT i = 0;
    while (value[i] != 0 && i < MAX_PATH - 1) {
        buf[i] = (WCHAR)(((value[i] & 0x00ff) << 8) + ((value[i] & 0xff00) >> 8));
        i++;
    }
    buf[i] = L'\0';
    set_config_group_parameter(config, name, buf);
}

void
get_config_group_parameter_scrambled(ConfigGroup *config, const WCHAR *name,
                                     WCHAR *buffer, UINT maxchars)
{
    WCHAR *value = get_config_group_parameter(config, name);
    UINT i = 0;

    if (value == NULL) {
        buffer[0] = L'\0';
        return;
    }

    while (value[i] != 0 && i < maxchars - 1) {
        buffer[i] = (WCHAR)(((value[i] & 0x00ff) << 8) + ((value[i] & 0xff00) >> 8));
        i++;
    }
    buffer[i] = L'\0';
}

void
add_config_group(ConfigGroup *parent, ConfigGroup *new_child)
{
    if (NULL != get_child(new_child->name, parent)) {
        DO_DEBUG(DL_WARN, printf("adding multiple child: %S\n", new_child->name););
    }

    if (parent->children == NULL)
        parent->children = new_child;

    if (parent->lastchild != NULL)
        parent->lastchild->next = new_child;

    parent->lastchild = new_child;
    new_child->next = NULL;
}

typedef DWORD (*custom_config_read_handler)(ConfigGroup *config, const WCHAR *name,
                                            const WCHAR *value);

#    define CURRENT_MODES_VERSION 42000
#    define MAX_MODES_FILE_SIZE (64 * 1024)

DWORD
custom_hotp_modes_read_handler(ConfigGroup *config, const WCHAR *name, const WCHAR *value)
{
    char modes_file[MAX_MODES_FILE_SIZE];
    WCHAR modes_path[MAX_PATH];
    WCHAR parambuf[MAX_PATH];
    WCHAR valbuf[MAX_PATH];
    DWORD res;
    SIZE_T needed = 0;
    BOOL done = FALSE;
    char *modes_line;
    ConfigGroup *hotp_config;

    if (name == NULL || value == NULL)
        return ERROR_SUCCESS;

    /* read in modes file */
    _snwprintf(modes_path, MAX_PATH, L"%s\\%d\\%S", value, CURRENT_MODES_VERSION,
               HOTP_MODES_FILENAME);
    modes_path[MAX_PATH - 1] = L'\0';

    res = read_file_contents(modes_path, modes_file, MAX_MODES_FILE_SIZE, &needed);

    /* if the modes file isn't there, no worries; assume no modes */
    if (ERROR_FILE_NOT_FOUND == res)
        return ERROR_SUCCESS;

    if (ERROR_SUCCESS != res)
        return res;

    if (needed > MAX_MODES_FILE_SIZE - 2)
        return ERROR_INSUFFICIENT_BUFFER;

    /* create child config group */
    hotp_config = new_config_group(L_DYNAMORIO_VAR_HOT_PATCH_MODES);

    /* modes_file has contents, now sscanf and set all nvp. */
    modes_line = parse_line_sep(modes_file, ':', &done, parambuf, valbuf, MAX_PATH);
    DO_DEBUG(DL_VERB, printf("hotp modes first line %S:%S\n", parambuf, valbuf););

    /* expect num lines first */
    if (valbuf[0] != '\0')
        return ERROR_PARSE_ERROR;

    while (!done) {
        modes_line = parse_line_sep(modes_line, ':', &done, parambuf, valbuf, MAX_PATH);
        if (done)
            break;
        set_config_group_parameter(hotp_config, parambuf, valbuf);
    }

    add_config_group(config, hotp_config);

    return ERROR_SUCCESS;
}

custom_config_read_handler
get_custom_config_read_handler(const WCHAR *name)
{
    if (name == NULL)
        return NULL;

    if (0 == wcscmp(name, L_DYNAMORIO_VAR_HOT_PATCH_MODES)) {
        return (&custom_hotp_modes_read_handler);
    }

    return NULL;
}

static DWORD
read_config_group_from_registry(HKEY parent, ConfigGroup **configptr, const WCHAR *name,
                                BOOL recursive)
{
    HKEY config_key = NULL;
    ConfigGroup *config = NULL;
    WCHAR keyname[MAX_PATH];
    WCHAR keyvalue[MAX_PARAM_LEN];
    DWORD idx, keySz, valSz, res = ERROR_SUCCESS;
    FILETIME writetime;
    custom_config_read_handler handler;

    if (name == NULL) {
        config_key = parent;
    } else {
        WCHAR translated_name[MAX_PATH];
        wcsncpy(translated_name, name, MAX_PATH);
        configpath_to_registry_path(translated_name);
        res = RegOpenKeyEx(parent, translated_name, 0, platform_key_flags() | KEY_READ,
                           &config_key);
        if (res != ERROR_SUCCESS)
            goto read_config_out;
    }

    config = new_config_group(name);

    /* first read in all values */
    for (idx = 0; /* TRUE */; idx++) {
        keySz = MAX_PATH;
        valSz = sizeof(keyvalue);
        res = RegEnumValue(config_key, idx, keyname, &keySz, 0, NULL, (LPBYTE)keyvalue,
                           &valSz);
        if (res == ERROR_NO_MORE_ITEMS) {
            res = ERROR_SUCCESS;
            break;
        } else if (res != ERROR_SUCCESS) {
            goto read_config_out;
        }

        if (NULL != (handler = get_custom_config_read_handler(keyname))) {
            res = handler(config, keyname, keyvalue);
            if (res != ERROR_SUCCESS)
                goto read_config_out;
        } else {
            set_config_group_parameter(config, keyname, keyvalue);
        }
    }

    /* and read in all children (if desired) */
    for (idx = 0; recursive; idx++) {
        ConfigGroup *child_config;

        keySz = MAX_PATH;
        res = RegEnumKeyEx(config_key, idx, keyname, &keySz, 0, NULL, NULL, &writetime);
        if (res == ERROR_NO_MORE_ITEMS) {
            res = ERROR_SUCCESS;
            break;
        } else if (res != ERROR_SUCCESS) {
            goto read_config_out;
        }

        res = read_config_group_from_registry(config_key, &child_config, keyname,
                                              recursive);
        if (res != ERROR_SUCCESS)
            return res;

        add_config_group(config, child_config);
    }

    *configptr = config;

read_config_out:

    if (NULL != config_key && NULL != name)
        RegCloseKey(config_key);

    return res;
}

DWORD
read_config_group(ConfigGroup **configptr, const WCHAR *name,
                  BOOL read_children_recursively)
{
    HKEY rootkey;
    DWORD res = ERROR_SUCCESS;

    DO_ASSERT(configptr != NULL);
    DO_ASSERT(name != NULL);
    *configptr = NULL;

    res = get_key_handle(&rootkey, NULL, NULL, FALSE, KEY_READ);
    if (res != ERROR_SUCCESS)
        return res;

    res = read_config_group_from_registry(rootkey, configptr, name,
                                          read_children_recursively);

    RegCloseKey(rootkey);

    return res;
}

typedef DWORD (*custom_config_write_handler)(HKEY parent, ConfigGroup *config,
                                             ConfigGroup *config_parent);

DWORD
custom_hotp_modes_write_handler(HKEY parent, ConfigGroup *config,
                                ConfigGroup *config_parent)
{
    /* need to write modes file path to key, and the modes file
     * contents. */
    DWORD res;
    WCHAR modes_file[MAX_PATH];
    WCHAR modes_key[MAX_PATH];
    char modes_file_contents[MAX_MODES_FILE_SIZE];
    char modes_file_line[MAX_PATH];
    BOOL changed = FALSE;
    NameValuePairNode *nvpn;

    /* the modes file name is based on the parent ConfigGroup; if the
     *  parent config is the root, then the modes file goes in the
     *  top-level config dir, otherwise it goes in an app-specific
     *  config dir. */
    if (0 == wcscmp(L_PRODUCT_NAME, config_parent->name)) {
        _snwprintf(modes_key, MAX_PATH, L"%s\\config", get_dynamorio_home());
    } else {
        _snwprintf(modes_key, MAX_PATH, L"%s\\config\\%s", get_dynamorio_home(),
                   config_parent->name);
    }
    modes_key[MAX_PATH - 1] = L'\0';

    /* now write modes file content; name=patch ID, value=mode */
    _snprintf(modes_file_contents, MAX_MODES_FILE_SIZE, "%d\n", count_params(config));
    modes_file_contents[MAX_MODES_FILE_SIZE - 1] = '\0';
    for (nvpn = config->params; NULL != nvpn; nvpn = nvpn->next) {
        _snprintf(modes_file_line, MAX_PATH, "%S:%S\n", nvpn->name, nvpn->value);
        modes_file_line[MAX_PATH - 1] = '\0';
        strncat(modes_file_contents, modes_file_line,
                MAX_MODES_FILE_SIZE - strlen(modes_file_contents));
        modes_file_contents[MAX_MODES_FILE_SIZE - 1] = '\0';
    }

    if (strlen(modes_file_contents) > MAX_MODES_FILE_SIZE - 2) {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    /* first, mkdir -p */
    _snwprintf(modes_file, MAX_PATH, L"%s\\%d\\%S", modes_key, CURRENT_MODES_VERSION,
               HOTP_MODES_FILENAME);
    modes_file[MAX_PATH - 1] = L'\0';
    ensure_directory_exists_for_file(modes_file);

    /* and then write file */
    res = write_file_contents_if_different(modes_file, modes_file_contents, &changed);
    if (res != ERROR_SUCCESS)
        return res;

    /* finally, write the filename to the modes key */
    return write_reg_string(parent, L_DYNAMORIO_VAR_HOT_PATCH_MODES, modes_key);
}

custom_config_write_handler
get_custom_config_write_handler(ConfigGroup *config)
{
    if (config == NULL || config->name == NULL)
        return NULL;

    if (0 == wcscmp(config->name, L_DYNAMORIO_VAR_HOT_PATCH_MODES)) {
        return (&custom_hotp_modes_write_handler);
    }

    return NULL;
}

DWORD
write_config_group_to_registry(HKEY parent, ConfigGroup *config,
                               ConfigGroup *parent_config)
{
    HKEY config_key = NULL;
    NameValuePairNode *nvpn;
    DWORD res = ERROR_SUCCESS;
    custom_config_write_handler handler = NULL;

    if (NULL == config->name) {
        config_key = parent;
    } else if (NULL != (handler = get_custom_config_write_handler(config))) {
        res = handler(parent, config, parent_config);
        if (res != ERROR_SUCCESS)
            goto write_config_out;
        /* only continue down the chain if custom-handled */
        if (NULL != config->next) {
            res = write_config_group_to_registry(parent, config->next, parent_config);
        }
        goto write_config_out;
    } else {
        /* open or create */
        res = get_key_handle(&config_key, parent, config->name, FALSE,
                             KEY_WRITE | KEY_ENUMERATE_SUB_KEYS);
        if (res != ERROR_SUCCESS)
            goto write_config_out;
    }

    /* write the NVP's */
    for (nvpn = config->params; NULL != nvpn; nvpn = nvpn->next) {
        /* hook for forced deletion of a key */
        if (0 == wcscmp(nvpn->value, L_DELETE_PARAMETER_KEY)) {
            RegDeleteValue(config_key, nvpn->name);
            res = ERROR_SUCCESS;
        } else {
            res = write_reg_string(config_key, nvpn->name, nvpn->value);
        }
        if (res != ERROR_SUCCESS)
            goto write_config_out;
    }

    /* write the children */
    if (NULL != config->children) {
        res = write_config_group_to_registry(config_key, config->children, config);
        if (res != ERROR_SUCCESS)
            goto write_config_out;
    }

    /* continue down the chain */
    if (NULL != config->next) {
        res = write_config_group_to_registry(parent, config->next, parent_config);
        if (res != ERROR_SUCCESS)
            goto write_config_out;
    }

write_config_out:

    if (NULL != config_key && NULL != config->name)
        RegCloseKey(config_key);

    return res;
}

/*
 * this enforces that the key tree matches the config group exactly,
 * e.g., all keys/values are deleted that do not appear in 'filter'
 *
 * this complexity is necessary in order to allow
 * write_config_group_to_registry to be atomic, in the sense that
 * config information is updated on a key-by-key atomic basis.
 */
DWORD
recursive_delete_key(HKEY parent, const WCHAR *keyname, ConfigGroup *filter)
{
    HKEY subkey = NULL;
    WCHAR subkeyname[MAX_PATH];
    FILETIME writetime;
    DWORD res, keySz, idx;
    ConfigGroup *child;

    res = get_key_handle(&subkey, parent, keyname, TRUE, KEY_WRITE | KEY_READ);
    if (res != ERROR_SUCCESS)
        goto recursive_delete_out;

    /* and read in all children (if desired) */
    for (idx = 0;;) {
        /* note that since deleting, we always pass index 0 */
        keySz = MAX_PATH;
        res = RegEnumKeyEx(subkey, idx, subkeyname, &keySz, 0, NULL, NULL, &writetime);
        if (res == ERROR_NO_MORE_ITEMS) {
            res = ERROR_SUCCESS;
            break;
        } else if (res != ERROR_SUCCESS) {
            goto recursive_delete_out;
        }

        /* note that recursive_delete_key will only delete the entire
         *  subkey if it's not a child (eg, if the 'filter' parameter
         *  is NULL) */
        child = get_child(subkeyname, filter);
        res = recursive_delete_key(subkey, subkeyname, child);
        if (res != ERROR_SUCCESS)
            goto recursive_delete_out;

        /* increment the index if we're not deleting the key */
        if (NULL != child)
            idx++;
    }

    if (NULL != filter) {
        /* prune values too */
        for (idx = 0;;) {
            keySz = MAX_PATH;
            res = RegEnumValue(subkey, idx, subkeyname, &keySz, 0, NULL, NULL, NULL);
            if (res == ERROR_NO_MORE_ITEMS) {
                res = ERROR_SUCCESS;
                break;
            } else if (res != ERROR_SUCCESS) {
                goto recursive_delete_out;
            }

            /* don't delete if the param exists, OR if this is a custom
             * handled name and the child exists. */
            if (is_param(subkeyname, filter) ||
                (get_child(subkeyname, filter) &&
                 get_custom_config_read_handler(subkeyname))) {
                /* if we're not deleting, increment the index */
                idx++;
            } else {
                res = RegDeleteValue(subkey, subkeyname);
                if (res != ERROR_SUCCESS)
                    goto recursive_delete_out;
            }
        }
    }

    RegCloseKey(subkey);
    subkey = NULL;

    /* only delete the key itself if it's not being filtered out. */
    if (filter == NULL) {
        /* PR 244206: we assume we're only going to recursively delete keys in
         * HKLM\Software\Company
         */
        res = delete_product_key(parent, keyname);
    }

recursive_delete_out:

    if (subkey != NULL)
        RegCloseKey(subkey);

    return res;
}

DWORD
write_config_group(ConfigGroup *config)
{
    HKEY rootkey;
    DWORD res;

    res = get_key_handle(&rootkey, NULL, NULL, FALSE, KEY_WRITE | KEY_ENUMERATE_SUB_KEYS);
    if (res != ERROR_SUCCESS)
        return res;

    res = write_config_group_to_registry(rootkey, config, NULL);

    /* prune if necessary */
    if (ERROR_SUCCESS == res && config->should_clear) {
        res = recursive_delete_key(rootkey, config->name, config);
    }

    RegCloseKey(rootkey);

    return res;
}

/* single parameter config functions */

DWORD
set_config_parameter(const WCHAR *path, BOOL absolute, const WCHAR *name,
                     const WCHAR *value)
{
    HKEY rootkey;
    DWORD res;

    res = get_key_handle(&rootkey, NULL, path, absolute, KEY_WRITE);
    if (res != ERROR_SUCCESS)
        return res;

    res = write_reg_string(rootkey, name, value);

    RegCloseKey(rootkey);

    return res;
}

DWORD
get_config_parameter(const WCHAR *path, BOOL absolute, const WCHAR *name, WCHAR *value,
                     int maxlen)
{
    HKEY rootkey;
    DWORD res;

    res = get_key_handle(&rootkey, NULL, path, absolute, KEY_READ);
    if (res != ERROR_SUCCESS)
        return res;

    res = read_reg_string(rootkey, name, value, maxlen);

    RegCloseKey(rootkey);

    return res;
}

DWORD
read_reg_string(HKEY subkey, const WCHAR *keyname, WCHAR *value, int valchars)
{
    DWORD len = valchars * sizeof(WCHAR);
    return RegQueryValueEx(subkey, keyname, 0, NULL, (LPBYTE)value, &len);
}

/* if value is NULL, the key will be deleted */
DWORD
write_reg_string(HKEY subkey, const WCHAR *keyname, const WCHAR *value)
{
    if (value)
        return RegSetValueEx(subkey, keyname, 0, REG_SZ, (LPBYTE)value,
                             (DWORD)(wcslen(value) + 1) * sizeof(WCHAR));
    else
        return RegDeleteValue(subkey, keyname);
}

/* process identification routines */

/* tries both with and without no_strip */
ConfigGroup *
get_qualified_config_group(ConfigGroup *config, const WCHAR *exename,
                           const WCHAR *cmdline)
{
    WCHAR qname[MAX_PATH];
    ConfigGroup *c;

    /* need to try both with and without NO_STRIP! */
    _snwprintf(qname, MAX_PATH, L"%s-", exename);
    if (get_commandline_qualifier(cmdline, qname + wcslen(qname),
                                  (UINT)(MAX_PATH - wcslen(qname)), FALSE)) {
        c = get_child(qname, config);
        if (NULL != c)
            return c;
    }

    _snwprintf(qname, MAX_PATH, L"%s-", exename);
    if (get_commandline_qualifier(cmdline, qname + wcslen(qname),
                                  (UINT)(MAX_PATH - wcslen(qname)), TRUE)) {
        c = get_child(qname, config);
        if (NULL != c)
            return c;
    }

    return NULL;
}

BOOL
is_parent_of_qualified_config_group(ConfigGroup *config)
{
    WCHAR *run_under = NULL;

    if (config == NULL)
        return FALSE;
    run_under = get_config_group_parameter(config, L_DYNAMORIO_VAR_RUNUNDER);
    if (run_under == NULL)
        return FALSE;

    return TEST(RUNUNDER_COMMANDLINE_DISPATCH, _wtoi(run_under));
}

ConfigGroup *
get_process_config_group(ConfigGroup *config, process_id_t pid)
{
    WCHAR buf[MAX_PATH];
    DWORD res;
    ConfigGroup *c;

    res = get_process_name(pid, buf, MAX_PATH);

    if (res != ERROR_SUCCESS)
        return NULL;

    wcstolower(buf);
    c = get_child(buf, config);

    if (c == NULL || is_parent_of_qualified_config_group(c)) {
        WCHAR cmdlbuf[MAX_PATH];
        ConfigGroup *qualified;
        get_process_cmdline(pid, cmdlbuf, MAX_PATH);
        qualified = get_qualified_config_group(config, buf, cmdlbuf);
        if (NULL != qualified)
            return qualified;
    }

    return c;
}

/******************
 * list functions *
 ******************/

/*
 * given a ;-separated list and a filename, return a pointer to
 *  the filename in the list, if it appears. comparisons are
 *  case insensitive and independent of path; eg,
 *     get_entry_location("c:\\foo\\bar.dll;blah;...", "D:\\Bar.DLL")
 *  would return a pointer to the beginning of the list.
 */
WCHAR *
get_entry_location(const WCHAR *list, const WCHAR *filename, WCHAR separator)
{
    WCHAR *lowerlist, *lowername, *entry;

    wcstolower(lowerlist = wcsdup(list));
    wcstolower(lowername = wcsdup(get_exename_from_path(filename)));

    entry = wcsstr(lowerlist, lowername);

    while (NULL != entry) {
        WCHAR last = *(entry + wcslen(lowername));

        /* make sure it's not just a substring */
        if ((last == separator || last == L'\0') &&
            (entry == lowerlist || *(entry - 1) == separator || *(entry - 1) == L'\\' ||
             *(entry - 1) == L'/')) {
            /* everything's cool; now put the path back in and translate */
            WCHAR *cur = lowerlist;
            while (NULL != wcschr(cur, separator) && wcschr(cur, separator) < entry)
                cur = wcschr(cur, separator) + 1;
            entry = ((WCHAR *)list) + (cur - lowerlist);
            break;
        } else {
            entry = wcsstr(entry + 1, lowername);
        }
    }

    free(lowername);
    free(lowerlist);

    return entry;
}

BOOL
is_in_file_list(const WCHAR *list, const WCHAR *filename, WCHAR separator)
{
    return NULL != get_entry_location(list, filename, separator);
}

/* returns a new list which needs to be freed, and frees old list */
WCHAR *
add_to_file_list(WCHAR *list, const WCHAR *filename, BOOL check_for_duplicates,
                 BOOL add_to_front, BOOL overwrite_existing, WCHAR separator)
{
    WCHAR *new_list;
    SIZE_T new_list_size;

    if (NULL == list || 0 == wcslen(list))
        return wcsdup(filename);

    if (overwrite_existing)
        remove_from_file_list(list, filename, separator);

    if (check_for_duplicates && is_in_file_list(list, filename, separator))
        return list;

    new_list_size = wcslen(list) + wcslen(filename) + 2;

    new_list = (WCHAR *)malloc(sizeof(WCHAR) * new_list_size);

    if (wcslen(list) == 0) {
        wcsncpy(new_list, filename, new_list_size);
    } else {
        _snwprintf(new_list, new_list_size, L"%s%c%s", add_to_front ? filename : list,
                   separator, add_to_front ? list : filename);
    }

    free(list);

    return new_list;
}

/* provide these helpers to avoid multiple libc problems */
WCHAR *
new_file_list(SIZE_T initial_chars)
{
    WCHAR *list = (WCHAR *)malloc(sizeof(WCHAR) * (1 + initial_chars));
    *list = L'\0';
    return list;
}

void
free_file_list(WCHAR *list)
{
    free(list);
}

/* removes all occurrences */
void
remove_from_file_list(WCHAR *list, const WCHAR *filename, WCHAR separator)
{
    WCHAR *entry, *end_of_entry;

    entry = get_entry_location(list, filename, separator);
    if (NULL == entry)
        return;

    end_of_entry = wcschr(entry, separator);

    if (NULL == end_of_entry) {
        if (entry == list)
            *entry = L'\0';
        else
            *(entry - 1) = L'\0';
    } else {
        /* note the whole wcslen(end_of_entry) to get the null
         *  terminator (since we're doing end_of_entry + 1)
         *
         * note that memmove handles appropriately the case
         *  where the src and dest regions overlap. */
        memmove(entry, end_of_entry + 1, (wcslen(end_of_entry)) * sizeof(WCHAR));
    }

    /* recurse for multiple occurrences */
    remove_from_file_list(list, filename, separator);
}

#    define BLACK 0
#    define WHITE 1

BOOL
filter(WCHAR *list, const WCHAR *filter, DWORD black_or_white, BOOL check_only,
       WCHAR separator)
{
    WCHAR *working_list, *cur, *next;
    BOOL satisfied = TRUE, remove_entry;

    if (list == NULL || wcslen(list) == 0)
        return TRUE;

    next = cur = working_list = wcsdup(list);

    do {

        if (NULL != next)
            next = wcschr(cur, separator);

        if (NULL != next)
            *next = L'\0';

        cur = get_exename_from_path(cur);
        remove_entry =
            !(is_in_file_list(filter, cur, separator) ^ (BLACK == black_or_white));

        if (remove_entry) {
            if (check_only)
                return FALSE;
            else
                remove_from_file_list(list, cur, separator);

            satisfied = FALSE;
        }

        cur = next + 1;

    } while (NULL != next);

    free(working_list);

    return satisfied;
}

BOOL
blocklist_filter(WCHAR *list, const WCHAR *blocklist, BOOL check_only, WCHAR separator)
{
    return filter(list, blocklist, BLACK, check_only, separator);
}

BOOL
allowlist_filter(WCHAR *list, const WCHAR *allowlist, BOOL check_only, WCHAR separator)
{
    return filter(list, allowlist, WHITE, check_only, separator);
}

/* appinit key */

DWORD
set_autoinjection_ex(BOOL inject, DWORD flags, const WCHAR *blocklist,
                     const WCHAR *allowlist,
                     /* OUT */ DWORD *list_error, const WCHAR *custom_preinject_name,
                     /* OUT */ WCHAR *current_list, SIZE_T maxchars)
{
    WCHAR curlist[MAX_PARAM_LEN];
    WCHAR preinject_name[MAX_PATH];
    WCHAR *list;
    DWORD res;
    BOOL list_ok_ = TRUE;
    BOOL using_system32 = using_system32_for_preinject(custom_preinject_name);

    res = get_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, curlist,
                               MAX_PARAM_LEN);
    if (res != ERROR_SUCCESS) {
        /* if it's not there, then we create it */
        curlist[0] = L'\0';
    }

    list = new_file_list(wcslen(curlist) + 1);
    wcsncpy(list, curlist, wcslen(curlist) + 1);

    if (NULL != current_list)
        wcsncpy(current_list, curlist, maxchars);

    if (using_system32 && TEST(APPINIT_SYS32_CLEAR_OTHERS, flags)) {
        /* if we're using system32, and the clear flag is set, clear it */
        curlist[0] = L'\0';
    }

    if (NULL == custom_preinject_name) {
        res = get_preinject_name(preinject_name, MAX_PATH);
        if (res != ERROR_SUCCESS)
            return res;
    } else {
        wcsncpy(preinject_name, custom_preinject_name, MAX_PATH);
    }

    /* if using system32, make sure to copy the DLL there from
     *  the standard place (or remove it if we're turning off) */
    if (using_system32) {
        WCHAR src_path[MAX_PATH], dst_path[MAX_PATH];

        if (NULL == custom_preinject_name) {
            res = get_preinject_path(src_path, MAX_PATH, TRUE, TRUE);
            if (res != ERROR_SUCCESS)
                return res;
            wcsncat(src_path, L"\\" L_EXPAND_LEVEL(INJECT_DLL_NAME),
                    MAX_PATH - wcslen(src_path));
        } else {
            wcsncpy(src_path, custom_preinject_name, MAX_PATH);
        }

        get_preinject_path(dst_path, MAX_PATH, FALSE, TRUE);
        if (res != ERROR_SUCCESS)
            return res;
        wcsncat(dst_path, L"\\" L_EXPAND_LEVEL(INJECT_DLL_NAME),
                MAX_PATH - wcslen(dst_path));

        if (inject) {
            /* We used to only copy if (!file_exists(dst_path))
             * but it seems that we should clobber to avoid
             * upgrade issues, at risk of messing up another product
             * w/ a dll of the same name.
             */
            CopyFile(src_path, dst_path, FALSE /*don't fail if exists*/);
        } else {
            /* FIXME: do we want to use delete_file_rename_in_use?
             * this should only be called at uninstall or by
             *  tools users, so there shouldn't be any problem
             *  leaving one .tmp file around...
             * alternatively, we could try renaming to a path in
             *  our installation directory, which would be
             *  wiped out on installation. however, it gets
             *  complicated if our installation folder is on
             *  a different volume.
             * another option is to rename to %SYSTEM32%\..\TEMP,
             *  which should always exist i think...
             */
            DeleteFile(dst_path);
        }

        /* now we want just the name, since in system32 */
        if (get_exename_from_path(preinject_name) != NULL) {
            wcsncpy(preinject_name, get_exename_from_path(preinject_name),
                    BUFFER_SIZE_ELEMENTS(preinject_name));
        }
    }

    if (inject) {
        BOOL force_overwrite;
        WCHAR *old = get_entry_location(list, preinject_name, APPINIT_SEPARATOR_CHAR);

        /* first, if there's something there, make sure it exists. if
         *  not, remove it before proceeding (to ensure overwrite)
         * cf case 4053 */
        if (NULL != old) {
            if (using_system32) {
                /* PR 232765: we want to replace any full path w/ filename */
                force_overwrite = true;
            } else {
                WCHAR *firstsep, tmpbuf[MAX_PATH];
                wcsncpy(tmpbuf, old, MAX_PATH);
                firstsep = wcschr(tmpbuf, APPINIT_SEPARATOR_CHAR);
                if (NULL != firstsep)
                    *firstsep = L'\0';
                force_overwrite = !file_exists(tmpbuf);
            }
        } else {
            /* force overwrite if someone cared enough to set one of these */
            force_overwrite =
                TEST((APPINIT_FORCE_TO_FRONT | APPINIT_FORCE_TO_BACK), flags);
        }
        /* always overwrite if asked to */
        force_overwrite = force_overwrite | TEST(APPINIT_OVERWRITE, flags);

        /* add !TEST(APPINIT_FORCE_TO_BACK, flags) so that we favor adding
         *  to the front if neither are set. */
        list = add_to_file_list(list, preinject_name, TRUE,
                                TEST(APPINIT_FORCE_TO_FRONT, flags) ||
                                    !TEST(APPINIT_FORCE_TO_BACK, flags),
                                force_overwrite, APPINIT_SEPARATOR_CHAR);
    } else {
        remove_from_file_list(list, preinject_name, APPINIT_SEPARATOR_CHAR);
    }

    if (TEST(APPINIT_USE_ALLOWLIST, flags)) {
        if (NULL == allowlist)
            res = ERROR_INVALID_PARAMETER;
        else
            list_ok_ =
                allowlist_filter(list, allowlist, TEST(APPINIT_CHECK_LISTS_ONLY, flags),
                                 APPINIT_SEPARATOR_CHAR);
    } else if (TEST(APPINIT_USE_BLOCKLIST, flags)) {
        /* else if since allowlist subsumes blocklist */
        if (NULL == blocklist)
            res = ERROR_INVALID_PARAMETER;
        else
            list_ok_ =
                blocklist_filter(list, blocklist, TEST(APPINIT_CHECK_LISTS_ONLY, flags),
                                 APPINIT_SEPARATOR_CHAR);
    }

    if (list_error != NULL && !list_ok_)
        *list_error = ERROR_LIST_VIOLATION;

    if (res == ERROR_INVALID_PARAMETER)
        return res;

    if (!list_ok_ && TEST(APPINIT_BAIL_ON_LIST_VIOLATION, flags))
        remove_from_file_list(list, preinject_name, APPINIT_SEPARATOR_CHAR);

    /* now, system32 flag checks */
    if (using_system32) {

        /* not yet supported */
        if (TEST(APPINIT_SYS32_USE_LENGTH_WORKAROUND, flags))
            return ERROR_INVALID_PARAMETER;

        if (wcslen(list) > APPINIT_SYSTEM32_LENGTH_LIMIT) {

            if (list_error != NULL)
                *list_error = ERROR_LENGTH_VIOLATION;

            if (TEST(APPINIT_SYS32_FAIL_ON_LENGTH_ERROR, flags)) {
                remove_from_file_list(list, preinject_name, APPINIT_SEPARATOR_CHAR);
            } else {
                /* truncate, if the flags specify it. */
                if (TEST(APPINIT_SYS32_TRUNCATE, flags)) {
                    list[APPINIT_SYSTEM32_LENGTH_LIMIT] = L'\0';
                }
            }
        }
    }

    /* only write if it's changed */
    if (0 != wcscmp(list, curlist)) {
        res = set_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, list);
        if (res != ERROR_SUCCESS)
            return res;
    }

    free_file_list(list);

    return ERROR_SUCCESS;
}

DWORD
set_custom_autoinjection(const WCHAR *preinject, DWORD flags)
{
    return set_autoinjection_ex(TRUE, flags, NULL, NULL, NULL, preinject, NULL, 0);
}

DWORD
set_autoinjection()
{
    return set_autoinjection_ex(TRUE, 0, NULL, NULL, NULL, NULL, NULL, 0);
}

DWORD
unset_custom_autoinjection(const WCHAR *preinject, DWORD flags)
{
    return set_autoinjection_ex(FALSE, flags, NULL, NULL, NULL, preinject, NULL, 0);
}

DWORD
unset_autoinjection()
{
    return set_autoinjection_ex(FALSE, 0, NULL, NULL, NULL, NULL, NULL, 0);
}

/* NOTE: that this returns the current status -- which on NT
 *  is not necessarily the actual status, which is cached by the
 *  os at boot time.
 * FIXME: add a helper method for determining the status of
 *  appinit that is being used for the current boot session. */
BOOL
is_autoinjection_set()
{
    WCHAR list[MAX_PARAM_LEN], preinject[MAX_PATH];
    DWORD res;

    res = get_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, list,
                               MAX_PARAM_LEN);
    if (res != ERROR_SUCCESS)
        return FALSE;

    res = get_preinject_name(preinject, MAX_PATH);
    if (res != ERROR_SUCCESS)
        return FALSE;

    return is_in_file_list(list, preinject, APPINIT_SEPARATOR_CHAR);
}

BOOL
is_custom_autoinjection_set(const WCHAR *preinject)
{
    WCHAR list[MAX_PARAM_LEN];
    DWORD res;

    res = get_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, list,
                               MAX_PARAM_LEN);
    if (res != ERROR_SUCCESS)
        return FALSE;
    return is_in_file_list(list, preinject, APPINIT_SEPARATOR_CHAR);
}

/* Returns true for Vista or later, including Windows 7 */
BOOL
is_vista()
{
    DWORD platform = 0;
    get_platform(&platform);
    return (platform >= PLATFORM_VISTA);
}

BOOL
is_win7()
{
    DWORD platform = 0;
    get_platform(&platform);
    return (platform >= PLATFORM_WIN_7);
}

/* Also disables reqt for signature on lib for win7+ */
DWORD
set_loadappinit_value(DWORD value)
{
    HKEY rootkey;
    DWORD res;

    if (!is_vista())
        return ERROR_UNSUPPORTED_OS;

    res = get_key_handle(&rootkey, INJECT_ALL_HIVE, INJECT_ALL_KEY_L, TRUE, KEY_WRITE);
    if (res != ERROR_SUCCESS)
        return res;

    res = RegSetValueEx(rootkey, INJECT_ALL_LOAD_SUBKEY_L, 0, REG_DWORD, (LPBYTE)&value,
                        sizeof(value));
    if (res != ERROR_SUCCESS)
        return res;

    if (is_win7()) {
        /* Disable the reqt for a signature.
         * FIXME i#323: better to sign drpreinject so we don't have to
         * relax security!
         */
        DWORD disable = 0;
        res = RegSetValueEx(rootkey, INJECT_ALL_SIGN_SUBKEY_L, 0, REG_DWORD,
                            (LPBYTE)&disable, sizeof(disable));
    }
    return res;
}

DWORD
set_loadappinit()
{
    return set_loadappinit_value(1);
}

DWORD
unset_loadappinit()
{
    return set_loadappinit_value(0);
}

BOOL
is_loadappinit_set()
{
    HKEY rootkey;
    DWORD value;
    DWORD size = sizeof(value);
    DWORD res;

    res = get_key_handle(&rootkey, INJECT_ALL_HIVE, INJECT_ALL_KEY_L, TRUE, KEY_READ);
    if (res != ERROR_SUCCESS)
        return res;

    res = RegQueryValueEx(rootkey, INJECT_ALL_LOAD_SUBKEY_L, NULL, NULL, (LPBYTE)&value,
                          &size);

    return (res == ERROR_SUCCESS && value != 0);
}

/* Deletes the product eventlog.  If no one else (nodemgr/pe) is using the eventlog also
 * deletes the base key and the eventlog file. */
DWORD
destroy_eventlog()
{
    DWORD res, res2;
    /* PR 244206: we don't need the wow64 flag since not HKLM\Software
     * (if we do want it, need to add on create/open as well, including
     * elm.c, which requires linking w/ utils.c)
     */
    res = RegDeleteKey(EVENTLOG_HIVE, L_EVENT_SOURCE_SUBKEY);
    res2 = RegDeleteKey(EVENTLOG_HIVE, L_EVENT_LOG_SUBKEY);
    if (res2 == ERROR_SUCCESS) {
        /* we deleted the top level key (which means it had no subkeys left) which means
         * no one else is using our eventlog, we go ahead and free the file. */
        WCHAR *file = is_vista() ? L_EVENT_FILE_NAME_VISTA : L_EVENT_FILE_NAME_PRE_VISTA;
        WCHAR *truncate_start;
        WCHAR file_exp[MAX_PATH];
        ExpandEnvironmentStrings(file, file_exp, BUFFER_SIZE_ELEMENTS(file_exp));
        NULL_TERMINATE_BUFFER(file_exp);
        if (file_exists(file_exp)) {
            /* If in use by the eventlog can't delete or re-name */
            if (!DeleteFile(file_exp))
                delete_file_on_boot(file_exp);
        }

        /* It appears the generated file is usually truncated to 8.3 by the eventlog
         * program (though apparently not always since some of my machines also have
         * the full name).  Will try to delete the truncated to 8.3 versions. */
        truncate_start = wcsrchr(file_exp, L'\\');
        if (truncate_start != NULL) {
            WCHAR *type = wcsrchr(file_exp, L'.');
            truncate_start += 8;
            if (type != NULL && truncate_start < type) {
                WCHAR tmp_buf[MAX_PATH];
                wcsncpy(tmp_buf, type, BUFFER_SIZE_ELEMENTS(tmp_buf));
                NULL_TERMINATE_BUFFER(tmp_buf);
                wcsncpy(truncate_start, tmp_buf,
                        BUFFER_SIZE_ELEMENTS(file_exp) - (truncate_start - file_exp));
                NULL_TERMINATE_BUFFER(file_exp);
                /* If in use by the eventlog can't delete or re-name */
                if (!DeleteFile(file_exp))
                    delete_file_on_boot(file_exp);
            }
        }
    }
    return res;
}

DWORD
create_eventlog(const WCHAR *dll_path)
{
    HKEY eventlog_key;
    HKEY eventsrc_key;
    WCHAR *wvalue;
    DWORD dvalue;
    DWORD res;

    res =
        get_key_handle(&eventlog_key, EVENTLOG_HIVE, L_EVENT_LOG_SUBKEY, TRUE, KEY_WRITE);
    if (res != ERROR_SUCCESS)
        return res;

    if (is_vista()) {
        wvalue = L_EVENT_FILE_NAME_VISTA;
    } else {
        wvalue = L_EVENT_FILE_NAME_PRE_VISTA;
    }
    /* REG_EXPAND_SZ since we use %systemroot% in the path which needs
     * to be expanded */
    res = RegSetValueEx(eventlog_key, L_EVENT_FILE_VALUE_NAME, 0, REG_EXPAND_SZ,
                        (LPBYTE)wvalue, (DWORD)(wcslen(wvalue) + 1) * sizeof(WCHAR));
    if (res != ERROR_SUCCESS)
        return res;

    dvalue = EVENT_MAX_SIZE;
    res = RegSetValueEx(eventlog_key, L_EVENT_MAX_SIZE_NAME, 0, REG_DWORD,
                        (LPBYTE)&dvalue, sizeof(dvalue));
    if (res != ERROR_SUCCESS)
        return res;

    dvalue = EVENT_RETENTION;
    res = RegSetValueEx(eventlog_key, L_EVENT_RETENTION_NAME, 0, REG_DWORD,
                        (LPBYTE)&dvalue, sizeof(dvalue));
    if (res != ERROR_SUCCESS)
        return res;

    res = get_key_handle(&eventsrc_key, EVENTLOG_HIVE, L_EVENT_SOURCE_SUBKEY, TRUE,
                         KEY_WRITE);
    if (res != ERROR_SUCCESS)
        return res;

    dvalue = EVENT_TYPES_SUPPORTED;
    res = RegSetValueEx(eventsrc_key, L_EVENT_TYPES_SUPPORTED_NAME, 0, REG_DWORD,
                        (LPBYTE)&dvalue, sizeof(dvalue));
    if (res != ERROR_SUCCESS)
        return res;

    dvalue = EVENT_CATEGORY_COUNT;
    res = RegSetValueEx(eventsrc_key, L_EVENT_CATEGORY_COUNT_NAME, 0, REG_DWORD,
                        (LPBYTE)&dvalue, sizeof(dvalue));
    if (res != ERROR_SUCCESS)
        return res;

    res = RegSetValueEx(eventsrc_key, L_EVENT_CATEGORY_FILE_NAME, 0, REG_SZ,
                        (LPBYTE)dll_path, (DWORD)(wcslen(dll_path) + 1) * sizeof(WCHAR));
    if (res != ERROR_SUCCESS)
        return res;

    res = RegSetValueEx(eventsrc_key, L_EVENT_MESSAGE_FILE, 0, REG_SZ, (LPBYTE)dll_path,
                        (DWORD)(wcslen(dll_path) + 1) * sizeof(WCHAR));
    if (res != ERROR_SUCCESS)
        return res;

    return ERROR_SUCCESS;
}

/* Copies the two drearlyhelper?.dlls located in "dir" to system32.
 * They are only needed on win2k, but it's up to the caller to check
 * for that.
 */
DWORD
copy_earlyhelper_dlls(const WCHAR *dir)
{
    WCHAR src_path[MAX_PATH], dst_path[MAX_PATH];
    DWORD len;

    /* We copy drearlyhelp2.dll first, to be on the safe side.
     * drearlyhelp1.dll has a dependency on drearlyhelp2.dll, so if #1 exists
     * and number 2 doesn't the loader will raise an error (and we're pre-image
     * entry point so it becomes a "process failed to initialize" error).
     */
    _snwprintf(src_path, BUFFER_SIZE_ELEMENTS(src_path), L"%s\\%s", dir,
               L_EXPAND_LEVEL(INJECT_HELPER_DLL2_NAME));
    NULL_TERMINATE_BUFFER(src_path);

    /* Get system32 path */
    len = GetSystemDirectory(dst_path, MAX_PATH);
    if (len == 0)
        return GetLastError();
    wcsncat(dst_path, L"\\" L_EXPAND_LEVEL(INJECT_HELPER_DLL2_NAME),
            BUFFER_SIZE_ELEMENTS(dst_path) - wcslen(dst_path));

    /* We could check file_exists(dst_path) but better to just
     * clobber so we can upgrade nicely (at risk of clobbering
     * some other product's same-name dll)
     */
    if (!CopyFile(src_path, dst_path, FALSE /*don't fail if exists*/))
        return GetLastError();

    _snwprintf(src_path, BUFFER_SIZE_ELEMENTS(src_path), L"%s\\%s", dir,
               L_EXPAND_LEVEL(INJECT_HELPER_DLL1_NAME));
    NULL_TERMINATE_BUFFER(src_path);

    /* Get system32 path, again: we could cache it */
    len = GetSystemDirectory(dst_path, MAX_PATH);
    if (len == 0)
        return GetLastError();
    wcsncat(dst_path, L"\\" L_EXPAND_LEVEL(INJECT_HELPER_DLL1_NAME),
            BUFFER_SIZE_ELEMENTS(dst_path) - wcslen(dst_path));

    if (!CopyFile(src_path, dst_path, FALSE /*don't fail if exists*/))
        return GetLastError();

    /* FIXME PR 232738: add a param for removing the files */

    return ERROR_SUCCESS;
}

void
dump_nvp(NameValuePairNode *nvpn)
{
    printf("%S=%S", nvpn->name, nvpn->value == NULL ? L"<null>" : nvpn->value);
}

void
dump_config_group(char *prefix, char *incr, ConfigGroup *c, BOOL traverse)
{
    NameValuePairNode *nvpn = NULL;

    printf("%sConfig Group: %S\n", prefix, c->name == NULL ? L"<null>" : c->name);
    printf("%sshould_clear: %d\n", prefix, c->should_clear);
    printf("%sparams:\n", prefix);
    for (nvpn = c->params; NULL != nvpn; nvpn = nvpn->next) {
        printf("%s%s%s", prefix, incr, incr);
        dump_nvp(nvpn);
        printf("\n");
    }
    if (c->children != NULL) {
        char p2[MAX_PATH];
        _snprintf(p2, MAX_PATH, "%s%s", prefix, incr);
        dump_config_group(p2, incr, c->children, TRUE);
    }
    if (traverse && c->next != NULL) {
        dump_config_group(prefix, incr, c->next, TRUE);
    }
}

#else // ifdef UNIT_TEST

int
main()
{
    ConfigGroup *c, *c2, *c3, *c4, *cp;
    DWORD res;
    WCHAR sep = LIST_SEPARATOR_CHAR;

    WCHAR *list1, *list2, *tmplist;

    WCHAR buf[MAX_PARAM_LEN];

    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    list1 = new_file_list(16);

    /* basic list operations */
    {
        DO_ASSERT_WSTR_EQ(L"", list1);

        list1 = add_to_file_list(list1, L"c:\\foo.dll", TRUE, TRUE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"c:\\foo.dll", list1);

        list1 = add_to_file_list(list1, L"C:\\bar.dll", TRUE, TRUE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;c:\\foo.dll", list1);

        list1 = add_to_file_list(list1, L"foo.dll", TRUE, TRUE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;c:\\foo.dll", list1);

        list1 = add_to_file_list(list1, L"C:\\shr\\Foo.dll", TRUE, TRUE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;c:\\foo.dll", list1);

        list1 = add_to_file_list(list1, L"C:\\shr\\Foo.dll", FALSE, TRUE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll", list1);

        list1 = add_to_file_list(list1, L"d:\\gee.dll", TRUE, FALSE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll", list1);

        list1 = add_to_file_list(list1, L"ar.dll", TRUE, FALSE, FALSE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll;ar.dll",
                          list1);
    }

    /* used in tests below */
    DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll;ar.dll",
                      list1);

    /* remove */
    {
        tmplist = wcsdup(list1);
        remove_from_file_list(tmplist, L"ar.dll", sep);
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll",
                          tmplist);

        remove_from_file_list(tmplist, L"foo.dll", sep);
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;d:\\gee.dll", tmplist);

        free_file_list(tmplist);

        tmplist = wcsdup(list1);
        remove_from_file_list(tmplist, L"Ar.dll", sep);
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll",
                          tmplist);

        remove_from_file_list(tmplist, L"Foo.DLL", sep);
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;d:\\gee.dll", tmplist);

        free_file_list(tmplist);
    }

    /* path checking */
    {
        tmplist = wcsdup(list1);
        tmplist = add_to_file_list(tmplist, L"C:\\Program Files\\blah\\blah\\foo.dll",
                                   TRUE, TRUE, TRUE, sep);
        DO_ASSERT_WSTR_EQ(
            L"C:\\Program Files\\blah\\blah\\foo.dll;C:\\bar.dll;d:\\gee.dll;ar.dll",
            tmplist);

        free_file_list(tmplist);
    }

    /* list filters */
    {
        list2 = new_file_list(1);
        list2 = add_to_file_list(list2, L"foo.dll", TRUE, TRUE, FALSE, sep);
        list2 = add_to_file_list(list2, L"gee.dll", TRUE, TRUE, FALSE, sep);

        tmplist = wcsdup(list1);
        DO_ASSERT(!blocklist_filter(tmplist, list2, TRUE, sep));
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;C:\\bar.dll;c:\\foo.dll;d:\\gee.dll;ar.dll",
                          tmplist);

        DO_ASSERT(!blocklist_filter(tmplist, list2, FALSE, sep));
        DO_ASSERT_WSTR_EQ(L"C:\\bar.dll;ar.dll", tmplist);

        DO_ASSERT(blocklist_filter(tmplist, list2, TRUE, sep));
        free_file_list(tmplist);

        tmplist = wcsdup(list1);
        DO_ASSERT(!allowlist_filter(tmplist, list2, FALSE, sep));
        DO_ASSERT_WSTR_EQ(L"C:\\shr\\Foo.dll;c:\\foo.dll;d:\\gee.dll", tmplist);

        DO_ASSERT(allowlist_filter(tmplist, list2, TRUE, sep));

        free_file_list(tmplist);

        free_file_list(list2);
    }

    free_file_list(list1);

    /* force to front */
    {
        list1 = L"home.dll;C:\\PROGRA~1\\DETERM~1\\SECURE~1\\lib\\DRPREI~1.DLL;foo.dll;"
                L"bar.dll";
        list2 = new_file_list(wcslen(list1) + 1);
        wcsncpy(list2, list1, wcslen(list1) + 1);
        list2 = add_to_file_list(list2,
                                 L"C:\\PROGRA~1\\DETERM~1\\SECURE~1\\lib\\DRPREI~1.DLL",
                                 TRUE, TRUE, TRUE, sep);
        DO_ASSERT_WSTR_EQ(L"C:\\PROGRA~1\\DETERM~1\\SECURE~1\\lib\\DRPREI~1.DLL;home.dll;"
                          L"foo.dll;bar.dll",
                          list2);
        DO_ASSERT(wcslen(list1) == wcslen(list2));
    }

    /* autoinject (FIXME: needs more..) */
    {
        res = get_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, buf,
                                   MAX_PARAM_LEN);
        DO_ASSERT(res == ERROR_SUCCESS);

        res = set_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L,
                                   L"foo.dll;bar.dll;home.dll");
        DO_ASSERT(res == ERROR_SUCCESS);

        /* FIXME: should all full automated tests of all
         *  APPINIT_FLAGS etc. */
        // res = set_autoinjection_ex(TRUE, );
        // DO_ASSERT(res == ERROR_SUCCESS);

        res = set_config_parameter(INJECT_ALL_KEY_L, TRUE, INJECT_ALL_SUBKEY_L, buf);
        DO_ASSERT(res == ERROR_SUCCESS);
    }

    /* config group */
    {
        c = new_config_group(L"foo");
        DO_ASSERT_WSTR_EQ(c->name, L"foo");

        set_config_group_parameter(c, L"bar", L"wise");

        c2 = new_config_group(L"foo2");
        set_config_group_parameter(c2, L"bar2", L"dumb");
        set_config_group_parameter(c2, L"bar3", L"dumber");
        add_config_group(c, c2);

        c3 = new_config_group(L"foo4");
        set_config_group_parameter(c3, L"bar4", L"eloquent");
        add_config_group(c, c3);

        c4 = new_config_group(L"foo5");
        set_config_group_parameter(c4, L"bar5", L"dull");
        add_config_group(c3, c4);
        c->should_clear = TRUE;

        DO_DEBUG(DL_VERB, dump_config_group("", "  ", c, FALSE););

        res = write_config_group(c);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(c->name, L"foo");
    }

    /* copy config group */
    {
        ConfigGroup *cc, *copy = copy_config_group(c, TRUE);
        DO_ASSERT_WSTR_EQ(copy->name, L"foo");

        cc = get_child(L"foo4", copy);
        DO_ASSERT(NULL != cc);
        DO_ASSERT_WSTR_EQ(L"eloquent", get_config_group_parameter(cc, L"bar4"));

        cc = get_child(L"foo5", cc);
        DO_ASSERT(NULL != cc);
        DO_ASSERT_WSTR_EQ(L"dull", get_config_group_parameter(cc, L"bar5"));

        free_config_group(copy);

        copy = copy_config_group(c, FALSE);
        DO_ASSERT_WSTR_EQ(copy->name, L"foo");
        DO_ASSERT_WSTR_EQ(L"wise", get_config_group_parameter(copy, L"bar"));

        cc = get_child(L"foo4", copy);
        DO_ASSERT(NULL == cc);

        free_config_group(copy);
    }

    /* remove child */
    {
        DO_ASSERT(c2 == get_child(L"foo2", c));
        remove_child(L"foo2", c);
        DO_ASSERT(NULL == get_child(L"foo2", c));

        DO_ASSERT(c3 == get_child(L"foo4", c));
        DO_ASSERT(c4 == get_child(L"foo5", c3));

        DO_ASSERT(c3 == get_child(L"Foo4", c));
        DO_ASSERT(c3 == get_child(L"FOO4", c));

        /* make sure we don't die */
        remove_child(L"foo2", c);
    }

    /* remove parameter */
    {
        set_config_group_parameter(c, L"testremove", L"sucks");
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"testremove"), L"sucks");
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"bar"), L"wise");
        remove_config_group_parameter(c, L"testremove");
        DO_ASSERT(NULL == get_config_group_parameter(c, L"testremove"));
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"bar"), L"wise");

        free_config_group(c);
    }

    /* read config */
    {
        res = read_config_group(&c, L"foo", TRUE);
        DO_ASSERT_HANDLE(res == ERROR_SUCCESS, printf("res=%d\n", res); return -1;);
        DO_ASSERT_WSTR_EQ(c->name, L"foo");
        DO_ASSERT(NULL != get_child(L"foo2", c));
        DO_ASSERT(NULL != get_child(L"foo4", c));
        DO_ASSERT(NULL != get_child(L"foo5", get_child(L"foo4", c)));

        free_config_group(c);
    }

    /* read deep config */
    {
        res = read_config_group(&c, L"foo:foo2", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_DEBUG(DL_VERB, printf("c->name: >%S<\n", c->name););
        DO_ASSERT_WSTR_EQ(c->name, L"foo:foo2");
        free_config_group(c);
    }

    /* read deeper config */
    {
        res = read_config_group(&c, L"foo:foo4:foo5", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(c->name, L"foo:foo4:foo5");
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"bar5"), L"dull");
        free_config_group(c);
    }

    /* test write */
    {
        res = read_config_group(&c, L"foo:foo4:foo5", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        set_config_group_parameter(c, L"testwrite", L"rocks");
        DO_ASSERT(get_config_group_parameter(c, L"testwrite") != NULL);
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"testwrite"), L"rocks");
        res = write_config_group(c);
        DO_ASSERT(res == ERROR_SUCCESS);
        free_config_group(c);

        res = read_config_group(&c, L"foo:foo4:foo5", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"testwrite"), L"rocks");
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"bar5"), L"dull");
        free_config_group(c);
    }

    /* test write patches */
    {
        ConfigGroup *hp = new_config_group(L_DYNAMORIO_VAR_HOT_PATCH_MODES);

        c = new_config_group(L"testapp.exe");
        DO_ASSERT(c != NULL);
        set_config_group_parameter(hp, L"TEST.0001", L"0");
        set_config_group_parameter(hp, L"TEST.0002", L"1");
        set_config_group_parameter(hp, L"MS06.019A", L"2");
        add_config_group(c, hp);
        hp = get_child(L_DYNAMORIO_VAR_HOT_PATCH_MODES, c);
        DO_ASSERT(hp != NULL);
        DO_ASSERT(get_config_group_parameter(hp, L"TEST.0001") != NULL);
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(hp, L"TEST.0001"), L"0");
        res = write_config_group(c);
        DO_ASSERT(res == ERROR_SUCCESS);
        free_config_group(c);

        res = read_config_group(&c, L"testapp.exe", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        hp = get_child(L_DYNAMORIO_VAR_HOT_PATCH_MODES, c);
        DO_ASSERT(hp != NULL);
        DO_ASSERT(get_config_group_parameter(hp, L"TEST.0001") != NULL);
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(hp, L"TEST.0001"), L"0");
        DO_DEBUG(DL_VERB, dump_config_group("", "  ", c, FALSE););
        free_config_group(c);
    }

    /* case insensitive read */
    {
        res = read_config_group(&c, L"FOO:FOO4:FOO5", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(get_config_group_parameter(c, L"bar5"), L"dull");

        free_config_group(c);
    }

    /* write back with foo2 removed */
    {
        res = read_config_group(&c, L"foo", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(c->name, L"foo");
        c->should_clear = TRUE;

        DO_ASSERT(NULL != get_child(L"foo2", c));
        remove_child(L"foo2", c);
        DO_ASSERT(NULL == get_child(L"foo2", c));

        res = write_config_group(c);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(c->name, L"foo");

        free_config_group(c);

        /* read back and make sure foo2 is still gone */
        res = read_config_group(&c, L"foo", TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(c->name, L"foo");

        DO_ASSERT(NULL == get_child(L"foo2", c));
        DO_ASSERT(NULL != get_child(L"foo4", c));
        DO_ASSERT(NULL != get_child(L"foo5", get_child(L"foo4", c)));

        free_config_group(c);
    }

    /* absolute config */
    {
        WCHAR buf[MAX_PATH];
        res = set_config_parameter(L"Software\\Microsoft", TRUE, L"foo", L"bar");
        DO_ASSERT(res == ERROR_SUCCESS);

        res = get_config_parameter(L"Software\\Microsoft", TRUE, L"foo", buf, MAX_PATH);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(buf, L"bar");

        res = set_config_parameter(L"Software\\Microsoft", TRUE, L"foo", L"rebar");
        DO_ASSERT(res == ERROR_SUCCESS);

        res = get_config_parameter(L"Software\\Microsoft", TRUE, L"foo", buf, MAX_PATH);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT_WSTR_EQ(buf, L"rebar");

        res = set_config_parameter(L"Software\\Microsoft", TRUE, L"foo", NULL);
        DO_ASSERT(res == ERROR_SUCCESS);

        res = get_config_parameter(L"Software\\Microsoft", TRUE, L"foo", buf, MAX_PATH);
        DO_ASSERT(res != ERROR_SUCCESS);
    }

    /* qnames */
    {
        c = new_config_group(L"apps");
        c2 = new_config_group(L"bar.exe");
        c3 = new_config_group(L"bar.exe-bar");
        c4 = new_config_group(L"foo.exe");

        add_config_group(c, c2);
        add_config_group(c, c3);
        add_config_group(c, c4);

        set_config_group_parameter(c2, L"DYNAMORIO_RUNUNDER", L"48");
        set_config_group_parameter(c3, L"DYNAMORIO_RUNUNDER", L"49");

        cp = get_qualified_config_group(c, L"bar.exe", L"bar.exe /bar");
        DO_ASSERT(cp != NULL);

        cp = get_qualified_config_group(c, L"bar.exe", L"bar.exe /b /a /r");
        DO_ASSERT(cp != NULL);

        cp = get_qualified_config_group(c, L"bar.exe", L"bar.exe /c bar");
        DO_ASSERT(cp != NULL);

        cp = get_qualified_config_group(c, L"bar.exe", L"bar.exe /c Bar");
        DO_ASSERT(cp != NULL);

        cp = get_qualified_config_group(c, L"bar.exe", L"Bar.exe /c bar");
        DO_ASSERT(cp != NULL);

        DO_ASSERT(is_parent_of_qualified_config_group(c2));
        DO_ASSERT(is_parent_of_qualified_config_group(c3));
        DO_ASSERT(!is_parent_of_qualified_config_group(c4));

        set_config_group_parameter(c4, L"DYNAMORIO_RUNUNDER", L"5");
        DO_ASSERT(!is_parent_of_qualified_config_group(c4));
    }

    /* autoinject tests */
    {
        res = unset_autoinjection();
        DO_ASSERT(res == ERROR_SUCCESS);

        res = set_autoinjection();
        DO_ASSERT(res == ERROR_SUCCESS);

        res = unset_autoinjection();
        DO_ASSERT(res == ERROR_SUCCESS);
    }

    printf("All Test Passed\n");

    return 0;
}

#endif
