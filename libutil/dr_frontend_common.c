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
#include <string.h>
#include <errno.h>
#include "dr_frontend_private.h" /* for debuglevel/abortlevel */
#include "dr_frontend.h"

#ifdef WINDOWS
# include <direct.h>
/* It looks better to consistently use the same separator */
# define DIRSEP '\\'
# define snprintf _snprintf
#else
# include <unistd.h>
# define DIRSEP '/'
#endif
#include <sys/types.h>
#include <sys/stat.h> /* for _stat */

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

static bool
drfront_is_system_install_dir(const char *dir)
{
#ifdef WINDOWS
    return (strstr(dir, "Program Files") != NULL);
#else
    return (strstr(dir, "/usr/") == dir);
#endif
}

drfront_status_t
drfront_appdata_logdir(const char *root, const char *subdir,
                       OUT bool *use_root,
                       OUT char *buf, size_t buflen/*# elements*/)
{
    drfront_status_t res = DRFRONT_ERROR;
    bool writable = false;
    char env[MAXIMUM_PATH];
    if (use_root == NULL || buf == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    /* On Vista+ we can't write to Program Files; plus better to not store
     * logs there on 2K or XP either.
     */
    if (drfront_is_system_install_dir(root) ||
        drfront_access(root, DRFRONT_WRITE, &writable) != DRFRONT_SUCCESS ||
        !writable) {
        bool have_env = false;
        drfront_status_t sc;
        *use_root = false;
#ifdef WINDOWS
        sc = drfront_get_env_var("APPDATA", env, BUFFER_SIZE_ELEMENTS(env));
        if (sc == DRFRONT_SUCCESS)
            have_env = true;
        if (have_env) {
            snprintf(buf, buflen, "%s%c%s", env, DIRSEP, subdir);
            buf[buflen - 1] = '\0';
        } else {
            sc = drfront_get_env_var("USERPROFILE", env, BUFFER_SIZE_ELEMENTS(env));
            if (sc == DRFRONT_SUCCESS)
                have_env = true;
            if (have_env) {
                snprintf(buf, buflen,
                          "%s%cApplication Data%c%s", env, DIRSEP, DIRSEP, subdir);
                buf[buflen - 1] = '\0';
            }
        }
#endif
        if (!have_env) {
            sc = drfront_get_env_var("TMPDIR", env, BUFFER_SIZE_ELEMENTS(env));
            if (sc != DRFRONT_SUCCESS)
                sc = drfront_get_env_var("TEMP", env, BUFFER_SIZE_ELEMENTS(env));
            if (sc != DRFRONT_SUCCESS)
                sc = drfront_get_env_var("TMP", env, BUFFER_SIZE_ELEMENTS(env));
            if (sc != DRFRONT_SUCCESS) {
#ifdef WINDOWS
                /* bail */
#else
                have_env = true;
                snprintf(env, BUFFER_SIZE_ELEMENTS(env), "%s", "/tmp");
                NULL_TERMINATE_BUFFER(env);
#endif
            } else
                have_env = true;
            if (have_env) {
                snprintf(buf, buflen, "%s%c%s", env, DIRSEP, subdir);
                buf[buflen - 1] = '\0';
            }
        }
        if (have_env) {
            /* XXX: I would create the dir, or check for existence, here --
             * but currently this lib has no reliance on DR so I can't
             * easily use dr_create_dir() or dr_directory_exists().
             */
            res = DRFRONT_SUCCESS;
        }
    } else {
        *use_root = true;
        res = DRFRONT_SUCCESS;
    }
    return res;
}

void
drfront_string_replace_character(OUT char *str, char old_char, char new_char)
{
    while (*str != '\0') {
        if (*str == old_char) {
            *str = new_char;
        }
        str++;
    }
}

void
drfront_string_replace_character_wide(OUT TCHAR *str, TCHAR old_char, TCHAR new_char)
{
    while (*str != _T('\0')) {
        if (*str == old_char) {
            *str = new_char;
        }
        str++;
    }
}

/* XXX: Since we can't simply use dr_* routines, we implement create/remove/exists
 * directory here as a short-term solution (xref i#1409).
 */
drfront_status_t
drfront_create_dir(const char *dir)
{
    uint res;
    if (dir == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    /* FIXME i#1530: we should not use libc on Windows,
     * which breaks the internationalization support
     */
#ifdef WINDOWS
    res = _mkdir(dir);
#else
    res = mkdir(dir, 0777);
#endif
    if (res != 0) {
        if(errno == EEXIST) {
            return DRFRONT_ERROR_FILE_EXISTS;
        } else if(errno == EACCES) {
            return DRFRONT_ERROR_ACCESS_DENIED;
        } else {
            DO_DEBUG(DL_WARN,
                     printf("mkdir failed %d", errno);
                     );
            return DRFRONT_ERROR;
        }
    }
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_remove_dir(const char *dir)
{
    uint res;
    if (dir == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    /* FIXME i#1530: we should not use libc on Windows,
     * which breaks the internationalization support
     */
#ifdef WINDOWS
    res = _rmdir(dir);
#else
    res = rmdir(dir);
#endif
    if (res != 0) {
        if(errno == ENOENT) {
            return DRFRONT_ERROR_INVALID_PATH;
        } else if(errno == EACCES) {
            return DRFRONT_ERROR_ACCESS_DENIED;
        } else {
            DO_DEBUG(DL_WARN,
                     printf("rmdir failed %d", errno);
                     );
            return DRFRONT_ERROR;
        }
    }
    return DRFRONT_SUCCESS;
}

#ifdef WINDOWS
# define S_ISDIR(mode) TEST(mode, _S_IFDIR)
# define stat _stat
#endif

drfront_status_t
drfront_dir_exists(const char *path, bool *is_dir)
{
    struct stat st_buf;
    if (is_dir == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    /* FIXME i#1530: we should not use libc on Windows,
     * which breaks the internationalization support
     */
    /* check if path is a file or directory */
    if (stat(path, &st_buf) != 0) {
        *is_dir = false;
        return DRFRONT_ERROR_INVALID_PATH;
    } else {
        if (S_ISDIR(st_buf.st_mode))
            *is_dir = true;
        else
            *is_dir = false;
    }
    return DRFRONT_SUCCESS;
}
