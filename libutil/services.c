/* **********************************************************
 * Copyright (c) 2019-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2005 VMware, Inc.  All rights reserved.
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
 * services.c
 *
 * helper methods dealing with services
 *
 */

#include "share.h"
#include "services.h"

#ifndef UNIT_TEST

UINT NUM_SERVICES = 0;

typedef struct ServiceInfo_ {
    ServiceHandle svch;
    DWORD start_type;
    WCHAR *binary_pathname;
    WCHAR *service_name;
    WCHAR *service_display_name;
} ServiceInfo;

ServiceInfo *services = NULL;

/* the service manager database handle */
SC_HANDLE scmdb;

BOOL services_initialized = FALSE;

#    define SVC_BUFSZ 4096

DWORD
get_service_strings()
{
    DWORD needed, nsvcs, next = 0, i;
    BOOL bRes;
    ENUM_SERVICE_STATUS *infobuf;
    SC_HANDLE sch;
    QUERY_SERVICE_CONFIG *scfg;
    BYTE buffer[SVC_BUFSZ];

    /* now read in the service information */
    scfg = (QUERY_SERVICE_CONFIG *)buffer;

    EnumServicesStatus(scmdb, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &needed, &nsvcs,
                       &next);
    infobuf = (ENUM_SERVICE_STATUS *)malloc(needed);

    bRes = EnumServicesStatus(scmdb, SERVICE_WIN32, SERVICE_STATE_ALL, infobuf, needed,
                              &needed, &nsvcs, &next);

    if (!bRes)
        return GetLastError();

    NUM_SERVICES = nsvcs;
    services = (ServiceInfo *)malloc(nsvcs * sizeof(ServiceInfo));
    memset(services, 0, nsvcs * sizeof(ServiceInfo));

    for (i = 0; i < nsvcs; i++) {
        services[i].svch = i;
        services[i].service_name = wcsdup(infobuf[i].lpServiceName);

        sch = OpenService(scmdb, services[i].service_name, SERVICE_QUERY_CONFIG);
        if (QueryServiceConfig(sch, scfg, SVC_BUFSZ, &needed)) {
            services[i].service_display_name = wcsdup(scfg->lpDisplayName);

            services[i].binary_pathname = wcsdup(scfg->lpBinaryPathName);

            services[i].start_type = scfg->dwStartType;
        }

        CloseServiceHandle(sch);
    }

    free(infobuf);

    return ERROR_SUCCESS;
}

void
free_service_strings()
{
    UINT i;

    if (services == NULL)
        return;

    for (i = 0; i < NUM_SERVICES; i++) {
        free(services[i].service_name);
        free(services[i].service_display_name);
        free(services[i].binary_pathname);
    }

    free(services);

    services = NULL;
}

DWORD
reload_service_info()
{
    free_service_strings();
    return get_service_strings();
}

/*
 *
 * interface functions
 *
 */

DWORD
enumerate_services(services_callback cb, void **param)
{
    ServiceHandle svc;

    for (svc = 0; svc < NUM_SERVICES; svc++)
        if (!(*cb)(svc, param))
            break;

    return ERROR_SUCCESS;
}

ServiceHandle
get_service_by_name(const WCHAR *name)
{
    ServiceHandle svc;
    for (svc = 0; svc < NUM_SERVICES; ++svc)
        if (!wcsicmp(services[svc].service_name, name))
            return svc;

    return INVALID_SERVICE_HANDLE;
}

const WCHAR *
get_service_name(ServiceHandle service)
{
    if (0 <= service && service < NUM_SERVICES)
        return services[service].service_name;
    else
        return NULL;
}

const WCHAR *
get_service_display_name(ServiceHandle service)
{
    if (0 <= service && service < NUM_SERVICES)
        return services[service].service_display_name;
    else
        return NULL;
}

