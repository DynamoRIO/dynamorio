/* *******************************************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
 * *******************************************************************************/

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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * fnmatch.c - Implementation of the fnmatch function.
 */

#include "../globals.h"

#include <string.h>

static const char *
rangematch(const char *pattern, char input, int flags)
{
    char c, end;
    int invert = 0;
    int match = 0;

    /* '!' or '^' following the '[' means the matching should be inverted. */
    if (*pattern == '!' || *pattern == '^') {
        invert = 1;
        ++pattern;
    }

    while ((c = *pattern++)) {
        /* Handle character escaping. */
        if (c == '\\' && !(flags & FNM_NOESCAPE)) {
            c = *pattern++;
        }

        /* Check for null termination. */
        if (c == '\0') {
            return NULL;
        }

        /* Match against the character range. */
        if (*pattern == '-' && (end = (*pattern + 1)) && end != ']') {
            pattern += 2;

            /* Handle character escaping. */
            if (end == '\\' && !(flags & FNM_NOESCAPE)) {
                end = *pattern++;
            }

            /* Check for null termination. */
            if (end == '\0') {
                return NULL;
            }

            /* Check whether the input is within the character range. */
            if (c <= input && input < end) {
                match = 1;
            }
        } else if (c == input) {
            match = 1;
        }
    }

    if (invert) {
        match = !match;
    }

    return match ? pattern : NULL;
}

int
d_r_fnmatch(const char *pattern, const char *string, int flags)
{
    const char *start = string;
    char c, input;

    while ((c = *pattern++)) {
        switch (c) {
        case '?':
            if (*string == '\0') {
                return FNM_NOMATCH;
            }

            if (*string == '/' && (flags & FNM_PATHNAME)) {
                return FNM_NOMATCH;
            }

            if (*string == '.' && (flags & FNM_PERIOD) &&
                (string == start || ((flags & FNM_PATHNAME) && *(string - 1) == '/'))) {
                return FNM_NOMATCH;
            }

            ++string;
            break;
        case '*':
            /* Collapse a sequence of stars. */
            while (c == '*') {
                c = *++pattern;
            }

            if (*string == '.' && (flags & FNM_PERIOD) &&
                (string == start || ((flags & FNM_PATHNAME) && *(string - 1) == '/'))) {
                return FNM_NOMATCH;
            }

            /* Handle pattern with * at the end. */
            if (c == '\0') {
                if (flags & FNM_PATHNAME) {
                    return strchr(string, '/') ? FNM_NOMATCH : 0;
                }

                return 0;
            }

            /* Handle pattern with * before / while handling paths. */
            if (c == '/' && flags & FNM_PATHNAME) {
                if ((string = strchr(string, '/'))) {
                    return FNM_NOMATCH;
                }

                break;
            }

            /* Handle any other variant through recursion. */
            while ((input = *string)) {
                if (fnmatch(pattern, string, flags & ~FNM_PERIOD) == 0) {
                    return 0;
                }

                if (input == '/' && flags & FNM_PATHNAME) {
                    break;
                }

                ++string;
            }

            return FNM_NOMATCH;
        case '[':
            if (*string == '\0') {
                return FNM_NOMATCH;
            }

            if (*string == '/' && flags & FNM_PATHNAME) {
                return FNM_NOMATCH;
            }

            if (!(pattern = rangematch(pattern, *string, flags))) {
                return FNM_NOMATCH;
            }

            ++string;
            break;
        case '\\':
            if (!(flags & FNM_NOESCAPE)) {
                if ((c = *pattern++)) {
                    c = '\\';
                    --pattern;
                }
            }

            /* fallthrough */
        default:
            if (c != *string++) {
                return FNM_NOMATCH;
            }

            break;
        }
    }

    if (*string != '\0') {
        return FNM_NOMATCH;
    }

    return 0;
}
