/* **********************************************************
 * Copyright (c) 2011 Google, Inc.  All rights reserved.
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
 * io.c - Linux specific routines for i/o that will not
 * trigger problematic pthread calls.
 * For PR 219380 we also export this routine to clients on Windows.
 */

/* FIXME: failure modes should be more gracefull then failing asserts in most places */

/* avoid depending on __isoc99_vsscanf which requires glibc >= 2.7 */
#define _GNU_SOURCE 1
#include <stdio.h>
#undef _GNU_SOURCE

#include "globals.h"
#include <string.h>
#include <stdarg.h> /* for varargs */

#define VA_ARG_CHAR2INT
#define BUF_SIZE 64

const static char base_letters[] = {
    '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};
const static char base_letters_cap[] = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

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

/* convert uint64 to a string */
static char *
uint64_to_str(uint64 num, int base, char * buf, int decimal, bool caps)
{
    int    cnt;
    char   *p = buf;
    int    end = (43 > decimal ? 43 : decimal);
    ASSERT(decimal < BUF_SIZE - 1); /* so don't overflow buf */

    buf[end] = '\0';
    for (cnt = end - 1; cnt >= 0; cnt--) {
        buf[cnt] = caps ? base_letters_cap[(num %base)] : base_letters[(num % base)];
        num /= base;
    }

    while (*p && *p == '0' && end - decimal > 0) {
        p++;
        decimal++;
    }
    
    return p;
}

/* convert ulong to a string */
static char *
ulong_to_str(ulong num, int base, char *buf, int decimal, bool caps)
{
    int    cnt;
    char   *p = buf;
    int    end = (22 > decimal ? 22 : decimal); /* room for 64 bits octal */
    ASSERT(decimal < BUF_SIZE - 1); /* so don't overflow buf */

    buf[end] = '\0';
    for (cnt = end - 1; cnt >= 0; cnt--) {
        buf[cnt] = caps ? base_letters_cap[(num %base)] : base_letters[(num % base)];
        num /= base;
    }

    while (*p && *p == '0' && end - decimal> 0) {
        p++;
        decimal++;
    }

    return p;
}

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

static char *
double_to_str(double d, int decimal, char *buf, bool force_dot, bool suppress_zeros)
{
    /* support really big numbers with %f? */
    char tmpbuf[BUF_SIZE];
    char *pre, *post, *c;
    long predot, postdot, sub, i;

    /* get pre and post dot sections as integers */
    if (d < 0)
        d = -d;
    if (decimal > 0)
        predot = (long)d;
    else
        predot = double2int(d);
    sub = 1;
    for (i=0; i<decimal; i++)
        sub *= 10;
    postdot = double2int((d - (long)d) * (double)sub);
    if (postdot == sub) {
        /* we had a .9* that rounded up! */
        postdot = 0;
        predot++;
    }

    pre = ulong_to_str((ulong)predot, 10, tmpbuf, 1, false);
    for (i=0, c = pre; *c; c++)
        buf[i++] = *c;
    if (force_dot || !(decimal == 0 || (suppress_zeros && postdot == 0))) {
        buf[i++] = '.';
        post = ulong_to_str((ulong)postdot, 10, tmpbuf, decimal, false);
        for (c = post; *c; c++)
            buf[i++] = *c;
        /* remove trailing zeros */
        if (suppress_zeros) {
            while (buf[i-1] == '0')
                i--; 
        }
    }

    buf[i] = '\0';
    ASSERT(i<BUF_SIZE);  /* make sure don't overflow buffer */
    return buf;
}

static char *
double_to_exp_str(double d, int exp, int decimal, char * buf, bool force_dot, bool suppress_zeros, bool caps)
{
    char tmp_buf[BUF_SIZE];
    char *tc;
    int i = 0;
    uint abval;
    
    tc = double_to_str(d, decimal, tmp_buf, force_dot, suppress_zeros);
    while (*tc) {
        buf[i++] = *tc++;
    }
    if (caps)
        buf[i++] = 'E';
    else
        buf[i++] = 'e';
    if (exp < 0) {
        buf[i++] = '-';
        abval = -exp;
    } else {
        buf[i++] = '+';
        abval = exp;
    }
    /* exp value always printed as at least 2 characters */
    tc = ulong_to_str((ulong)abval, 10, tmp_buf, 2, false);
    while (*tc) {
        buf[i++] = *tc++;
    }
    buf[i] = '\0';
    ASSERT(i < BUF_SIZE); /* make sure don't overflow buffer */
    return buf;
}

/* i#386: separated out to avoid floating-point instrs in our_vsnprintf */
static char *
our_vsnprintf_float(double val, const char *c, char prefixbuf[3], char buf[BUF_SIZE],
                    int decimal, bool space_flag, bool plus_flag, bool pound_flag)
{
    char *str;
    bool caps = (*c == 'E') || (*c == 'G');
    double d = val;
    int exp = 0;
    bool is_g = (*c == 'g' || *c == 'G'); 
    /* check for NaN */
    if (val != val) {
        if (caps)
            str = "NAN";
        else
            str = "nan";
        if (space_flag)
            prefixbuf[0] = ' ';
        return str;
    }
    if (decimal == -1)
        decimal = 6; /* default */
    if (val >= 0 && space_flag)
        prefixbuf[0] = ' '; /* get prefix */
    if (val >= 0 && plus_flag)
        prefixbuf[0] = '+';
    if (val < 0)
        prefixbuf[0] = '-';
    /* check for inf */
    if (val == pos_inf || val == neg_inf) {
        if (caps)
            str = "INF";
        else
            str = "inf";
        return str;
    }
    if (*c == 'f') { /* ready to generate string now for f */
        str = double_to_str(val, decimal, buf, pound_flag, false);
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
        str = double_to_str(val, decimal, buf, pound_flag, !pound_flag);
    } else {
        /* print with exponent */
        str = double_to_exp_str(d, exp, decimal, buf, pound_flag,
                                is_g && !pound_flag, caps);
    }
    return str;
}

/* Returns number of chars printed.  If number is larger than max,
 * prints max (without null) and returns -1.
 * (Thus, matches Windows snprintf, not Linux.)
 */
int
our_vsnprintf(char *s, size_t max, const char *fmt, va_list ap)
{
    const char *c;
    char     *str = NULL;
    char     *start = s;
    char     buf[BUF_SIZE];
    
    if (fmt == NULL)
        return 0;
    
    c = fmt;
    while (*c) {
        if (*c == '%') {
            int  fill = 0;
            char filler = ' ';
            int decimal = -1; /* codes defaults (6 int, 1 float, all string) */
            char charbuf[2] = {'\0','\0'};
            char prefixbuf[3] = {'\0','\0','\0'};
            char *prefix;
            bool minus_flag = false;
            bool plus_flag = false;
            bool pound_flag = false;
            bool space_flag = false;
            bool h_type = false;
            bool l_type = false;
            bool ll_type = false;
            prefix = prefixbuf;
            c++;
            ASSERT(*c);

            /* Collect flags -, +, #, 0,  */
            while (*c == '0' || *c == '-' || *c == '#' || *c == '+' || *c == ' ') {
                if (*c == '0') 
                    filler = '0';
                if (*c == '-') 
                    minus_flag = true;
                if (*c == '+') 
                    plus_flag = true;
                if (*c == '#') 
                    pound_flag = true;
                if (*c == ' ') 
                    space_flag = true;
                c++;
                ASSERT(*c);
            }
            if (minus_flag) 
                filler = ' ';
            if (plus_flag) 
                space_flag = false;
        
            /* get field width */
            if (*c == '*') {
                fill = va_arg(ap, int);
                if (fill < 0) {
                    minus_flag = true;
                    fill = -fill;
                }
                c++;
                ASSERT(*c);
            } else {
                while (*c >= '0' && *c <= '9') {
                    fill *= 10;
                    fill += *c - '0';
                    c++;
                    ASSERT(*c);
                }
            }

            /* get precision */
            if (*c == '.') {
                c++;
                ASSERT(*c);
                decimal = 0;
                if (*c == '*') {
                    decimal = va_arg(ap, int);
                    c++;
                    ASSERT(*c);
                } else {
                    while (*c >= '0' && *c <= '9') {
                        decimal *= 10;
                        decimal += *c - '0';
                        c++;
                        ASSERT(*c);
                    }
                }
            }

            /* get size modifiers l, h, ll/L */
            if (*c == 'l' || *c == 'L' || *c == 'h') {
                if (*c == 'L') 
                    ll_type = true;
                if (*c == 'h') 
                    h_type = true;
                if (*c == 'l') {
                    c++;
                    ASSERT(*c);
                    if (*c == 'l') {
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
            } else if (*c == 'I') { /* %I64 or %I32, to match Win32 */
                if (*(c+1)=='6' && *(c+2)=='4') {
                    ll_type = true;
                    c += 3;
                    ASSERT(*c);
                } else if (*(c+1)=='3' && *(c+2)=='2') {
                    l_type = true;
                    c += 3;
                    ASSERT(*c);
                } else
                    ASSERT(false && "unsupported printf code");
            }

            /* dispatch */
            switch (*c) {
            case '%':
                charbuf[0] = '%';
                str = charbuf;
                break;
            case 'd':
            case 'i':
                {
                    long val;
                    ulong abval;
                    if (decimal == -1) 
                        decimal = 1;  /* defaults */
                    else
                        filler = ' ';
                    if (l_type) 
                        val = va_arg(ap, long);  /* get arg */
                    else if (h_type) 
                        val = (long)va_arg(ap, int); /* short is promoted to int */
                    else
                        val = (long)va_arg(ap, int);
                    if (val >= 0 && space_flag) 
                        prefixbuf[0] = ' ';  /* set prefix */
                    if (val >= 0 && plus_flag) 
                        prefixbuf[0] = '+';
                    if (val < 0) {
                        prefixbuf[0] = '-';
                        abval = -val;
                    } else {
                        abval = val;
                    }
                    str = ulong_to_str(abval, 10, buf, decimal, false); /* generate string */
                    break;
                }
            case 'u':
                /* handle long long u type */
                if (decimal == -1)
                    decimal = 1;
                else
                    filler = ' ';
                if (ll_type) {
                    str = uint64_to_str((uint64)va_arg(ap, uint64), 10, buf, decimal, false);
                    break;
                }
                /* note no break */
            case 'x':
            case 'X':
            case 'o':
            case 'p':
                {
                    ptr_uint_t val;
                    bool caps = *c == 'X';
                    int base = 10;
                    if (decimal == -1)
                        decimal = 1; /* defaults */
                    else
                        filler = ' ';
                    if (*c == 'p')
                        decimal = 2 * sizeof(void *); /* pointer precision */
                    if ((pound_flag && *c != 'u') || (*c == 'p')) {  /* generate prefix */
                        prefixbuf[0] = '0';
                        if (*c == 'x' || *c == 'p')
                            prefixbuf[1] = 'x';
                        if (*c == 'X')
                            prefixbuf[1] = 'X';
                    }
                    if (*c == 'o')
                        base = 8;    /* determine base */
                    if (*c == 'x' || *c == 'X' || *c == 'p')
                        base = 16;
                    ASSERT(sizeof(void *) == sizeof(val));
                    if (*c == 'p') {
                        val = (ptr_uint_t) va_arg(ap, void *);  /* get val */
#ifdef X64
                        str = uint64_to_str((uint64)val, base, buf, decimal, caps);
                        break;
#endif
                    } else if (l_type)
                        val = (ptr_uint_t) va_arg(ap, ulong);
                    else if (h_type)
                        val = (ptr_uint_t) va_arg(ap, uint); /* ushort promoted */
                    else if (ll_type) {
                        str = uint64_to_str((uint64)va_arg(ap, uint64), base, buf,
                                            decimal, caps);
                        break;
                    } else
                        val = (ptr_uint_t) va_arg(ap, uint);
                    /* generate string */
                    ASSERT(sizeof(val) >= sizeof(ulong));
                    str = ulong_to_str((ulong)val, base, buf, decimal, caps);
                    break;
                }
            case 'c':
                /* FIXME: using int instead of char seems to work for RH7.2 as
                 * well as 8.0, but using char crashes 8.0 but not 7.2
                 */
#ifdef VA_ARG_CHAR2INT
                charbuf[0] = (char) va_arg(ap, int); /* char -> int in va_list */
#else
                charbuf[0] = va_arg(ap, char);
#endif
                str = charbuf;
                break;
            case 's':
                str = va_arg(ap, char*);
                break;
            case 'g':
            case 'G':
                if (decimal == 0 || decimal == -1)
                    decimal = 1; /* default */
                /* no break */
            case 'e':
            case 'E':
            case 'f':
                {
                    /* pretty sure will always be promoted to a double in arg list */
                    double val = va_arg(ap, double);
                    str = our_vsnprintf_float(val, c, prefixbuf, buf, decimal,
                                              space_flag, plus_flag, pound_flag);
                    break;
                }
            case 'n':
                {
                    /* save num of chars printed so far in address specified */
                    uint num_char = (uint) (s - start);
                    /* yes, snprintf on all platforms returns int, not ssize_t */
                    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_int(s - start)));
                    if (l_type) {
                        long * val = va_arg(ap, long *);
                        *val = (long) num_char;
                    } else if (h_type) {
                        short * val = va_arg(ap, short *);
                        *val = (short) num_char;
                    } else {
                        int * val = va_arg(ap, int *);
                        *val = num_char;
                    }
                    buf[0] = '\0';
                    str = buf;
                    break;
                }
                /* FIXME : support the following? */
            case 'a':
            case 'A':
            default:
                ASSERT_NOT_REACHED();
            }

            /* if filler is 0 fill after prefix, else fill before prefix */
            /* if - flag then fill after str and ignore filler type */

            /* calculate number of fill characters */
            if (fill > 0) {
                IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(strlen(str) + strlen(prefix))));
                fill -= (uint) (strlen(str) + strlen(prefix));
            }
            /* insert prefix if filler is 0, filler won't be 0 if - flag is set */
            if (filler == '0') {
                while (*prefix) {
                    if (max > 0 && (size_t)(s - start) >= max)
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
                    if (max > 0 && (size_t)(s - start) >= max)
                        goto max_reached;
                    *s = filler;
                    s++;
                }
            }
            /* insert prefix if not 0 filling */
            if (filler != '0') {
                while (*prefix) {
                    if (max > 0 && (size_t)(s - start) >= max)
                        goto max_reached;
                    *s = *prefix;
                    s++;
                    prefix++;
                }
            }
            /* insert the actual str representation */
            while (*str) {
                if (max > 0 && (size_t)(s - start) >= max)
                    goto max_reached;
                if (*c == 's' && decimal == 0)
                    break;  /* check string precision */
                decimal--;
                *s = *str;
                s++;
                str++;
            }
            /* if left justified do the fill now after the actual string */
            if (fill > 0 && minus_flag) {
                int i;
                for (i = 0; i < fill; i++) {
                    if (max > 0 && (size_t)(s - start) >= max)
                        goto max_reached;
                    *s = filler;
                    s++;
                }
            }
            c++;
        }
        else {
            const char *cstart = c;
            int  nbytes = 0;
            while (*c && *c != '%') {
                nbytes++;
                c++;
            }
            while (cstart < c) {
                if (max > 0 && (size_t)(s - start) >= max)
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
    return (int) (s - start);

 max_reached:
    return -1;
}

/* Returns number of chars printed.  If number is larger than max,
 * prints max (without null) and returns -1.
 * (Thus, matches Windows snprintf, not Linux.)
 */
int
our_snprintf(char *s, size_t max, const char *fmt, ...)
{
    int res;
    va_list ap;
    ASSERT(s);
    va_start(ap, fmt);
    res = our_vsnprintf(s, max, fmt, ap);
    va_end(ap);
    return res;
}

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

