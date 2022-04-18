/* **********************************************************
 * Copyright (c) 2012-2020 Google, Inc.  All rights reserved.
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

#ifndef _SHARELIB_OUR_TCHAR_H_
#define _SHARELIB_OUR_TCHAR_H_

#ifdef WINDOWS
/* Choose based on _UNICODE on Windows. */
#    include <tchar.h>
#else
/* Paths always use char on Linux. */
#    define TCHAR char
#    define _tcsstr strstr
#    define _tcslen strlen
#    define _tcscmp strcmp
#    define _tcsncpy strncpy
/* TODO: Switch to DR's snprintf for consistent behavior, like dr_config.c has done. */
#    define _sntprintf snprintf
#    define _tcstoul strtoul
#    define _TEXT(str) str
#    define _wfopen fopen
#    define _wremove remove
#endif

#ifdef _UNICODE
#    define TSTR_FMT "%S"
#else
#    define TSTR_FMT "%s"
#endif

/* Convenient place for other string compat routines. */
#ifndef WINDOWS
#    define _snprintf snprintf
#endif

#endif /* _SHARELIB_OUR_TCHAR_H_ */
