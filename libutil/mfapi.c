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

/*
 *
 * implementations of API wrappers
 *
 */

#include "share.h"
#include "config.h"
#include "processes.h"
#include "elm.h"

#ifndef UNIT_TEST

DWORD
disable_protection()
{
    return unset_autoinjection();
}

DWORD
enable_protection()
{
    return set_autoinjection();
}

BOOL
is_protection_enabled()
{
    return is_autoinjection_set();
}

DWORD
enable_protection_ex(BOOL inject, DWORD flags, const WCHAR *blocklist,
                     const WCHAR *allowlist, DWORD *list_error,
                     const WCHAR *custom_preinject_name, WCHAR *current_list,
                     SIZE_T maxchars)
{
    return set_autoinjection_ex(inject, flags, blocklist, allowlist, list_error,
                                custom_preinject_name, current_list, maxchars);
}

DWORD
inject_status(process_id_t pid, DWORD *status, DWORD *build)
{
    DWORD dll_stat, res = ERROR_SUCCESS;

    dll_stat = under_dynamorio_ex(pid, build);

    switch (dll_stat) {
    case DLL_NONE: *status = INJECT_STATUS_NATIVE; break;
    case DLL_UNKNOWN: *status = INJECT_STATUS_UNKNOWN; break;
    case DLL_RELEASE:
    case DLL_DEBUG:
    case DLL_PROFILE:
    case DLL_CUSTOM: *status = INJECT_STATUS_PROTECTED; break;
    case DLL_PATHHAS:
    default:
        *status = INJECT_STATUS_UNKNOWN;
        res = ERROR_INVALID_PARAMETER;
        break;
    }

    return res;
}

DWORD
consistency_detach(DWORD timeout)
{
    DWORD res;
    ConfigGroup *policy;

    if (is_protection_enabled()) {
        res = read_config_group(&policy, L_PRODUCT_NAME, TRUE);
        if (res != ERROR_SUCCESS)
            return res;

        res = detach_all_not_in_config_group(policy, timeout);

        free_config_group(policy);
    } else {
        res = detach_all(timeout);
    }

    return res;
}

BOOL
is_process_pending_restart(process_id_t pid)
{
    DWORD res;
    ConfigGroup *policy;
    BOOL pending_restart;

    res = read_config_group(&policy, L_PRODUCT_NAME, TRUE);
    if (res != ERROR_SUCCESS)
        return FALSE;

    res = check_status_and_pending_restart(policy, pid, &pending_restart, NULL, NULL);
    if (res != ERROR_SUCCESS)
        return FALSE;

    free_config_group(policy);

    return pending_restart;
}

BOOL
is_any_process_pending_restart()
{
    DWORD res;
    ConfigGroup *policy;
    BOOL pending_restart;

    res = read_config_group(&policy, L_PRODUCT_NAME, TRUE);
    if (res != ERROR_SUCCESS)
        return FALSE;

    res = is_anything_pending_restart(policy, &pending_restart);
    if (res != ERROR_SUCCESS)
        return FALSE;

    free_config_group(policy);

    return pending_restart;
}

HANDLE
get_eventlog_monitor_thread_handle()
{
    return get_elm_thread_handle();
}

const WCHAR *
get_installation_path()
{
    return get_dynamorio_home();
}

const WCHAR *
get_product_name()
{
    return L_PRODUCT_NAME;
}

#else // ifdef UNIT_TEST

int
main()
{
    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    /* FIXME: wrapper testing (but may not be necessary...) */

    DO_ASSERT(get_eventlog_monitor_thread_handle() == NULL);
    CHECKED_OPERATION(clear_policy());
    DO_ASSERT(!is_any_process_pending_restart());

    printf("All Test Passed\n");

    return 0;
}

#endif
