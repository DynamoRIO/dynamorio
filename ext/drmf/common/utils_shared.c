/* **********************************************************
 * Copyright (c) 2011-2024 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2010 VMware, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Code shared between Dr. Memory and the DRMF Extensions (which don't
 * want to link the full utils.c)
 */

#include "dr_api.h"
#include "utils.h"
#include <ctype.h> /* for tolower */
#include <string.h>

char *
strnchr(const char *str, int find, size_t max)
{
    register const char *s = str;
    register char c = (char) find;
    while (true) {
        if (s - str >= max)
            return NULL;
        if (*s == c)
            return (char *) s;
        if (*s == '\0')
            return NULL;
        s++;
    }
    return NULL;
}

/* Not available in ntdll CRT so we supply our own.
 * It is available on Mac and Android, and we want to avoid it for libs that do not
 * want a libc dependence.
 */
#if !defined(MACOS) && !defined(ANDROID) && !defined(NOLINK_STRCASESTR)
char *
strcasestr(const char *text, const char *pattern)
{
    const char *cur_text, *cur_pattern, *root;
    cur_text = text;
    root = text;
    cur_pattern = pattern;
    while (true) {
        if (*cur_pattern == '\0')
            return (char *) root;
        if (*cur_text == '\0')
            return NULL;
        /* XXX DRi#943: toupper is better, for int18n, and we need to call
         * islower() first to be safe for all tolower() implementations.
         * Even better would be switching to our own locale-independent case
         * folding.
         */
        if ((char)tolower(*cur_text) == (char)tolower(*cur_pattern)) {
            cur_text++;
            cur_pattern++;
        } else {
            root++;
            cur_text = root;
            cur_pattern = pattern;
        }
    }
}
#endif

char *
drmem_strdup(const char *src, heapstat_t type)
{
    char *dup = NULL;
    if (src != NULL) {
        size_t len = strlen(src);
        dup = global_alloc(len+1, type);
        strncpy(dup, src, len+1);
    }
    return dup;
}

char *
drmem_strndup(const char *src, size_t max, heapstat_t type)
{
    char *dup = NULL;
    /* deliberately not calling strlen on src since may be quite long */
    const char *c;
    size_t sz;
    for (c = src; *c != '\0' && (c - src < max); c++)
        ; /* nothing */
    sz = (c - src < max) ? c -src : max;
    if (src != NULL) {
        dup = global_alloc(sz + 1, type);
        strncpy(dup, src, sz);
        dup[sz] = '\0';
    }
    return dup;
}

/* see description in header */
const char *
find_next_line(const char *start, const char *eof, const char **sol,
               const char **eol DR_PARAM_OUT, bool skip_ws)
{
    const char *line = start, *newline, *next_line;
    /* First, set "line" to start of line and "newline" to end (pre-whitespace) */
    /* We have to use strnchr to avoid SIGBUS on non-Windows */
    newline = strnchr(line, '\n', eof - line);
    if (newline == NULL) {
        newline = eof; /* handle EOF w/o trailing \n */
        next_line = newline + 1;
    } else {
        for (next_line = newline; *next_line == '\r' || *next_line == '\n';
             next_line++)
            ; /* nothing */
        if (*(newline-1) == '\r') /* always skip CR */
            newline--;
        if (skip_ws) {
            for (; newline > line && (*(newline-1) == ' ' || *(newline-1) == '\t');
                 newline--)
                ; /* nothing */
        }
    }
    if (skip_ws) {
        for (; line < newline && (*line == ' ' || *line == '\t'); line++)
            ; /* nothing */
    }
    if (sol != NULL)
        *sol = line;
    if (eol != NULL)
        *eol = newline;
    return next_line;
}
