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
double2int_trunc(double d)
{
    long i = (long)d;
    double id = (double)i;
    /* when building with /QIfist casting rounds instead of truncating (i#763) */
    if (id > d)
        return i-1;
    else
        return i;
}

/* assumes that d > 0 */
static long
double2int(double d)
{
    long i = (long)d;
    double id = (double)i;
    /* when building with /QIfist casting rounds instead of truncating (i#763) */
    if (id < d && d - id >= 0.5)
        return i+1;
    else if (id > d && id - d >= 0.5)
        return i-1;
    else
        return i;
}

/* we generate from a template to get wide and narrow versions */
#undef IOX_WIDE_CHAR
#include "iox.h"

#define IOX_WIDE_CHAR
#include "iox.h"

#ifdef LINUX
/*****************************************************************************
 * Stand alone sscanf implementation.
 */

typedef enum _specifier_t {
    SPEC_INT,
    SPEC_CHAR,
    SPEC_STRING
} specifer_t;

typedef enum _int_sz_t {
    SZ_SHORT,
    SZ_INT,
    SZ_LONG,
    SZ_LONGLONG,
#if defined(X64) && defined(WINDOWS)
    SZ_PTR = SZ_LONGLONG
#else
    SZ_PTR = SZ_LONG
#endif
} int_sz_t;

/* The isspace() from ctype.h is actually a macro that calls __ctype_b_loc(),
 * which tries to look something up in the library TLS.  This doesn't work
 * without the private loader, so we roll our own isspace().
 */
static bool inline
our_isspace(int c)
{
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
            c == '\v');
}

static const char *
parse_int(const char *sp, uint64 *res_out, int base, int width, bool is_signed)
{
    bool negative = false;
    uint64 res = 0;
    int i;  /* Use an index rather than pointer to compare with width. */

    /* our_sscanf only uses these bases currently. */
    ASSERT(base == 10 || base == 16);

    /* Check for negative sign if signed. */
    if (is_signed) {
        if (*sp == '-') {
            negative = true;
            sp++;
        }
    }

    /* Ignore leading +. */
    if (!negative && *sp == '+')
        sp++;

    /* 0x prefix for hex is optional. */
    if (base == 16 && sp[0] == '0' && sp[1] == 'x')
        sp += 2;

    /* XXX: For efficiency we could do a couple things:
     * - Specialize the loop on base
     * - Use a lookup table
     */
    for (i = 0; width == 0 || i < width; i++) {
        uint d = sp[i];
        if (d >= '0' && d <= '9') {
            d -= '0';
        } else if (base == 16 && d >= 'a' && d <= 'f') {
            d = d - 'a' + 10;
        } else if (base == 16 && d >= 'A' && d <= 'F') {
            d = d - 'A' + 10;
        } else {
            break;  /* Non-digit character.  Could be \0. */
        }
        /* FIXME: Check for overflow. */
        /* XXX: int64 multiply is inefficient on 32-bit. */
        res = res * base + d;
    }

    /* No digits found, return failure. */
    if (i == 0)
        return NULL;

    if (negative)
        res = -(int64)res;

    *res_out = res;
    return sp + i;
}

/* Stand alone implementation of sscanf.  We used to call libc's vsscanf while
 * trying to isolate errno (i#238), but these days sscanf calls malloc (i#762).
 * Therefore, we roll our own.
 */
