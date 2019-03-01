/* **********************************************************
 * Copyright (c) 2011-2018 Google, Inc.  All rights reserved.
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
 * iox.h: i/o routines for both Linux and Windows
 */

#include <limits.h> /* for UCHAR_MAX */

#ifdef IOX_WIDE_CHAR
#    define TCHAR wchar_t
#    define _T(s) L##s
#    define TNAME(n) n##_wide
#    define IF_WIDE_ELSE(x, y) x
#else
#    define TCHAR char
#    define _T(s) s
#    define TNAME(n) n
#    define IF_WIDE_ELSE(x, y) y
#endif

const static char TNAME(base_letters)[] = { _T('0'), _T('1'), _T('2'), _T('3'),
                                            _T('4'), _T('5'), _T('6'), _T('7'),
                                            _T('8'), _T('9'), _T('a'), _T('b'),
                                            _T('c'), _T('d'), _T('e'), _T('f') };
const static char TNAME(base_letters_cap)[] = { _T('0'), _T('1'), _T('2'), _T('3'),
                                                _T('4'), _T('5'), _T('6'), _T('7'),
                                                _T('8'), _T('9'), _T('A'), _T('B'),
                                                _T('C'), _T('D'), _T('E'), _T('F') };

/* convert uint64 to a string */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
static TCHAR *
TNAME(uint64_to_str)(uint64 num, int base, TCHAR *buf, int decimal,
                     bool caps)
/* clang-format on */
{
    int cnt;
    TCHAR *p = buf;
    int end = (43 > decimal ? 43 : decimal);
    ASSERT(decimal < BUF_SIZE - 1); /* so don't overflow buf */

    buf[end] = '\0';
    for (cnt = end - 1; cnt >= 0; cnt--) {
        buf[cnt] = caps ? TNAME(base_letters_cap)[(num % base)]
                        : TNAME(base_letters)[(num % base)];
        num /= base;
    }

    while (*p && *p == _T('0') && end - decimal > 0) {
        p++;
        decimal++;
    }

    return p;
}

/* convert ulong to a string */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
static TCHAR *
TNAME(ulong_to_str)(ulong num, int base, TCHAR *buf, int decimal, bool caps)
/* clang-format on */
{
    int cnt;
    TCHAR *p = buf;
    int end = (22 > decimal ? 22 : decimal); /* room for 64 bits octal */
    ASSERT(decimal < BUF_SIZE - 1);          /* so don't overflow buf */

    buf[end] = '\0';
    for (cnt = end - 1; cnt >= 0; cnt--) {
        buf[cnt] = caps ? TNAME(base_letters_cap)[(num % base)]
                        : TNAME(base_letters)[(num % base)];
        num /= base;
    }

    while (*p && *p == _T('0') && end - decimal > 0) {
        p++;
        decimal++;
    }

    return p;
}

/* N.B.: when building with /QIfist casting rounds instead of truncating (i#763)!
 * Thus, use double2int_trunc() instead of casting.
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
static TCHAR *
TNAME(double_to_str)(double d, int decimal, TCHAR *buf, bool force_dot,
                     bool suppress_zeros)
/* clang-format on */
{
    /* support really big numbers with %f? */
    TCHAR tmpbuf[BUF_SIZE];
    TCHAR *pre, *post, *c;
    long predot, postdot, sub, i;

    /* get pre and post dot sections as integers */
    if (d < 0)
        d = -d;
    if (decimal > 0)
        predot = double2int_trunc(d);
    else
        predot = double2int(d);
    sub = 1;
    for (i = 0; i < decimal; i++)
        sub *= 10;
    postdot = double2int((d - double2int_trunc(d)) * (double)sub);
    if (postdot == sub) {
        /* we had a .9* that rounded up! */
        postdot = 0;
        predot++;
    }

    pre = TNAME(ulong_to_str)((ulong)predot, 10, tmpbuf, 1, false);
    for (i = 0, c = pre; *c; c++)
        buf[i++] = *c;
    if (force_dot || !(decimal == 0 || (suppress_zeros && postdot == 0))) {
        buf[i++] = _T('.');
        post = TNAME(ulong_to_str)((ulong)postdot, 10, tmpbuf, decimal, false);
        for (c = post; *c; c++)
            buf[i++] = *c;
        /* remove trailing zeros */
        if (suppress_zeros) {
            while (buf[i - 1] == _T('0'))
                i--;
        }
    }

    buf[i] = '\0';
    ASSERT(i < BUF_SIZE); /* make sure don't overflow buffer */
    return buf;
}

