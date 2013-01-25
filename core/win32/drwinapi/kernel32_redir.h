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

/* kernel32 and kernelbase redirection routines */

#ifndef _KERNEL32_REDIR_H_
#define _KERNEL32_REDIR_H_ 1

#include "../../globals.h"
#include "../../module_shared.h"

void
kernel32_redir_init(void);

void
kernel32_redir_exit(void);

void
kernel32_redir_onload(privmod_t *mod);

app_pc
kernel32_redir_lookup(const char *name);

/* Pointers to routines in the (private copy of the) system library itself */
void (WINAPI *priv_SetLastError)(DWORD val); /* kernel32 or ntdll */

/***************************************************************************
 * Processes and threads
 */

void
kernel32_redir_init_proc(void);

void
kernel32_redir_exit_proc(void);

void
kernel32_redir_onload_proc(privmod_t *mod);

DWORD WINAPI
redirect_FlsAlloc(PFLS_CALLBACK_FUNCTION cb);

/***************************************************************************
 * Libraries
 */

void
kernel32_redir_onload_lib(privmod_t *mod);

HMODULE WINAPI
redirect_GetModuleHandleA(const char *name);

HMODULE WINAPI
redirect_GetModuleHandleW(const wchar_t *name);

FARPROC WINAPI
redirect_GetProcAddress(app_pc modbase, const char *name);

HMODULE WINAPI
redirect_LoadLibraryA(const char *name);

HMODULE WINAPI
redirect_LoadLibraryW(const wchar_t *name);

DWORD WINAPI
redirect_GetModuleFileNameA(HMODULE mod, char *buf, DWORD bufcnt);

DWORD WINAPI
redirect_GetModuleFileNameW(HMODULE mod, wchar_t *buf, DWORD bufcnt);


#endif /* _KERNEL32_REDIR_H_ */
