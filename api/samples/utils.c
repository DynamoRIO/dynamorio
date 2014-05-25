/* ******************************************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

#include "utils.h"

file_t
log_file_open(client_id_t id, void *drcontext,
              const char *path, const char *name, uint flags)
{
    file_t log;
    char logname[MAXIMUM_PATH];
    size_t len;
    char *dirsep;

    DR_ASSERT(name != NULL);
    len = dr_snprintf(logname, BUFFER_SIZE_ELEMENTS(logname), "%s",
                      path == NULL ? dr_get_client_path(id) : path, name);
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(logname);
    dirsep = logname + len - 1;
    if (path == NULL /* removing client lib */ ||
        /* path does not have a trailing / and is too large to add it */
        (*dirsep != '/' IF_WINDOWS(&& *dirsep != '\\') &&
         len == BUFFER_SIZE_ELEMENTS(logname) - 1)) {
        for (dirsep = logname + len;
             *dirsep != '/' IF_WINDOWS(&& *dirsep != '\\');
             dirsep--)
            DR_ASSERT(dirsep > logname);
    }
    /* add trailing / if necessary */
    if (*dirsep != '/' IF_WINDOWS(&& *dirsep != '\\')) {
        dirsep++;
        /* append a dirsep at the end if missing */
        *dirsep = IF_UNIX_ELSE('/', '\\');
    }
    len = dr_snprintf(dirsep + 1,
                      (sizeof(logname)-(dirsep+1-logname))/sizeof(logname[0]),
                      name);
    DR_ASSERT(len > 0);
    NULL_TERMINATE_BUFFER(logname);
    log = dr_open_file(logname, flags);
    if (log != INVALID_FILE)
        dr_log(drcontext, LOG_ALL, 1, "log file %s\n", logname);
    return log;
}

void
log_file_close(file_t log)
{
    dr_close_file(log);
}