/* clang-format off */ /* (work around clang-format newline-after-type bug) */
static TCHAR *
TNAME(double_to_exp_str)(double d, int exp, int decimal, TCHAR *buf,
                         bool force_dot, bool suppress_zeros, bool caps)
/* clang-format on */
{
    TCHAR tmp_buf[BUF_SIZE];
    TCHAR *tc;
    int i = 0;
    uint abval;

    tc = TNAME(double_to_str)(d, decimal, tmp_buf, force_dot, suppress_zeros);
    while (*tc) {
        buf[i++] = *tc++;
    }
    if (caps)
        buf[i++] = _T('E');
    else
        buf[i++] = _T('e');
    if (exp < 0) {
        buf[i++] = _T('-');
        abval = -exp;
    } else {
        buf[i++] = _T('+');
        abval = exp;
    }
    /* exp value always printed as at least 2 characters */
    tc = TNAME(ulong_to_str)((ulong)abval, 10, tmp_buf, 2, false);
    while (*tc) {
        buf[i++] = *tc++;
    }
    buf[i] = '\0';
    ASSERT(i < BUF_SIZE); /* make sure don't overflow buffer */
    return buf;
}

/* i#386: separated out to avoid floating-point instrs in d_r_vsnprintf */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
static const TCHAR *
TNAME(d_r_vsnprintf_float)(double val, const TCHAR *c,
                           TCHAR prefixbuf[3], TCHAR buf[BUF_SIZE],
                           int decimal, bool space_flag,
                           bool plus_flag, bool pound_flag)
/* clang-format on */
{
    const TCHAR *str;
    bool caps = (*c == _T('E')) || (*c == _T('G'));
    double d = val;
    int exp = 0;
    bool is_g = (*c == _T('g') || *c == _T('G'));
    /* i#1213: we must mask all fpu exceptions prior to running this code,
     * as it assumes div-by-zero won't raise an exception.
     * The caller must have already saved the app's full fpu state.
     */
    dr_fpu_exception_init();
    /* check for NaN */
    if (val != val) {
        if (caps)
            str = _T("NAN");
        else
            str = _T("nan");
        if (space_flag)
            prefixbuf[0] = _T(' ');
        return str;
    }
    if (decimal == -1)
        decimal = 6; /* default */
    if (val >= 0 && space_flag)
        prefixbuf[0] = _T(' '); /* get prefix */
    if (val >= 0 && plus_flag)
        prefixbuf[0] = _T('+');
    if (val < 0)
        prefixbuf[0] = _T('-');
    /* check for inf */
    if (val == pos_inf || val == neg_inf) {
        if (caps)
            str = _T("INF");
        else
            str = _T("inf");
        return str;
    }
    if (*c == _T('f')) { /* ready to generate string now for f */
        str = TNAME(double_to_str)(val, decimal, buf, pound_flag, false);
        return str;
    }
    /* get exponent value */
    while (d >= 10.0 || d <= -10.0) {
        exp++;
        d = d / 10.0;
    }
    while (d < 1.0 && d > -1.0 && d != 0.0) {
        exp--;
        d = d * 10.0;
    }

    if (is_g)
        decimal--; /* g/G precision is number of signifigant digits */
    if (is_g && exp >= -4 && exp <= decimal) {
        /* exp is small enough for f, print without exponent */
        str = TNAME(double_to_str)(val, decimal, buf, pound_flag, !pound_flag);
    } else {
        /* print with exponent */
        str = TNAME(double_to_exp_str)(d, exp, decimal, buf, pound_flag,
                                       is_g && !pound_flag, caps);
    }
    return str;
}

