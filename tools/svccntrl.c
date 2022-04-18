/* **********************************************************
 * Copyright (c) 2003-2006 VMware, Inc.  All rights reserved.
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
 * svccntrl.c
 *
 * command-line tool for manipulating services
 *
 * (c) determina, inc. all rights reserved
 */

#include "configure.h"
#include "mfapi.h"
#include "services.h"

#include <stdio.h>

WCHAR *longbuff;
ServiceHandle foundsvc = INVALID_SERVICE_HANDLE;

BOOL
show_svcs_cb(ServiceHandle service, void **param)
{
    static const char *automatic = "auto";
    static const char *manual = "manual";
    static const char *disabled = "disabled";
    static const char *unknown = "<unknown>";
    const char *typename;
    DWORD type = get_service_start_type(service);
    typename = (type == SERVICE_AUTO_START
                    ? automatic
                    : (type == SERVICE_DEMAND_START
                           ? manual
                           : (type == SERVICE_DISABLED ? disabled : unknown)));
    printf("%S %s\n", get_service_name(service), typename);
    return TRUE;
}

BOOL
finddispname_cb(ServiceHandle svc, void **param)
{
    if (!wcsicmp(longbuff, get_service_display_name(svc))) {
        foundsvc = svc;
        return FALSE;
    }
    return TRUE;
}

void
usage()
{
    fprintf(stderr,
            "Usage:\nsvcctrl svcname [ -auto | -manual | -restart | -disabled ] [-help] "
            "[-show] [-dep svc2] [-depreset] [-v]\n");
    exit(1);
}

void
help()
{

    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -auto\\tt\tset the service to run automatically\n");
    fprintf(stderr, " -manual\t\tset the service to manual control\n");
    fprintf(stderr, " -restart\t\tset the service to auto-restart\n");
    fprintf(stderr, " -disabled\t\tdisable the service\n");
    fprintf(stderr, " -show\t\t\tshow all installed services and start types\n");
    fprintf(stderr, " -dep svc2\t\tmake the service dependent on svc2\n");
    fprintf(stderr, " -depreset\t\treset service dependencies\n");
    fprintf(stderr, " -v\t\t\tdisplay version information\n\n");
    exit(1);
}

ServiceHandle
get_svc(char *buf)
{
    ServiceHandle svc;
    WCHAR w_buf[MAX_PATH];

    _snwprintf(w_buf, MAX_PATH, L"%S", buf);
    svc = get_service_by_name(w_buf);
    if (svc == INVALID_SERVICE_HANDLE) {
        longbuff = w_buf;
        enumerate_services(&finddispname_cb, NULL);
        svc = foundsvc;
    }
    return svc;
}

int
main(int argc, char **argv)
{
    int res = 0, sauto = 0, sman = 0, sdis = 0, show = 0, depreset = 0, srestart = 0;
    char *dep = NULL;

    char *svcname = NULL;
    int argidx = 1;
    ServiceHandle svc;
    WCHAR w_buf[MAX_PATH];

    svcname = argv[argidx++];

    while (argidx < argc) {

        if (!strcmp(argv[argidx], "-help")) {
            help();
        } else if (!strcmp(argv[argidx], "-auto")) {
            sauto = 1;
        } else if (!strcmp(argv[argidx], "-manual")) {
            sman = 1;
        } else if (!strcmp(argv[argidx], "-restart")) {
            srestart = 1;
        } else if (!strcmp(argv[argidx], "-show")) {
            show = 1;
        } else if (!strcmp(argv[argidx], "-depreset")) {
            depreset = 1;
        } else if (!strcmp(argv[argidx], "-dep")) {
            if (argidx + 1 >= argc)
                usage();
            dep = argv[++argidx];
        } else if (!strcmp(argv[argidx], "-disabled")) {
            sdis = 1;
        } else if (!strcmp(argv[argidx], "-v")) {
#ifdef BUILD_NUMBER
            printf("svccntrl.exe build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
            printf("svccntrl.exe custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage();
        }
        argidx++;
    }

    if (argc < 3)
        usage();

    if (sauto + sman + sdis + show + srestart != 1 && dep == NULL && !depreset) {
        fprintf(stderr, "Bad combination of options.\n");
        usage();
    }

    services_init();

    if (show) {
        enumerate_services(&show_svcs_cb, NULL);
    } else {

        _snwprintf(w_buf, MAX_PATH, L"%S", svcname);

        if (srestart) {
            res = set_service_restart_type(w_buf, FALSE);
            if (res != ERROR_SUCCESS) {
                fprintf(stderr, "Error %d updating the configuration\n", res);
            }
        } else if (dep) {
            ServiceHandle svc2;
            svc = get_svc(svcname);
            svc2 = get_svc(dep);
            if (svc == INVALID_SERVICE_HANDLE || svc2 == INVALID_SERVICE_HANDLE) {
                fprintf(stderr, "Invalid services: %s, %s\n", svcname, dep);
            } else {
                res = add_dependent_service(svc, svc2);
                if (res != ERROR_SUCCESS)
                    fprintf(stderr, "Error %d setting dependencies\n", res);
            }
        } else if (depreset) {
            svc = get_svc(svcname);
            if (svc == INVALID_SERVICE_HANDLE) {
                fprintf(stderr, "Invalid service: %s\n", svcname);
            } else {
                res = reset_dependent_services(svc);
                if (res != ERROR_SUCCESS)
                    fprintf(stderr, "Error %d resetting dependencies\n", res);
            }
        } else {
            svc = get_svc(svcname);

            if (svc == INVALID_SERVICE_HANDLE) {
                fprintf(stderr, "Invalid service: %s\n", svcname);
            } else {
                res = set_service_start_type(svc,
                                             sauto ? SERVICE_AUTO_START
                                                   : sman ? SERVICE_DEMAND_START
                                                          : sdis ? SERVICE_DISABLED
                                                                 : SERVICE_NO_CHANGE);
                if (res != ERROR_SUCCESS)
                    fprintf(stderr, "There was an error setting the configuration\n");
            }
        }
    }

    services_cleanup();

    return 0;
}