DWORD
services_init()
{
    DWORD res;
    if (services_initialized)
        return ERROR_ALREADY_INITIALIZED;

    /* FIXME: probably only need read access, except
     *  for change config...but that should just open its own
     *  handle. */
    scmdb = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scmdb == NULL) {
        /* probably because of non-admin privileges. open read only. */
        scmdb = OpenSCManager(NULL, NULL, GENERIC_READ);
        if (scmdb == NULL) {
            res = GetLastError();
            return res == ERROR_SUCCESS ? -1 : res;
        }
    }

    res = get_service_strings();
    if (res != ERROR_SUCCESS)
        return res;
    services_initialized = TRUE;

    return ERROR_SUCCESS;
}

DWORD
services_cleanup()
{
    if (!services_initialized)
        return ERROR_SUCCESS;

    services_initialized = FALSE;

    free_service_strings();

    CloseServiceHandle(scmdb);
    NUM_SERVICES = 0;

    return ERROR_SUCCESS;
}

int
service_status(ServiceHandle service)
{
    SERVICE_STATUS ss;

    SC_HANDLE hsvc =
        OpenService(scmdb, services[service].service_name, SERVICE_QUERY_STATUS);

    QueryServiceStatus(hsvc, &ss);
    CloseServiceHandle(hsvc);

    return ss.dwCurrentState;
}

DWORD
add_dependent_service(ServiceHandle service, ServiceHandle requiredService)
{
    BOOL cscres;
    WCHAR dep[MAX_PATH];
    WCHAR *nextdep = NULL;
    QUERY_SERVICE_CONFIG *scfg;
    BYTE buffer[SVC_BUFSZ];
    SIZE_T size;
    SC_HANDLE hsvc = OpenService(scmdb, services[service].service_name,
                                 SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);

    if (hsvc == NULL)
        return GetLastError();

    scfg = (QUERY_SERVICE_CONFIG *)buffer;
    if (!QueryServiceConfig(hsvc, scfg, SVC_BUFSZ, &size)) {
        return GetLastError();
    }

    nextdep = scfg->lpDependencies;
    size = 0;

    while (nextdep != NULL) {
        if (nextdep[0] == L'\0')
            break;
        size += wcslen(nextdep) + 1;
        nextdep = scfg->lpDependencies + size;
    }

    if (size > MAX_PATH - 2)
        return ERROR_INSUFFICIENT_BUFFER;

    if (size != 0) {
        memcpy(dep, scfg->lpDependencies, size * sizeof(WCHAR));
    }

    wcsncpy(&dep[size], services[requiredService].service_name, MAX_PATH - size);

    size += wcslen(services[requiredService].service_name) + 1;

    if (size > MAX_PATH - 2) {
        CloseServiceHandle(hsvc);
        return ERROR_INSUFFICIENT_BUFFER;
    }

    dep[size + 1] = L'\0';

    cscres =
        ChangeServiceConfig(hsvc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                            NULL, NULL, NULL, dep, NULL, NULL, NULL);

    CloseServiceHandle(hsvc);

    if (!cscres)
        return GetLastError();

    return reload_service_info();
}

DWORD
reset_dependent_services(ServiceHandle service)
{
    BOOL cscres;
    SC_HANDLE hsvc =
        OpenService(scmdb, services[service].service_name, SERVICE_CHANGE_CONFIG);

    if (hsvc == NULL)
        return GetLastError();

    cscres =
        ChangeServiceConfig(hsvc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                            NULL, NULL, NULL, L"", NULL, NULL, NULL);

    CloseServiceHandle(hsvc);

    if (!cscres)
        return GetLastError();

    return reload_service_info();
}

