/* ******************************************************************************
 * Copyright (c) 2013-2016 Google, Inc.  All rights reserved.
 * Copyright (c) 2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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

#include "dr_api.h" /* for file_t, client_id_t */
#include <stdio.h>

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof((buf)[0]))
#define BUFFER_LAST_ELEMENT(buf) (buf)[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#    define IF_UNIX_ELSE(x, y) y
#else
#    define IF_WINDOWS(x)
#    define IF_UNIX_ELSE(x, y) x
#endif

#ifdef WINDOWS
#    define DISPLAY_STRING(msg) dr_messagebox("%s", msg)
#    define IF_WINDOWS(x) x
#else
#    define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#    define IF_WINDOWS(x) /* nothing */
#endif

/* open a log file
 * - id:        client id for getting the client library path
 * - drcontext: DR's context for per-thread logging, pass NULL if global logging
 * - path:      where the log file should be, pass NULL if using client library path
 * - name:      name of the log file
 * - flags:     file open mode, e.g., DR_FILE_WRITE_REQUIRE_NEW
 */
file_t
log_file_open(client_id_t id, void *drcontext, const char *path, const char *name,
              uint flags);

/* close a log file opened by log_file_open */
void
log_file_close(file_t log);

/* Converts a raw file descriptor into a FILE stream. */
FILE *
log_stream_from_file(file_t f);

/* log_file_close does *not* need to be called when calling this on a
 * stream converted from a file descriptor.
 */
void
log_stream_close(FILE *f);
