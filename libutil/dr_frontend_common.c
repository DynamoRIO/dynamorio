/* **********************************************************
 * Copyright (c) 2013-2014 Google, Inc.  All rights reserved.
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

#include "share.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "dr_frontend.h"

drfront_status_t
drfront_bufprint(char *buf, size_t bufsz, INOUT size_t *sofar, OUT ssize_t *len,
                 char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* XXX i#1397: We would like to use our_vsnprintf() instead of depending
     * on libc/winapi.
     */
    *len = vsnprintf(buf + *sofar, bufsz - *sofar, fmt, ap);
    *sofar += (*len < 0 ? 0 : *len);
    if (bufsz <= *sofar) {
        va_end(ap);
        return DRFRONT_ERROR_INVALID_SIZE;
    }
    /* be paranoid: though usually many calls in a row and could delay until end */
    buf[bufsz - 1] = '\0';
    va_end(ap);
    return DRFRONT_SUCCESS;
}

/* Always null-terminates.
 * No conversion on UNIX, just copy the data.
 */
drfront_status_t
drfront_convert_args(const TCHAR **targv, OUT char ***argv, int argc)
{
    int i = 0;
    drfront_status_t ret = DRFRONT_ERROR;
    *argv = (char **) calloc(argc + 1/*null*/, sizeof(**argv));
    for (i = 0; i < argc; i++) {
        size_t len = 0;
        ret = drfront_tchar_to_char_size_needed(targv[i], &len);
        if (ret != DRFRONT_SUCCESS)
            return ret;
        (*argv)[i] = (char *) malloc(len); /* len includes terminating null */
        ret = drfront_tchar_to_char(targv[i], (*argv)[i], len);
        if (ret != DRFRONT_SUCCESS)
            return ret;
    }
    (*argv)[i] = NULL;
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_cleanup_args(char **argv, int argc)
{
    int i = 0;
    for (i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
    return DRFRONT_SUCCESS;
}