DWORD
set_service_start_type(ServiceHandle service, DWORD dwStartType)
{
    BOOL cscres;
    SC_HANDLE hsvc =
        OpenService(scmdb, services[service].service_name, SERVICE_CHANGE_CONFIG);

    if (hsvc == NULL)
        return GetLastError();

    cscres = ChangeServiceConfig(hsvc, SERVICE_NO_CHANGE, dwStartType, SERVICE_NO_CHANGE,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    CloseServiceHandle(hsvc);

    if (!cscres)
        return GetLastError();

    return reload_service_info();
}

DWORD
get_service_start_type(ServiceHandle service)
{
    return services[service].start_type;
}

/* delay to wait before restarting the service after a failure. */
#    define SERVICE_RESTART_DELAY_MS 60000

/* QueryServiceConfig2 and ChangeServiceConfig2 are not supported
 *  on NT, so we load these dynamically.
 */
typedef BOOL(WINAPI *QueryServiceConfig2Func)(SC_HANDLE hService, DWORD dwInfoLevel,
                                              LPBYTE lpBuffer, DWORD cbBufSize,
                                              LPDWORD pcbBytesNeeded);

typedef BOOL(WINAPI *ChangeServiceConfig2Func)(SC_HANDLE hService, DWORD dwInfoLevel,
                                               LPVOID lpInfo);

/* sets the windows service config to autostart. we do this here
 *  because InstallShield doesn't provide a nice interface for it, it
 *  would require adding a custom DLL to the nodemanager
 *  install. plus, we get the additional benefit that we always make
 *  sure this is set, even if it gets turned off somehow. in the
 *  future we can have a controller-configurable parameter that
 *  controls whether we always enforce auto-restart of the service.
 *
 * if fatal_error_encountered, turn this shit off so we don't spin
 *  wheels and write one eventlog message every minute. */
DWORD
set_service_restart_type(WCHAR *svcname, BOOL disable)
{
    SC_HANDLE local_scmdb = NULL, service = NULL;
    SC_ACTION restart_action = { SC_ACTION_RESTART, SERVICE_RESTART_DELAY_MS };
    SERVICE_FAILURE_ACTIONS failure_actions = {
        0,   /* dwResetPeriod: we only have one failure action, so
                just keep at zero*/
        L"", /* we don't force server reboot on failure */
        L"", /* nor do we execute any commands */
        1,   /* we have one action in our array */
        NULL /* which will be set to &restart_action below */
    };
    /* if this buffer size is exceeded then the QueryServiceConfig
     *  will fail with ERROR_INSUFFICIENT_BUFFER, in which case we can
     *  be sure that someone mucked our settings. */
    BYTE current_actions_buffer[sizeof(SERVICE_FAILURE_ACTIONS) +
                                5 * sizeof(SC_ACTION)] = { 0 };
    SERVICE_FAILURE_ACTIONS *current_actions =
        (SERVICE_FAILURE_ACTIONS *)&current_actions_buffer;
    DWORD res = ERROR_SUCCESS, needed;
    QueryServiceConfig2Func qscf;
    ChangeServiceConfig2Func cscf;
    HMODULE advapi = NULL;

    /* finish setting up the failure_actions data structure */
    failure_actions.lpsaActions = &restart_action;

    /* first: check if we've got the necessary API available. if not,
     *  this means we're on NT -- abort with ERROR_NOT_SUPPORTED (NT
     *  does not support service failure actions). */
    advapi = GetModuleHandle(L"advapi32.dll");
    if (advapi == NULL) {
        return ERROR_NOT_SUPPORTED;
    }

    qscf = (QueryServiceConfig2Func)GetProcAddress(advapi, "QueryServiceConfig2W");
    cscf = (ChangeServiceConfig2Func)GetProcAddress(advapi, "ChangeServiceConfig2W");

    if (qscf == NULL || cscf == NULL) {
        return ERROR_NOT_SUPPORTED;
    }

    /* okay, we've got the API, now do the work */

    if (disable)
        restart_action.Type = SC_ACTION_NONE;

    local_scmdb = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (local_scmdb == NULL) {
        res = GetLastError();
        goto autorestart_out;
    }

    service = OpenService(local_scmdb, svcname, SERVICE_ALL_ACCESS);

    if (service == NULL) {
        res = GetLastError();
        goto autorestart_out;
    }

    if ((*qscf)(service, SERVICE_CONFIG_FAILURE_ACTIONS, (LPBYTE)current_actions,
                sizeof(current_actions_buffer), &needed) ||
        (res = GetLastError()) == ERROR_INSUFFICIENT_BUFFER) {
        /* check if it's already set up */
        SC_ACTION *current_action = current_actions->lpsaActions;
        if (!disable && current_actions->cActions == 1 &&
            current_action->Type == SC_ACTION_RESTART)
            goto autorestart_out;

        /* if not, change the configuration */
        if (!(*cscf)(service, SERVICE_CONFIG_FAILURE_ACTIONS, (LPVOID)&failure_actions)) {
            res = GetLastError();
            goto autorestart_out;
        }

    } else {
        res = GetLastError();
        goto autorestart_out;
    }

autorestart_out:
    if (service != NULL)
        CloseServiceHandle(service);

    if (local_scmdb != NULL)
        CloseServiceHandle(local_scmdb);

    return res;
}

#else // ifdef UNIT_TEST

BOOL found;
ServiceHandle target;
int num;

BOOL
svc_cb(ServiceHandle service, void **param)
{
    if (service == target)
        found = TRUE;
    num++;
    DO_ASSERT(wcslen(get_service_name(service)) > 0);
    return TRUE;
}

int
main()
{
    DWORD res, type;
    WCHAR *s1, *sn1;
    const WCHAR *c1;
    ServiceHandle sh1, sh2 = 0;

    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    res = services_init();
    DO_ASSERT(res == ERROR_SUCCESS);

    sn1 = L"Eventlog";
    s1 = L"Event Log";
    sh1 = get_service_by_name(sn1);
    DO_ASSERT(sh1 != INVALID_SERVICE_HANDLE);

    c1 = get_service_name(sh1);
    DO_ASSERT_WSTR_EQ(sn1, (WCHAR *)c1);

    c1 = get_service_display_name(sh1);
    DO_ASSERT_WSTR_EQ(s1, (WCHAR *)c1);

    found = FALSE;
    target = sh1;
    res = enumerate_services(&svc_cb, NULL);
    DO_ASSERT(res == ERROR_SUCCESS);
    DO_ASSERT(num > 10);
    DO_ASSERT(found);
    printf("%d services found on local machine.\n", num);

    sh1 = get_service_by_name(L"Alerter");
    DO_ASSERT(sh1 != INVALID_SERVICE_HANDLE);
    type = get_service_start_type(sh1);
    DO_ASSERT(res == ERROR_SUCCESS);

    res = set_service_start_type(sh1, SERVICE_DEMAND_START);
    DO_ASSERT(res == ERROR_SUCCESS);

    res = reload_service_info();
    DO_ASSERT(res == ERROR_SUCCESS);
    DO_ASSERT(SERVICE_DEMAND_START == get_service_start_type(sh1));

    res = set_service_start_type(sh1, SERVICE_AUTO_START);
    DO_ASSERT(res == ERROR_SUCCESS);

    DO_ASSERT(SERVICE_AUTO_START == get_service_start_type(sh1));

    res = set_service_start_type(sh1, type);
    DO_ASSERT(type == get_service_start_type(sh1));

#    if 0
    /* FIXME: should have a usable test here */
    sh1 = get_service_by_name(L"TomcatSC");
    DO_ASSERT(sh1 != INVALID_SERVICE_HANDLE);
    sh2 = get_service_by_name(L"MySQL");
    DO_ASSERT(sh2 != INVALID_SERVICE_HANDLE);

    res = add_dependent_service(sh1, sh2);
    DO_ASSERT(res == ERROR_SUCCESS);
#    endif

    res = services_cleanup();
    DO_ASSERT(res == ERROR_SUCCESS);

    printf("All Test Passed\n");

    return 0;
}

#endif
