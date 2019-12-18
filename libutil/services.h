/* **********************************************************
 * Copyright (c) 2005 VMware, Inc.  All rights reserved.
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
 * services.h
 *
 * helper methods dealing with services
 *
 */

#ifndef _DETERMINA_SERVICES_H_
#define _DETERMINA_SERVICES_H_

typedef DWORD ServiceHandle;

#define INVALID_SERVICE_HANDLE 0xffffffff

/* Loads services information from the registry and sets up
 *  ArakshaService handles; must be called before using any service
 *  methods below. */
DWORD
services_init();

/* call when finished using the interface */
DWORD
services_cleanup();

DWORD
reload_service_info();

ServiceHandle
get_service_by_name(const WCHAR *name);

const WCHAR *
get_service_name(ServiceHandle service);

const WCHAR *
get_service_display_name(ServiceHandle service);

/* the callback should return FALSE to abort the walk */
typedef BOOL (*services_callback)(ServiceHandle service, void **param);

/* enumeration callback for all of the services on the system */
DWORD
enumerate_services(services_callback cb, void **param);

/*
 * returns SERVICE_STOPPED, SERVICE_RUNNING, SERVICE_PENDING_*...
 */
int
service_status(ServiceHandle service);

DWORD
set_service_start_type(ServiceHandle service, DWORD dwStartType);

DWORD
get_service_start_type(ServiceHandle service);

DWORD
set_service_restart_type(WCHAR *svcname, BOOL disable);

DWORD
add_dependent_service(ServiceHandle service, ServiceHandle requiredService);

DWORD
reset_dependent_services(ServiceHandle service);

#endif
