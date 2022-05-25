/* **********************************************************
 * Copyright (c) 2013-2022 Google, Inc.  All rights reserved.
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

#ifndef UNIX
#    error UNIX-only
#endif

#ifdef LINUX
#    include <elf.h>
#elif defined(MACOS)
#    include <mach-o/loader.h> /* mach_header */
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include "dr_frontend.h"
#include "drlibc.h"

extern bool
module_get_platform(file_t f, dr_platform_t *platform, dr_platform_t *alt_platform);

#define drfront_die() exit(1)

/* up to caller to call die() if necessary */
#define drfront_error(msg, ...)                             \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
    } while (0)

/* Semi-compatibility with the Windows CRT _access function.
 */
drfront_status_t
drfront_access(const char *fname, drfront_access_mode_t mode, OUT bool *ret)
{
    int r;
    struct stat64 st;
    uid_t euid;

    if (ret == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    /* Use the raw syscall to avoid glibc 2.33 deps (i#5474). */
    r = dr_stat_syscall(fname, &st);

    if (r < 0) {
        *ret = false;
        if (r == -EACCES || r == -ENOENT || r == -ENOTDIR)
            return DRFRONT_SUCCESS;
        return DRFRONT_ERROR;
    } else if (mode == DRFRONT_EXIST) {
        /* Just checking for existence */
        *ret = true;
        return DRFRONT_SUCCESS;
    }

    *ret = false;
    euid = geteuid();
    /* It is assumed that (S_IRWXU >> 6) == DRFRONT_READ | DRFRONT_WRITE | DRFRONT_EXEC */
    if (euid == 0) {
        /* XXX DrMi#1857: we assume that euid == 0 means +rw access to any file,
         * and +x access to any file with at least one +x bit set.  This is
         * usually true but not always.
         */
        if (TEST(DRFRONT_EXEC, mode)) {
            *ret = TESTANY((DRFRONT_EXEC << 6) | (DRFRONT_EXEC << 3) | DRFRONT_EXEC,
                           st.st_mode);
        } else
            *ret = true;
    } else if (euid == st.st_uid) {
        /* Check owner permissions */
        *ret = TESTALL(mode << 6, st.st_mode);
    } else {
        /* Check other permissions */
        *ret = TESTALL(mode, st.st_mode);
    }

    if (*ret && S_ISDIR(st.st_mode) && TEST(DRFRONT_WRITE, mode)) {
        /* We use an actual write try, to avoid failing on a read-only filesystem
         * (DrMi#1857).
         */
        return drfront_dir_try_writable(fname, ret);
    }

    return DRFRONT_SUCCESS;
}

/* Implements a normal path search for fname on the paths in env_var.
 * Resolves symlinks, which is needed to get the right config filename (i#1062).
 */
drfront_status_t
drfront_searchenv(const char *fname, const char *env_var, OUT char *full_path,
                  const size_t full_path_size, OUT bool *ret)
{
    const char *paths = getenv(env_var);
    const char *cur;
    const char *next;
    const char *end;
    char tmp[PATH_MAX];
    char realpath_buf[PATH_MAX]; /* realpath hardcodes its buffer length */
    drfront_status_t status_check = DRFRONT_ERROR;
    bool access_ret = false;
    int r;
    struct stat64 st;

    if (ret == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    /* Windows searches the current directory first. */
    /* XXX: realpath resolves symlinks, which we may not want. */
    if (realpath(fname, realpath_buf) != NULL) {
        status_check = drfront_access(realpath_buf, DRFRONT_EXIST, &access_ret);
        if (status_check != DRFRONT_SUCCESS) {
            *ret = false;
            return status_check;
        } else if (access_ret == true) {
            *ret = true;
            snprintf(full_path, full_path_size, "%s", realpath_buf);
            full_path[full_path_size - 1] = '\0';
            return DRFRONT_SUCCESS;
        }
    }

    cur = paths;
    end = strchr(paths, '\0');
    while (cur < end) {
        next = strchr(cur, ':');
        next = (next == NULL ? end : next);
        snprintf(tmp, BUFFER_SIZE_ELEMENTS(tmp), "%.*s/%s", (int)(next - cur), cur,
                 fname);
        NULL_TERMINATE_BUFFER(tmp);
        /* realpath checks for existence too. */
        if (realpath(tmp, realpath_buf) != NULL) {
            status_check = drfront_access(realpath_buf, DRFRONT_EXEC, &access_ret);
            if (status_check != DRFRONT_SUCCESS) {
                *ret = false;
                return status_check;
            } else if (access_ret == true) {
                // XXX: An other option to prevent calling stat() twice
                // could be a new variant DRFRONT_NOTDIR for drfront_access_mode_t
                // that drfront_access() then takes into account.
                /* Use the raw syscall to avoid glibc 2.33 deps (i#5474). */
                r = dr_stat_syscall(realpath_buf, &st);
                if (r == 0 && !S_ISDIR(st.st_mode)) {
                    *ret = true;
                    snprintf(full_path, full_path_size, "%s", realpath_buf);
                    full_path[full_path_size - 1] = '\0';
                    return DRFRONT_SUCCESS;
                }
            }
        }
        cur = next + 1;
    }
    full_path[0] = '\0';
    *ret = false;
    return DRFRONT_ERROR;
}

/* Always null-terminates.
 * No conversion on UNIX, just copy the data.
 */
drfront_status_t
drfront_tchar_to_char(const char *wstr, OUT char *buf, size_t buflen /*# elements*/)
{
    strncpy(buf, wstr, buflen);
    buf[buflen - 1] = '\0';
    return DRFRONT_SUCCESS;
}

/* includes the terminating null */
drfront_status_t
drfront_tchar_to_char_size_needed(const char *wstr, OUT size_t *needed)
{
    if (needed == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    *needed = strlen(wstr) + 1;
    return DRFRONT_SUCCESS;
}

/* Always null-terminates.
 * No conversion on UNIX, just copy the data.
 */
drfront_status_t
drfront_char_to_tchar(const char *str, OUT char *wbuf, size_t wbuflen /*# elements*/)
{
    strncpy(wbuf, str, wbuflen);
    wbuf[wbuflen - 1] = '\0';
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_is_64bit_app(const char *exe, OUT bool *is_64, OUT bool *also_32)
{
    FILE *target_file;
    drfront_status_t res = DRFRONT_ERROR;

    if (is_64 == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    target_file = fopen(exe, "rb");
    if (target_file != NULL) {
        dr_platform_t platform, alt_platform;
        if (module_get_platform(fileno(target_file), &platform, &alt_platform)) {
            res = DRFRONT_SUCCESS;
            /* XXX: on a 32-bit kernel we'll claim a 64+32 binary is *not* 64-bit:
             * is that ok?
             */
            *is_64 = (platform == DR_PLATFORM_64BIT);
            if (also_32 != NULL)
                *also_32 = (alt_platform == DR_PLATFORM_32BIT);
        }
        fclose(target_file);
    }
    return res;
}

/* This function is only relevant on Windows, so we return false. */
drfront_status_t
drfront_is_graphical_app(const char *exe, OUT bool *is_graphical)
{
    if (is_graphical == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    *is_graphical = false;
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_get_env_var(const char *name, OUT char *buf, size_t buflen /*# elements*/)
{
    const char *tmp_buf = getenv(name);
    size_t len_var = 0;

    if (tmp_buf == NULL) {
        return DRFRONT_ERROR_INVALID_PARAMETER;
    }
    len_var = strlen(tmp_buf);
    /* strlen doesn't include \0 in length */
    if (len_var + 1 > buflen) {
        return DRFRONT_ERROR_INVALID_SIZE;
    }
    strncpy(buf, tmp_buf, len_var + 1);
    buf[buflen - 1] = '\0';
    return DRFRONT_SUCCESS;
}

/* Simply concatenates the cwd with the given relative path.  Previously we
 * called realpath, but this requires the path to exist and expands symlinks,
 * which is inconsistent with Windows GetFullPathName().
 */
drfront_status_t
drfront_get_absolute_path(const char *rel, OUT char *buf, size_t buflen /*# elements*/)
{
    size_t len = 0;
    char *err = NULL;

    if (rel[0] != '/') {
        err = getcwd(buf, buflen);
        if (err != NULL) {
            len = strlen(buf);
            /* Append a slash if it doesn't have a trailing slash. */
            if (buf[len - 1] != '/' && len < buflen) {
                buf[len++] = '/';
                buf[len] = '\0';
            }
            /* Omit any leading "./". */
            if (rel[0] == '.' && rel[1] == '/') {
                rel += 2;
            }
        }
    }
    strncpy(buf + len, rel, buflen - len);
    buf[buflen - 1] = '\0';
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_get_app_full_path(const char *app, OUT char *buf, size_t buflen /*# elements*/)
{
    bool res = false;
    drfront_status_t status_check = DRFRONT_ERROR;

    status_check = drfront_searchenv(app, "PATH", buf, buflen, &res);
    if (status_check != DRFRONT_SUCCESS)
        return status_check;

    buf[buflen - 1] = '\0';
    if (buf[0] == '\0') {
        /* last try: expand w/ cur dir */
        status_check = drfront_get_absolute_path(app, buf, buflen);
        if (status_check != DRFRONT_SUCCESS)
            return status_check;
        buf[buflen - 1] = '\0';
    }
    return DRFRONT_SUCCESS;
}

drfront_status_t
drfront_dir_exists(const char *path, OUT bool *is_dir)
{
    struct stat64 st_buf;
    if (is_dir == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;

    /* check if path is a file or directory */
    /* Use the raw syscall to avoid glibc 2.33 deps (i#5474). */
    if (dr_stat_syscall(path, &st_buf) != 0) {
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

drfront_status_t
drfront_dir_try_writable(const char *path, OUT bool *is_writable)
{
    /* It would be convenient to use O_TMPFILE but not all filesystems support it */
    int fd;
    char tmpname[PATH_MAX];
    /* We actually don't care about races w/ other threads or processes running
     * this same code: each syscall should succeed and truncate whatever is there.
     */
#define TMP_FILE_NAME ".__drfrontendlib_tmp"
    if (is_writable == NULL)
        return DRFRONT_ERROR_INVALID_PARAMETER;
    snprintf(tmpname, BUFFER_SIZE_ELEMENTS(tmpname), "%s/%s", path, TMP_FILE_NAME);
    NULL_TERMINATE_BUFFER(tmpname);
    fd = creat(tmpname, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        drfront_status_t res;
        bool is_dir;
        *is_writable = false;
        res = drfront_dir_exists(path, &is_dir);
        if (res != DRFRONT_SUCCESS)
            return res;
        if (!is_dir)
            return DRFRONT_ERROR_INVALID_PATH;
    } else {
        *is_writable = true;
        close(fd);
        unlink(tmpname);
    }
    return DRFRONT_SUCCESS;
}