/* Returns number of chars printed, not including the null terminator.
 * If number is larger than max,
 * prints max (without null) and returns -1.
 * For %S on Windows, converts between UTF-8 and UTF-16, and returns -1
 * if passed an invalid encoding.
 * (Thus, matches Windows snprintf, not Linux.)
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
int
TNAME(d_r_vsnprintf)(TCHAR *s, size_t max, const TCHAR *fmt, va_list ap)
/* clang-format on */
{
    const TCHAR *c;
    const TCHAR *str = NULL;
    TCHAR *start = s;
    TCHAR buf[BUF_SIZE];

    if (fmt == NULL)
        return 0;
    if (max == 0)
        goto max_reached;

    c = fmt;
    while (*c) {
        if (*c == _T('%')) {
            int fill = 0;
            TCHAR filler = _T(' ');
            int decimal = -1; /* codes defaults (6 int, 1 float, all string) */
            TCHAR charbuf[2] = { _T('\0'), _T('\0') };
            TCHAR prefixbuf[3] = { _T('\0'), _T('\0'), _T('\0') };
            TCHAR *prefix;
            bool minus_flag = false;
            bool plus_flag = false;
            bool pound_flag = false;
            bool space_flag = false;
            bool h_type = false;
            bool l_type = false;
            bool ll_type = false;
#ifdef IOX_WIDE_CHAR
            char *wstr = NULL;
#else
            wchar_t *wstr = NULL;
#endif
            prefix = prefixbuf;
            c++;
            ASSERT(*c);

            /* Collect flags -, +, #, 0,  */
            while (*c == _T('0') || *c == _T('-') || *c == _T('#') || *c == _T('+') ||
                   *c == _T(' ')) {
                if (*c == _T('0'))
                    filler = _T('0');
                if (*c == _T('-'))
                    minus_flag = true;
                if (*c == _T('+'))
                    plus_flag = true;
                if (*c == _T('#'))
                    pound_flag = true;
                if (*c == _T(' '))
                    space_flag = true;
                c++;
                ASSERT(*c);
            }
            if (minus_flag)
                filler = _T(' ');
            if (plus_flag)
                space_flag = false;

            /* get field width */
            if (*c == _T('*')) {
                fill = va_arg(ap, int);
                if (fill < 0) {
                    minus_flag = true;
                    fill = -fill;
                }
                c++;
                ASSERT(*c);
            } else {
                while (*c >= _T('0') && *c <= _T('9')) {
                    fill *= 10;
                    fill += *c - _T('0');
                    c++;
                    ASSERT(*c);
                }
            }

            /* get precision */
            if (*c == _T('.')) {
                c++;
                ASSERT(*c);
                decimal = 0;
                if (*c == _T('*')) {
                    decimal = va_arg(ap, int);
                    c++;
                    ASSERT(*c);
                } else {
                    while (*c >= _T('0') && *c <= _T('9')) {
                        decimal *= 10;
                        decimal += *c - _T('0');
                        c++;
                        ASSERT(*c);
                    }
                }
            }

            /* get size modifiers l, h, ll/L, z */
            if (*c == _T('l') || *c == _T('L') || *c == _T('h') || *c == _T('z')) {
                if (*c == _T('L'))
                    ll_type = true;
                if (*c == _T('h'))
                    h_type = true;
                if (*c == _T('z')) {
#if defined(WINDOWS) && defined(X64)
                    ll_type = true;
#else
                    l_type = true;
#endif
                }
                if (*c == _T('l')) {
                    c++;
                    ASSERT(*c);
                    if (*c == _T('l')) {
                        ll_type = true;
                        c++;
                        ASSERT(*c);
                    } else {
                        l_type = true;
                    }
                } else {
                    c++;
                    ASSERT(*c);
                }
            } else if (*c == _T('I')) { /* %I64 or %I32, to match Win32 */
                if (*(c + 1) == _T('6') && *(c + 2) == _T('4')) {
                    ll_type = true;
                    c += 3;
                    ASSERT(*c);
                } else if (*(c + 1) == _T('3') && *(c + 2) == _T('2')) {
                    l_type = true;
                    c += 3;
                    ASSERT(*c);
                } else
                    ASSERT(false && "unsupported printf code");
            }

            /* dispatch */
            switch (*c) {
            case _T('%'):
                charbuf[0] = _T('%');
                str = charbuf;
                break;
            case _T('d'):
            case _T('i'): {
                long val;
                ulong abval = 0;
                int64 val64;
                uint64 abval64 = 0;
                bool negative = false;
                if (decimal == -1)
                    decimal = 1; /* defaults */
                else
                    filler = _T(' ');
                if (ll_type) {
                    val64 = va_arg(ap, int64); /* get arg */
                    negative = (val64 < 0);
                    if (negative)
                        abval64 = -val64;
                    else
                        abval64 = val64;
                } else {
                    if (l_type)
                        val = va_arg(ap, long); /* get arg */
                    else if (h_type)
                        val = (long)va_arg(ap, int); /* short is promoted to int */
                    else
                        val = (long)va_arg(ap, int);
                    negative = (val < 0);
                    if (negative)
                        abval = -val;
                    else
                        abval = val;
                }
                if (!negative && space_flag)
                    prefixbuf[0] = _T(' '); /* set prefix */
                if (!negative && plus_flag)
                    prefixbuf[0] = _T('+');
                if (negative)
                    prefixbuf[0] = _T('-');
                /* generate string */
                if (ll_type)
                    str = TNAME(uint64_to_str)(abval64, 10, buf, decimal, false);
                else
                    str = TNAME(ulong_to_str)(abval, 10, buf, decimal, false);
                break;
            }
            case _T('u'):
                /* handle long long u type */
                if (decimal == -1)
                    decimal = 1;
                else
                    filler = _T(' ');
                if (ll_type) {
                    str = TNAME(uint64_to_str)((uint64)va_arg(ap, uint64), 10, buf,
                                               decimal, false);
                    break;
                }
                /* note no break */
            case _T('x'):
            case _T('X'):
            case _T('o'):
            case _T('p'): {
                ptr_uint_t val;
                bool caps = *c == _T('X');
                int base = 10;
                if (decimal == -1)
                    decimal = 1; /* defaults */
                else
                    filler = _T(' ');
                if (*c == _T('p'))
                    decimal = 2 * sizeof(void *); /* pointer precision */
                /* generate prefix */
                if ((pound_flag && *c != _T('u')) || (*c == _T('p'))) {
                    prefixbuf[0] = _T('0');
                    if (*c == _T('x') || *c == _T('p'))
                        prefixbuf[1] = _T('x');
                    if (*c == _T('X'))
                        prefixbuf[1] = _T('X');
                }
                if (*c == _T('o'))
                    base = 8; /* determine base */
                if (*c == _T('x') || *c == _T('X') || *c == _T('p'))
                    base = 16;
                ASSERT(sizeof(void *) == sizeof(val));
                if (*c == _T('p')) {
                    val = (ptr_uint_t)va_arg(ap, void *); /* get val */
#ifdef X64
                    str = TNAME(uint64_to_str)((uint64)val, base, buf, decimal, caps);
                    break;
#endif
                } else if (l_type)
                    val = (ptr_uint_t)va_arg(ap, ulong);
                else if (h_type)
                    val = (ptr_uint_t)va_arg(ap, uint); /* ushort promoted */
                else if (ll_type) {
                    str = TNAME(uint64_to_str)((uint64)va_arg(ap, uint64), base, buf,
                                               decimal, caps);
                    break;
                } else
                    val = (ptr_uint_t)va_arg(ap, uint);
                /* generate string */
                ASSERT(sizeof(val) >= sizeof(ulong));
                str = TNAME(ulong_to_str)((ulong)val, base, buf, decimal, caps);
                break;
            }
            case _T('c'):
                /* FIXME: using int instead of char seems to work for RH7.2 as
                 * well as 8.0, but using char crashes 8.0 but not 7.2
                 */
#ifdef VA_ARG_CHAR2INT
                charbuf[0] = (TCHAR)va_arg(ap, int); /* char -> int in va_list */
#else
                charbuf[0] = va_arg(ap, TCHAR);
#endif
                str = charbuf;
                break;
            case _T('s'):
                if (!IF_WIDE_ELSE(h_type, l_type)) {
                    str = va_arg(ap, TCHAR *);
                    break;
                }
                /* fall-through */
            case _T('S'):
#ifdef IOX_WIDE_CHAR
                h_type = true;
                wstr = va_arg(ap, char *);
#else
                l_type = true;
                wstr = va_arg(ap, wchar_t *);
#endif
                break;
            case _T('g'):
            case _T('G'):
                if (decimal == 0 || decimal == -1)
                    decimal = 1; /* default */
                /* no break */
            case _T('e'):
            case _T('E'):
            case _T('f'): {
                /* pretty sure will always be promoted to a double in arg list */
                double val = va_arg(ap, double);
                str = TNAME(d_r_vsnprintf_float)(val, c, prefixbuf, buf, decimal,
                                                 space_flag, plus_flag, pound_flag);
                break;
            }
            case _T('n'): {
                /* save num of chars printed so far in address specified */
                uint num_char = (uint)(s - start);
                /* yes, snprintf on all platforms returns int, not ssize_t */
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(s - start)));
                if (l_type) {
                    long *val = va_arg(ap, long *);
                    *val = (long)num_char;
                } else if (h_type) {
                    short *val = va_arg(ap, short *);
                    *val = (short)num_char;
                } else {
                    int *val = va_arg(ap, int *);
                    *val = num_char;
                }
                buf[0] = '\0';
                str = buf;
                break;
            }
                /* FIXME : support the following? */
            case _T('a'):
            case _T('A'):
            default: ASSERT_NOT_REACHED();
            }

            /* if filler is 0 fill after prefix, else fill before prefix */
            /* if - flag then fill after str and ignore filler type */

            /* calculate number of fill characters */
            if (fill > 0) {
                size_t plen = IF_WIDE_ELSE(wcslen, strlen)(prefix);
                if (wstr != NULL) {
                    /* XXX: this doesn't take into account UTF-16 or UTF-8
                     * multi-byte chars.  For now we just don't support
                     * properly filling those.  It should only matter
                     * for pretty-printing.
                     */
                    size_t wlen = IF_WIDE_ELSE(strlen, wcslen)(wstr);
                    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(wlen + plen)));
                    fill -= (uint)(wlen + plen);
                } else {
                    size_t len = IF_WIDE_ELSE(wcslen, strlen)(str);
                    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(len + plen)));
                    fill -= (uint)(len + plen);
                }
            }
            /* insert prefix if filler is 0, filler won't be 0 if - flag is set */
            if (filler == _T('0')) {
                while (*prefix) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    *s = *prefix;
                    s++;
                    prefix++;
                }
            }
            /* fill now if not left justified */
            if (fill > 0 && !minus_flag) {
                int i;
                for (i = 0; i < fill; i++) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    *s = filler;
                    s++;
                }
            }
            /* insert prefix if not 0 filling */
            if (filler != _T('0')) {
                while (*prefix) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    *s = *prefix;
                    s++;
                    prefix++;
                }
            }
            /* insert the actual str representation */
            if (wstr != NULL) {
#ifdef WINDOWS
                /* We follow Linux sprintf which has precision on multi-byte
                 * transformation as specifying bytes, not unicode chars.
                 * MSDN docs say "characters", but Windows %S doesn't support
                 * any conversion other than truncating to ascii (or 0 if not
                 * ascii).
                 */
                ssize_t els;
                size_t max_bytes = max - (s - start);
                /* string precision */
                if ((*c == _T('s') || *c == _T('S')) && decimal >= 0 &&
                    (size_t)decimal < max_bytes)
                    max_bytes = decimal;
                els = IF_WIDE_ELSE(utf8_to_utf16, utf16_to_utf8)(s, max_bytes, wstr, 0,
                                                                 NULL);
                if (els < 0)
                    return -1;
                s += els;
                if ((size_t)(s - start) >= max)
                    goto max_reached;
#else
                while (*wstr) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    if ((*c == _T('s') || *c == _T('S')) && decimal == 0)
                        break; /* check string precision */
                    decimal--;
                    /* we only support ascii */
                    ASSERT((unsigned short)(*wstr) <= UCHAR_MAX);
                    *s = (TCHAR)*wstr;
                    s++;
                    wstr++;
                }
