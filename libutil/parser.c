/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2006 VMware, Inc.  All rights reserved.
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

#include "share.h"
#include "parser.h"
#include <stdio.h>

#ifndef UNIT_TEST

char *
next_token_sep(char *start, SIZE_T *len, char sep)
{
    char *curtok = start;
    char seps[16];
    _snprintf(seps, 16, "%c\r\n", sep);

    DO_ASSERT(start != NULL);
    DO_ASSERT(len != NULL);

    *len = 0;

    if (start == NULL)
        return NULL;

    /* special case: '=' followed by a newline */
    if (start[0] == sep && (start[1] == '\r' || start[1] == '\n')) {
        curtok += 1;
        return curtok;
    }

    /* determine the next token */
    while (curtok[0] != '\0' && *len == 0) {
        *len = strcspn(curtok, seps);
        if (*len == 0)
            curtok = curtok + strspn(curtok, seps);
    }

    return curtok;
}

char *
next_token(char *start, SIZE_T *len)
{
    return next_token_sep(start, len, '=');
}

/* assumes that start points directly after the start_delimeter
 * returns the size of the message block, or -1 on error */
char *
get_message_block_size(char *start, WCHAR *end_delimiter_w, SIZE_T *size)
{
    char *endptr;
    char end_delimiter[MAX_PATH];

    DO_ASSERT(start != NULL);
    DO_ASSERT(end_delimiter_w != NULL);
    DO_ASSERT(size != NULL);

    _snprintf(end_delimiter, MAX_PATH, "%S", end_delimiter_w);
    endptr = strstr(start, end_delimiter);

    if (endptr == NULL) {
        DO_DEBUG(DL_WARN, printf("No %s end delimiter!\n", end_delimiter););
        *size = 0xffffffff;
        return NULL;
    } else {
        DO_DEBUG(DL_VERB,
                 printf("Block size %zd for %s:\n", endptr - start, end_delimiter););
        *size = endptr - start;
        return endptr + strlen(end_delimiter);
    }
}

/* takes an optional separator, by default '=' */
char *
parse_line_sep(char *start, char sep, BOOL *done, WCHAR *param, WCHAR *value,
               SIZE_T maxchars)
{
    char *curtok;
    SIZE_T toklen = 0;
    const WCHAR *prefix;

    DO_ASSERT(start != NULL);
    DO_ASSERT(done != NULL);
    DO_ASSERT(param != NULL);
    DO_ASSERT(maxchars > 0);

    *done = FALSE;
    param[0] = L'\0';
    value[0] = L'\0';

    curtok = start;

    curtok = next_token_sep(start, &toklen, sep);

    DO_DEBUG(DL_FINEST, printf("curtok (tl=%zu):\n%s", toklen, curtok););

    if (toklen == 0 && curtok[0] == '\0') {
        *done = TRUE;
        goto parse_line_out;
    }

    _snwprintf(param, MIN(toklen, maxchars - 1), L"%S", curtok);
    param[MIN(toklen, maxchars - 1)] = L'\0';

    DO_DEBUG(DL_FINEST,
             printf("curtok: %s, nc=%d (%d or %d?)\n", curtok, curtok[toklen + 1], '\r',
                    '\n'););

    /* handle the empty option properly */
    if (curtok[toklen] == '\r' || curtok[toklen] == '\n') {
        value[0] = L'\0';
    } else {
        curtok = next_token_sep(curtok + toklen, &toklen, sep);

        if (toklen == 0 && curtok[0] == '\0') {
            *done = TRUE;
            goto parse_line_out;
        }

        DO_DEBUG(DL_FINEST, printf("vt(%zu): %s\n", toklen, curtok););

        /* all values that start with \\ have DR_HOME prepended */
        if ('\\' == curtok[0])
            prefix = get_dynamorio_home();
        else
            prefix = L"";

        _snwprintf(value, MIN(toklen + wcslen(prefix), maxchars - 1), L"%s%S", prefix,
                   curtok);
        value[MIN(toklen + wcslen(prefix), maxchars - 1)] = L'\0';
    }

parse_line_out:

    DO_DEBUG(DL_FINEST, printf("parsed line: %S(%zu)=%S\n", param, toklen, value););

    return curtok + toklen;
}

