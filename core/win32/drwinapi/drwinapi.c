/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.   All rights reserved.
 * Copyright (c) 2009-2010 Derek Bruening   All rights reserved.
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

/* Shared redirection code for custom private library loader */

#include "kernel32_redir.h" /* must be included first */
#include "../globals.h"
#include "ntdll_redir.h"

#ifndef WINDOWS
# error Windows-only
#endif

void
drwinapi_init(void)
{
    ntdll_redir_init();
    kernel32_redir_init();
}

void
drwinapi_exit(void)
{
    kernel32_redir_exit();
    ntdll_redir_exit();
}

void
drwinapi_onload(privmod_t *mod)
{
    if (strcasecmp(mod->name, "kernel32.dll") == 0)
        kernel32_redir_onload(mod);
}

app_pc
drwinapi_redirect_imports(privmod_t *impmod, const char *name, privmod_t *importer)
{
    if (strcasecmp(impmod->name, "ntdll.dll") == 0) {
        return ntdll_redir_lookup(name);
    } else if (strcasecmp(impmod->name, "kernel32.dll") == 0 ||
               /* we don't want to redirect kernel32.dll's calls to kernelbase */
               ((importer == NULL ||
                 strcasecmp(importer->name, "kernel32.dll") != 0) &&
                strcasecmp(impmod->name, "kernelbase.dll") == 0)) {
        return kernel32_redir_lookup(name);
    }
    return NULL;
}

BOOL WINAPI
redirect_ignore_arg0(void)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg4(void *arg1)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg8(void *arg1, void *arg2)
{
    return TRUE;
}

BOOL WINAPI
redirect_ignore_arg12(void *arg1, void *arg2, void *arg3)
{
    return TRUE;
}

