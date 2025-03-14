/* **********************************************************
 * Copyright (c) 2016-2018 Google, Inc.  All rights reserved.
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

#include "dr_api.h"
#include "drsyscall.h"
#include "drsyscall_os.h"
#include "drsyscall_windows.h"

static const char *
double_strchr(const char *s, char c1, char c2)
{
    while (*s != '\0') {
        if (*s == c1 || *s == c2) {
            return s;
        }
        ++s;
    }
    return NULL;
}

/* i#1908: we support loading numbers from a file.  The file format is documented
 * in drsyscall.h.
 * We support carriage returns and a missing trailing newline, which are common
 * when saving from our URL by pasting into notepad.
 */
bool
read_sysnum_file(void *drcontext, const char *sysnum_file, module_data_t *ntdll_data)
{
    drmf_status_t status = DRMF_ERROR_INVALID_PARAMETER;
    file_t f;
    void *map = NULL;
    bool ok;
    uint64 map_size;
    size_t actual_size;
    const char *line, *search;
    int val;
    char name[MAXIMUM_PATH];
    drsys_sysnum_t num_from_wrapper;

    f = dr_open_file(sysnum_file, DR_FILE_READ);
    if (f == INVALID_FILE) {
        LOG(SYSCALL_VERBOSE, "syscall file %s not found\n", sysnum_file);
        goto read_sysnum_file_done;
    }
    LOG(SYSCALL_VERBOSE, "processing syscall file %s\n", sysnum_file);
    ok = dr_file_size(f, &map_size);
    if (ok) {
        actual_size = (size_t)map_size;
        ASSERT(actual_size == map_size, "file size too large");
        map = dr_map_file(f, &actual_size, 0, NULL, DR_MEMPROT_READ, 0);
    }
    if (!ok || map == NULL || actual_size < map_size)
        goto read_sysnum_file_done;
    if (strncmp((char *)map, DRSYS_SYSNUM_FILE_HEADER,
                strlen(DRSYS_SYSNUM_FILE_HEADER)) != 0)
        goto read_sysnum_file_done;
    /* ntdll's sscanf is unsafe for mmap (i#1057) */
    line = (char *)map + strlen(DRSYS_SYSNUM_FILE_HEADER) + 1 /* skip \n */;
    if (*line == '\n') /* if there was \r */
        line++;
    if (dr_sscanf(line, "%d", &val) != 1 ||
        /* neither forward nor backward compatible */
        val != DRSYS_SYSNUM_FILE_VERSION) {
        status = DRMF_ERROR_INCOMPATIBLE_VERSION;
        goto read_sysnum_file_done;
    }
    /* XXX: what we want is strnchr to which we can pass a size to avoid going
     * off the end of the map.  That should only happen for an ill-formed file
     * so we live with the chance of a crash for now.
     */
    line = strchr(line, '\n');
    if (line == NULL || line - map > actual_size)
        goto read_sysnum_file_done;
    line++;
    search = double_strchr(line, '\r', '\n');
    if (search == NULL || search - map > actual_size ||
        search - line >= BUFFER_SIZE_ELEMENTS(name))
        goto read_sysnum_file_done;
    strncpy(name, line, search - line);
    name[search - line] = '\0';
    LOG(SYSCALL_VERBOSE, "syscall file: index name is %s\n", name);

    ok = syscall_num_from_name(drcontext, ntdll_data, name, NULL, false/*exported*/,
                               &num_from_wrapper);
    if (!ok)
        goto read_sysnum_file_done;
    LOG(SYSCALL_VERBOSE, "syscall file: index num is 0x%x\n", num_from_wrapper.number);

    /* Originally my design was to store the offset for each list at the top,
     * to avoid this scan, but once it was clear that it is not simple to
     * download a text file on Windows 10 and it is likely to end up copy-pasted
     * with CRLF and other changes, relying on offsets is too fragile.
     */
    do {
        search = strstr(line, "\nSTART=0x");
        LOG(SYSCALL_VERBOSE, "syscall file: examining %.16s\n",
            search == NULL ? "<null>" : search+1);
        if (search == NULL || search - map > actual_size)
            goto read_sysnum_file_done;
        if (dr_sscanf(search+1, "START=0x%x", &val) != 1)
            goto read_sysnum_file_done;
        if (num_from_wrapper.number == val) {
            LOG(SYSCALL_VERBOSE, "syscall file: found target list\n");
            break;
        }
        line = strchr(search+1, '\n');
        if (line == NULL || line - map > actual_size)
            goto read_sysnum_file_done;
    } while (search - map < actual_size);

    line = strchr(search+1, '\n');
    if (line == NULL || line - map > actual_size)
        goto read_sysnum_file_done;
    line++;

    while (strncmp(line, DRSYS_SYSNUM_FILE_FOOTER,
                   strlen(DRSYS_SYSNUM_FILE_FOOTER)) != 0) {
        search = strchr(line, '=');
        if (search == NULL || search - map > actual_size ||
            search - line >= BUFFER_SIZE_ELEMENTS(name))
            goto read_sysnum_file_done;
        strncpy(name, line, search - line);
        name[search - line] = '\0';
        /* Sanity check in case offs is wrong */
        if (name[0] < 'A' || name[0] > 'Z')
            goto read_sysnum_file_done;
        line = search + 1;
        if (dr_sscanf(line, "0x%x", &val) != 1)
            goto read_sysnum_file_done;
        name2num_record(name, val, true/*dup the string*/);
        line = strchr(line, '\n');
        /* Handle a missing trailing newline */
        if (line == NULL && strncmp(line, DRSYS_SYSNUM_FILE_FOOTER,
                                    strlen(DRSYS_SYSNUM_FILE_FOOTER)) == 0)
            break;
        if (line == NULL || line - map > actual_size)
            goto read_sysnum_file_done;
        line++;
    }

    status = DRMF_SUCCESS;
 read_sysnum_file_done:
    DOLOG(SYSCALL_VERBOSE, {
        if (status != DRMF_SUCCESS)
            LOG(SYSCALL_VERBOSE, "failed to parse syscall file %s\n", sysnum_file);
    });
    if (map != NULL)
        dr_unmap_file(map, actual_size);
    if (f != INVALID_FILE)
        dr_close_file(f);
    return status;
}