/* first argument is where to start searching from.
 * returns a value which is the right place to start searching
 *  from for the next token.
 * a line is either a single token, or an NVP (separated by =). */
char *
parse_line(char *start, BOOL *done, WCHAR *param, WCHAR *value, SIZE_T maxchars)
{
    return parse_line_sep(start, '=', done, param, value, maxchars);
}

void
msg_append(char *msg_buffer, SIZE_T maxchars, WCHAR *data, SIZE_T *accumlen)
{
    DO_ASSERT(accumlen != NULL);

    if (data == NULL)
        return;

    if (msg_buffer != NULL && strlen(msg_buffer) + wcslen(data) < maxchars) {
        SIZE_T oldsz = strlen(msg_buffer);
        DO_DEBUG(DL_FINEST,
                 printf("msg_buffer: %s, os=%zu, data=%S\n", msg_buffer, oldsz, data););
        _snprintf(msg_buffer + oldsz, maxchars - oldsz, "%S", data);
        msg_buffer[oldsz + wcslen(data)] = '\0';
        DO_DEBUG(DL_FINEST, printf("msg_buffer: %s\n", msg_buffer););
        DO_ASSERT(strlen(msg_buffer) == oldsz + wcslen(data));
    }

    *accumlen += wcslen(data);
    return;
}

/* ok this is annoying.
 *
 * apparently there's a bug in MS's CRT that causes \r\n to get
 *  converted to \r\r\n.
 *
 * see the unit tests below for an example of this bug.
 *
 * i guess we're ok with \r\r\n since notepad handles it fine. that's
 *  preferable to just \n which notepad can't handle.
 */

void
msg_append_nvp(char *msg_buffer, SIZE_T maxchars, SIZE_T *accumlen, WCHAR *name,
               WCHAR *value)
{
    /* exclude installation-specific parameters */
    if (0 == wcscmp(name, L_DYNAMORIO_VAR_HOME) ||
        0 == wcscmp(name, L_DYNAMORIO_VAR_LOGDIR))
        return;

    msg_append(msg_buffer, maxchars, name, accumlen);
    msg_append(msg_buffer, maxchars, L_EQUALS, accumlen);

    /* relativize any paths to DRHOME */
    if (0 == wcsncmp(value, get_dynamorio_home(), wcslen(get_dynamorio_home())))
        msg_append(msg_buffer, maxchars, value + wcslen(get_dynamorio_home()), accumlen);
    else
        msg_append(msg_buffer, maxchars, value, accumlen);

    msg_append(msg_buffer, maxchars, L_NEWLINE, accumlen);
}

#else // ifdef UNIT_TEST