int
our_vsscanf(const char *str, const char *fmt, va_list ap)
{
    int num_parsed = 0;
    const char *fp = fmt;
    const char *sp = str;
    int c;

    while (*fp != '\0' && *sp != '\0') {
        specifer_t spec = SPEC_INT;
        int_sz_t int_size = SZ_INT;
        uint base = 10;
        bool is_signed = false;
        bool is_ignored = false;
        uint width = 0;

        /* Handle literal characters and spaces up front. */
        c = *fp++;
        if (our_isspace(c)) {
            /* Space means consume any number of spaces. */
            while (our_isspace(*sp)) {
                sp++;
            }
            continue;
        } else if (c != '%') {
            /* Literal, so check mismatch. */
            if (c != *sp)
                return num_parsed;
            sp++;
            continue;
        }

        /* Parse the format specifier. */
        ASSERT(c == '%');
        while (true) {
            c = *fp++;
            switch (c) {
            /* Modifiers, should all continue the loop. */
            case 'l':
                ASSERT(int_size != SZ_LONGLONG && "too many longs");
                if (int_size == SZ_LONG)
                    int_size = SZ_LONGLONG;
                else
                    int_size = SZ_LONG;
                continue;
            case 'h':
                int_size = SZ_SHORT;
                continue;
            case '*':
                is_ignored = true;
                continue;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                /* We honor the specified width for strings to prevent buffer
                 * overruns, but we don't honor it for integers.  Honoring the
                 * width for integers would require our own integer parser.
                 */
                width = width * 10 + c - '0';
                continue;

            /* Specifiers, should all break the loop. */
            case 'u':
                spec = SPEC_INT;
                is_signed = false;
                goto spec_done;
            case 'd':
                spec = SPEC_INT;
                is_signed = true;
                goto spec_done;
            case 'x':
                spec = SPEC_INT;
                is_signed = false;
                base = 16;
                goto spec_done;
            case 'p':
                int_size = SZ_PTR;
                spec = SPEC_INT;
                is_signed = false;
                base = 16;
                goto spec_done;
            case 'c':
                spec = SPEC_CHAR;
                goto spec_done;
            case 's':
                spec = SPEC_STRING;
                goto spec_done;
            default:
                ASSERT(false && "unknown specifier");
                return num_parsed;
            }
        }
spec_done:

        /* Parse the string. */
        switch (spec) {
        case SPEC_CHAR:
            if (!is_ignored) {
                *va_arg(ap, char*) = *sp;
            }
            sp++;
            break;
        case SPEC_STRING:
            if (is_ignored) {
                while (*sp != '\0' && !our_isspace(*sp)) {
                    sp++;
                }
            } else {
                char *str_out = va_arg(ap, char*);
                if (width > 0) {
                    int i = 0;
                    while (i < width && *sp != '\0' && !our_isspace(*sp)) {
                        *str_out++ = *sp++;
                        i++;
                    }
                    /* Spec says only null terminate if we hit width. */
                    if (i < width)
                        *str_out = '\0';
                } else {
                    while (*sp != '\0' && !our_isspace(*sp)) {
                        *str_out++ = *sp++;
                    }
                    *str_out = '\0';
                }
            }
            break;
        case SPEC_INT: {
            uint64 res;
            sp = parse_int(sp, &res, base, width, is_signed);
            if (sp == NULL)
                return num_parsed;

            if (!is_ignored) {
                if (int_size == SZ_SHORT)
                    *va_arg(ap, short *) = (short)res;
                else if (int_size == SZ_INT)
                    *va_arg(ap, int *) = (int)res;
                else if (int_size == SZ_LONG)
                    *va_arg(ap, long *) = (long)res;
                else if (int_size == SZ_LONGLONG)
                    *va_arg(ap, long long *) = (long long)res;
                else
                    ASSERT_NOT_REACHED();
            }
            break;
        }
        default:
            /* Format parsing code above should return an error earlier. */
            ASSERT_NOT_REACHED();
        }

        if (!is_ignored)
            num_parsed++;
    }
    return num_parsed;
}

int
our_sscanf(const char *str, const char *fmt, ...)
{
    /* No need to save errno, we don't call libc anymore. */
    int res;
    va_list ap;
    va_start(ap, fmt);
    res = our_vsscanf(str, fmt, ap);
    va_end(ap);
    return res;
}

#endif /* LINUX */

#ifdef IO_UNIT_TEST
# ifdef LINUX
#  include <errno.h>

/* Copied from core/linux/os.c and modified so that they work when run
 * cross-arch.  We need %ll to parse 64-bit ints on 32-bit and drop the %l to
 * parse 32-bit ints on x64.
 */
#  define UI64 UINT64_FORMAT_CODE
#  define HEX64 INT64_FORMAT"x"
#  define MAPS_LINE_FORMAT4 "%08x-%08x %s %08x %*s %"UI64" %4096s"
#  define MAPS_LINE_FORMAT8 "%016"HEX64"-%016"HEX64" %s %016"HEX64" %*s %"UI64" %4096s"

static void
test_sscanf_maps_x86(void)
{
    char line_copy[1024];
    uint start, end;
    uint offset;
    uint64 inode;
    char perm[16];
    char comment[4096];
    int len;
    const char *maps_line =
        "f75c3000-f75c4000 rw-p 00155000 fc:00 1840387"
        "                            /lib32/libc-2.11.1.so";

    strcpy(line_copy, maps_line);
    len = our_sscanf(line_copy, MAPS_LINE_FORMAT4,
                     &start, &end, perm, &offset, &inode, comment);
    EXPECT(len, 6);
    /* Do int64 comparisons directly.  EXPECT casts to ptr_uint_t. */
    EXPECT(start, 0xf75c3000UL);
    EXPECT(end, 0xf75c4000UL);
    EXPECT(offset, 0x00155000UL);
    EXPECT((inode == 1840387ULL), 1);
    EXPECT(strcmp(perm, "rw-p"), 0);
    EXPECT(strcmp(comment, "/lib32/libc-2.11.1.so"), 0);
    /* sscanf should not modify it's input. */
    EXPECT(strcmp(line_copy, maps_line), 0);
}

