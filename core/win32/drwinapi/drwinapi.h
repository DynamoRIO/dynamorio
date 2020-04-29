/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.   All rights reserved.
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

/* Routines exposed to the loader for DynamoRIO Windows API
 * redirection routines to avoid needing to use private copies or share
 * application copies of key system libraries.
 */

#ifndef _DRWINAPI_H_
#define _DRWINAPI_H_ 1

#include "../../module_shared.h"

void
drwinapi_init(void);

void
drwinapi_exit(void);

void
drwinapi_onload(privmod_t *mod);

app_pc
drwinapi_redirect_imports(privmod_t *impmod, const char *name, privmod_t *importer);

bool
drwinapi_redirect_getprocaddr(app_pc modbase, const char *name, app_pc *res_out OUT);

void
ntdll_redir_fls_init(PEB *app_peb, PEB *private_peb);

void
ntdll_redir_fls_exit(PEB *private_peb);

/* i#3633: Fix Windows 1903 issue. FLS is not held inside of PEB but in private
 * variables inside of ntdll.dll. This routine will perform unlinking to prevent
 * crash.
 */
void
ntdll_redir_fls_thread_exit(PPVOID fls_data);
#endif /* _DRWINAPI_H_ */
