/* **********************************************************
 * Copyright (c) 2013 Google, Inc.   All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* advapi32 redirection routines.
 * We initially target the union of the imports of C++ apps, msvcrt,
 * and dbghelp.
 */

#ifndef _ADVAPI32_REDIR_H_
#define _ADVAPI32_REDIR_H_ 1

#include "../../globals.h"
#include "../../module_shared.h"

void
advapi32_redir_init(void);

void
advapi32_redir_exit(void);

void
advapi32_redir_onload(privmod_t *mod);

app_pc
advapi32_redir_lookup(const char *name);

/**************************************************
 * support older VS versions
 */

#ifndef __out_data_source
#    define __out_data_source(x) /* nothing */
#endif

#if _MSC_VER <= 1400 /* VS2005- */
/* winreg.h */
typedef LONG LSTATUS;
#endif

LSTATUS
WINAPI
redirect_RegCloseKey(__in HKEY hKey);

LSTATUS
WINAPI
redirect_RegOpenKeyExA(__in HKEY hKey, __in_opt LPCSTR lpSubKey, __in_opt DWORD ulOptions,
                       __in REGSAM samDesired, __out PHKEY phkResult);

LSTATUS
WINAPI
redirect_RegOpenKeyExW(__in HKEY hKey, __in_opt LPCWSTR lpSubKey,
                       __in_opt DWORD ulOptions, __in REGSAM samDesired,
                       __out PHKEY phkResult);

LSTATUS
WINAPI
redirect_RegQueryValueExA(__in HKEY hKey, __in_opt LPCSTR lpValueName,
                          __reserved LPDWORD lpReserved, __out_opt LPDWORD lpType,
                          __out_bcount_part_opt(*lpcbData, *lpcbData)
                              __out_data_source(REGISTRY) LPBYTE lpData,
                          __inout_opt LPDWORD lpcbData);

LSTATUS
WINAPI
redirect_RegQueryValueExW(__in HKEY hKey, __in_opt LPCWSTR lpValueName,
                          __reserved LPDWORD lpReserved, __out_opt LPDWORD lpType,
                          __out_bcount_part_opt(*lpcbData, *lpcbData)
                              __out_data_source(REGISTRY) LPBYTE lpData,
                          __inout_opt LPDWORD lpcbData);

#endif /* _ADVAPI32_REDIR_H_ */