int
main()
{
    char *testline = "GLOBAL_PROTECT=1\r\nBEGIN_BLOCK\r\nAPP_NAME=inetinfo."
                     "exe\r\nDYNAMORIO_OPTIONS=\r\nFOO=\\bar.dll\r\n";
    WCHAR *sample = L"sample.mfp";
    SIZE_T len;
    DWORD res;
    char *policy;

    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    /* load the sample policy file for testing */
    res = read_file_contents(sample, NULL, 0, &len);
    DO_ASSERT(res == ERROR_MORE_DATA);
    DO_ASSERT(len > 1000);

    policy = (char *)malloc(len);
    res = read_file_contents(sample, policy, len, NULL);
    DO_ASSERT(res == ERROR_SUCCESS);

    /* next_token tests */
    {
        char *tok, *line, *ptr;
        UINT len;

        line = testline;
        tok = next_token(line, &len);

        DO_ASSERT(line == tok);
        DO_ASSERT(len == strlen("GLOBAL_PROTECT"));

        ptr = line + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(ptr + 1 == tok);
        DO_ASSERT(len == 1);

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(0 == strncmp(tok, "BEGIN_BLOCK", strlen("BEGIN_BLOCK")));
        DO_ASSERT(len == strlen("BEGIN_BLOCK"));

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(0 == strncmp(tok, "APP_NAME", strlen("APP_NAME")));
        DO_ASSERT(len == strlen("APP_NAME"));

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(0 == strncmp(tok, "inetinfo.exe", strlen("inetinfo.exe")));
        DO_ASSERT(len == strlen("inetinfo.exe"));

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(0 == strncmp(tok, "DYNAMORIO_OPTIONS", strlen("DYNAMORIO_OPTIONS")));
        DO_ASSERT(len == strlen("DYNAMORIO_OPTIONS"));

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(len == 0);

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(0 == strncmp(tok, "FOO", strlen("FOO")));
        DO_ASSERT(len == strlen("FOO"));

        ptr = tok + len;
        tok = next_token(ptr, &len);
        DO_ASSERT(len == strlen("\\bar.dll"));

        /* FIXME: long string / overrun tets.. */
    }

    /* get_message_block_size tests */
    {
        char *line, *ptr;
        UINT len;

        line = "a b\r\nfoo bar=blah\r\nEND_TEST_BLOCK\r\n";
        ptr = get_message_block_size(line, L"END_TEST_BLOCK", &len);
        DO_ASSERT(len == 19);
        DO_ASSERT(ptr == line + strlen(line) - 2);
    }

    /* parse_line tests */
    {
        WCHAR param[MAX_PATH], value[MAX_PATH];
        BOOL done;
        char *ptr, *line = testline;

        ptr = parse_line(line, &done, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT_WSTR_EQ(param, L"GLOBAL_PROTECT");
        DO_ASSERT_WSTR_EQ(value, L"1");

        ptr = parse_line(ptr, &done, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT_WSTR_EQ(param, L"BEGIN_BLOCK");
        DO_ASSERT_WSTR_EQ(value, L"");

        ptr = parse_line(ptr, &done, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT_WSTR_EQ(param, L"APP_NAME");
        DO_ASSERT_WSTR_EQ(value, L"inetinfo.exe");

        ptr = parse_line(ptr, &done, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT_WSTR_EQ(param, L"DYNAMORIO_OPTIONS");
        DO_ASSERT_WSTR_EQ(value, L"");

        ptr = parse_line(ptr, &done, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT_WSTR_EQ(param, L"FOO");
        DO_ASSERT(NULL != wcsstr(value, L"bar"));
        DO_ASSERT(NULL != wcsstr(value, get_dynamorio_home()));
    }

    /* msg_append tests */
    {
        char buf[MAX_PATH];
        UINT loc;
        buf[0] = '\0';

        loc = 0;
        msg_append(buf, 0, L"GLOBAL", &loc);
        DO_ASSERT(0 == strlen(buf));
        DO_ASSERT(6 == loc);

        loc = 0;
        msg_append(buf, MAX_PATH, L"GLOBAL", &loc);
        DO_ASSERT(0 == strcmp(buf, "GLOBAL"));
        DO_ASSERT(6 == loc);

        loc = 6;
        msg_append(buf, 0, L"_PROTECT", &loc);
        DO_ASSERT(0 == strcmp(buf, "GLOBAL"));
        DO_ASSERT(14 == loc);

        loc = 6;
        msg_append(buf, 6, L"_PROTECT", &loc);
        DO_ASSERT(0 == strcmp(buf, "GLOBAL"));
        DO_ASSERT(14 == loc);

        loc = 6;
        msg_append(buf, MAX_PATH, L"_PROTECT", &loc);
        DO_ASSERT(0 == strcmp(buf, "GLOBAL_PROTECT"));
        DO_ASSERT(14 == loc);
    }

    /* MSVCRT \r\r\n bug */
    {
        WCHAR *outfn = L"testcr.txt";
        char *str = "line\r\n";
        char buf[MAX_PATH];

        res = write_file_contents(outfn, str, TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);

        res = read_file_contents(outfn, buf, MAX_PATH, NULL);
        DO_ASSERT(res == ERROR_SUCCESS);

        DO_ASSERT(0 == strcmp(buf, str));

        /* this passes: but check out the file in emacs!
         * apparently the bug works both ways. */
    }

    printf("All Test Passed\n");

    return 0;
}

#endif