static void
test_sscanf_maps_x64(void)
{
    char line_copy[1024];
    uint64 start, end;
    uint64 offset;
    uint64 inode;
    char perm[16];
    char comment[4096];
    int len;
    const char *maps_line =
        "7f94a6757000-7f94a6758000 rw-p 0017d000 fc:00 "
        "1839331                     /lib/libc-2.11.1.so";

    strcpy(line_copy, maps_line);
    len = our_sscanf(line_copy, MAPS_LINE_FORMAT8,
                     &start, &end, perm, &offset, &inode, comment);
    EXPECT(len, 6);
    /* Do int64 comparisons directly.  EXPECT casts to ptr_uint_t. */
    EXPECT((start == 0x7f94a6757000ULL), 1);
    EXPECT((end == 0x7f94a6758000ULL), 1);
    EXPECT((offset == 0x00017d000ULL), 1);
    EXPECT((inode == 1839331ULL), 1);
    EXPECT(strcmp(perm, "rw-p"), 0);
    EXPECT(strcmp(comment, "/lib/libc-2.11.1.so"), 0);
    /* sscanf should not modify it's input. */
    EXPECT(strcmp(line_copy, maps_line), 0);
}

static void
test_sscanf_all_specs(void)
{
    int res;
    char ch;
    char str[16];
    int signed_int;
    int signed_int_2;
    uint unsigned_int;
    uint hex_num;
    unsigned long long ull_num;

    /* ULLONG_MAX is a corner case. */
    res = our_sscanf("c str -123 +456 0x789 0xffffffffffffffff",
                     "%c %s %d %u %x %llx", &ch, str, &signed_int,
                     &unsigned_int, &hex_num, &ull_num);
    EXPECT(res, 6);
    EXPECT(ch, 'c');
    EXPECT(strcmp(str, "str"), 0);
    EXPECT(signed_int, -123);
    EXPECT(unsigned_int, 456);
    EXPECT(hex_num, 0x789);
    EXPECT((ull_num == ULLONG_MAX), true);

    /* A variety of ways to say negative one. */
    res = our_sscanf("-1-1", "%d%d", &signed_int, &signed_int_2);
    EXPECT(res, 2);
    EXPECT(signed_int, -1);
    EXPECT(signed_int_2, -1);

    /* Test ignores. */
    res = our_sscanf("c str -123 +456 0x789 0xffffffffffffffff 1",
                     "%*c %*s %*d %*u %*x %*llx %d", &signed_int);
    EXPECT(res, 1);
    EXPECT(signed_int, 1);

    /* Test width specifications on strings. */
    memset(str, '*', sizeof(str));  /* Fill string with garbage. */
    res = our_sscanf("abcdefghijklmnopqrstuvwxyz",
                     "%13s", str);
    EXPECT(res, 1);
    /* our_sscanf should read 13 chars without null termination. */
    EXPECT(memcmp(str, "abcdefghijklm", 13), 0);
    EXPECT(str[13], '*');  /* Asterisk should still be there. */

    /* Test width specifications for integers. */
    res = our_sscanf("123456 0x9abc", "%03d%03d %03xc",
                     &signed_int, &signed_int_2, &unsigned_int);
    EXPECT(res, 3);
    EXPECT(signed_int, 123);
    EXPECT(signed_int_2, 456);
    EXPECT(unsigned_int, 0x9ab);

    /* FIXME: When parse_int has range checking, we should add tests for parsing
     * integers that overflow their requested integer sizes.
     */
}
# endif /* LINUX */

int
main(void)
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

    /* test float */
    res = our_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%3.1f", 42.9f);
    EXPECT(res == strlen("42.9"), true);
    EXPECT(strcmp(buf, "42.9"), 0);
    /* XXX: add more */

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

#ifdef LINUX
    /* sscanf tests */
    test_sscanf_maps_x86();
    test_sscanf_maps_x64();
    test_sscanf_all_specs();
#endif /* LINUX */

    /* XXX: add more tests */

    print_file(STDERR, "all done\n");
    return 0;
}
#endif /* IO_UNIT_TEST */
