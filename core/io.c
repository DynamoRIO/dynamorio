/* **********************************************************
 * Copyright (c) 2011-2012 Google, Inc.  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */

/*
 * io.c - routines for i/o to avoid library dependencies
 */

/* FIXME: failure modes should be more graceful than failing asserts in most places */

/* avoid depending on __isoc99_vsscanf which requires glibc >= 2.7 */
#define _GNU_SOURCE 1
#include <stdio.h>
#undef _GNU_SOURCE

#include "globals.h"
#include <string.h>
#include <stdarg.h> /* for varargs */

#ifdef LINUX
# include <wchar.h>
#endif

#define VA_ARG_CHAR2INT
#define BUF_SIZE 64

#ifdef LINUX
const static double pos_inf = 1.0/0.0;
const static double neg_inf = -1.0/0.0;
#else
/* Windows says "error C2099: initializer is not a constant", or
 * "error C2124: divide or mod by zero", for the above.
 */
# pragma warning(disable : 4723) /* warning C4723: potential divide by 0 */
const static double zerof = 0.0;
# define pos_inf (1.0/zerof)
# define neg_inf (-1.0/zerof)
#endif

/* assumes that d > 0 */
static long
double2int(double d)
{
    long i = (long)d;
    if ((d - (double)i) >= 0.5)
        return i+1;
    else
        return i;
}

/* we generate from a template to get wide and narrow versions */
#undef IOX_WIDE_CHAR
#include "iox.h"

#define IOX_WIDE_CHAR
#include "iox.h"

#ifdef LINUX
/* i#238/PR 499179: avoid touching errno: our __errno_location doesn't
 * affect libc and we're using libc's sscanf.
 *
 * If we do have a private loader (i#157) and isolation of a private
 * libc then there's no need to write our own complete sscanf: if we
 * don't have that though we'll want our own for libc independence
 * (i#46/PR 206369) for earlier injection and to solve errno issues.
 */
int
our_sscanf(const char *str, const char *fmt, ...)
{
    int res, saved_errno;
    va_list ap;
    va_start(ap, fmt);
    saved_errno = get_libc_errno();
    res = vsscanf(str, fmt, ap);
    set_libc_errno(saved_errno);
    va_end(ap);
    return res;
}
#endif

#ifdef IO_UNIT_TEST
int
main()
{
    char buf[512];
    wchar_t wbuf[512];
    int res;
    standalone_init();

    /* test wide char conversion */
    res = our_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%S", L"wide string");
    EXPECT(res == strlen("wide string"), true);
    EXPECT(strcmp(buf, "wide string"), 0);
    res = our_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%ls", L"wide string");
    EXPECT(res == strlen("wide string"), true);
    EXPECT(strcmp(buf, "wide string"), 0);
    res = our_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%.3S", L"wide string");
    EXPECT(res == strlen("wid"), true);
    EXPECT(strcmp(buf, "wid"), 0);
    res = our_snprintf(buf, 4, "%S", L"wide string");
    EXPECT(res == -1, true);
    EXPECT(buf[4], ' '); /* ' ' from prior calls: no NULL written since hit max */
    buf[4] = '\0';
    EXPECT(strcmp(buf, "wide"), 0);

    /* test all-wide */
    res = our_snprintf_wide(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%d%s%3.1f",
                            -42, L"wide string", 42.9f);
    EXPECT(res == wcslen(L"-42wide string42.9"), true);
    EXPECT(wcscmp(wbuf, L"-42wide string42.9"), 0);

    /* test all-wide conversion */
    res = our_snprintf_wide(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%S", "narrow string");
    EXPECT(res == wcslen(L"narrow string"), true);
    EXPECT(wcscmp(wbuf, L"narrow string"), 0);
    res = our_snprintf_wide(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%hs", "narrow string");
    EXPECT(res == wcslen(L"narrow string"), true);
    EXPECT(wcscmp(wbuf, L"narrow string"), 0);
    res = our_snprintf_wide(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%.3S", "narrow string");
    EXPECT(res == wcslen(L"nar"), true);
    EXPECT(wcscmp(wbuf, L"nar"), 0);
    res = our_snprintf_wide(wbuf, 6, L"%S", "narrow string");
    EXPECT(res == -1, true);
    EXPECT(wbuf[6], L' '); /* ' ' from prior calls: no NULL written since hit max */
    wbuf[6] = L'\0';
    EXPECT(wcscmp(wbuf, L"narrow"), 0);

    /* XXX: add more tests */

    print_file(STDERR, "all done\n");
    return 0;
}
#endif /* IO_UNIT_TEST */
