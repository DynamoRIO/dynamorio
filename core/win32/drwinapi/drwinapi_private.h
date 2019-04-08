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

/* Data and code shared among all DynamoRIO Windows API redirection routines */

#ifndef _DRWINAPI_PRIVATE_H_
#define _DRWINAPI_PRIVATE_H_ 1

/* Redirection of system library routines.
 * This includes some ntdll routines that for transparency reasons we can't
 * point at the real ntdll.
 * We use a hashtable at runtime, but we construct the hashtable at init
 * time from a table of these entries.  We use a separate table and
 * separate hashtable per system library.
 */
typedef struct _redirect_import_t {
    const char *name;
    app_pc func;
} redirect_import_t;

DWORD
ntstatus_to_last_error(NTSTATUS status);

/* Redirection targets shared among multiple modules */

BOOL WINAPI
redirect_ignore_arg0(void);

BOOL WINAPI
redirect_ignore_arg4(void *arg1);

BOOL WINAPI
redirect_ignore_arg8(void *arg1, void *arg2);

BOOL WINAPI
redirect_ignore_arg12(void *arg1, void *arg2, void *arg3);

#endif /* _DRWINAPI_PRIVATE_H_ */