#endif
            } else {
                if (str == NULL)
                    str = _T("<NULL>");
                while (*str) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    if (*c == _T('s') && decimal == 0)
                        break; /* check string precision */
                    decimal--;
                    *s = *str;
                    s++;
                    str++;
                }
            }
            /* if left justified do the fill now after the actual string */
            if (fill > 0 && minus_flag) {
                int i;
                for (i = 0; i < fill; i++) {
                    if ((size_t)(s - start) >= max)
                        goto max_reached;
                    *s = filler;
                    s++;
                }
            }
            c++;
        } else {
            const TCHAR *cstart = c;
            int nbytes = 0;
            while (*c && *c != _T('%')) {
                nbytes++;
                c++;
            }
            while (cstart < c) {
                if ((size_t)(s - start) >= max)
                    goto max_reached;
                *s = *cstart;
                s++;
                cstart++;
            }
        }
    }

    if (max == 0 || (size_t)(s - start) < max)
        *s = '\0';

    /* yes, snprintf on all platforms returns int, not ssize_t */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(s - start)));
    return (int)(s - start);

max_reached:
    return -1;
}

/* Returns number of chars printed.  If number is larger than max,
 * prints max (without null) and returns -1.
 * (Thus, matches Windows snprintf, not Linux.)
 */
/* clang-format off */ /* (work around clang-format newline-after-type bug) */
int
TNAME(d_r_snprintf)(TCHAR *s, size_t max, const TCHAR *fmt, ...)
/* clang-format on */
{
    int res;
    va_list ap;
    ASSERT(s);
    va_start(ap, fmt);
    res = TNAME(d_r_vsnprintf)(s, max, fmt, ap);
    va_end(ap);
    return res;
}

#undef TCHAR
#undef _T
#undef TNAME
#undef IF_WIDE_ELSE
