/* **********************************************************
 * Copyright (c) 2007 VMware, Inc.  All rights reserved.
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

/* DRdel - clean ASLR or persistent cache files and directories */

/* most of the functionality used in nodemgr should be in share/ while
 * this tool may be a good place for experimentation and test
 * scaffolding
 */
#include <windows.h>
#include <assert.h>
#include <stdio.h>

#include "share.h"

#define DEBUG

#ifdef X64
enum { LAST_STATUS_VALUE_OFFSET = 0x1250 };
#else
enum { LAST_STATUS_VALUE_OFFSET = 0xbf4 }; /* on Win2000+ case 6789 */
#endif

int
get_last_status()
{
    /* get_own_teb()->LastStatusValue */
#ifdef X64
    return __readgsdword(LAST_STATUS_VALUE_OFFSET);
#else
    return __readfsdword(LAST_STATUS_VALUE_OFFSET);
#endif
}

#ifdef DEBUG
int verbose = 0;

void
print_status(bool ok)
{
    printf("%s 0x%08x %s %d\n", ok ? "" : "NTSTATUS", ok ? 0 : get_last_status(),
           ok ? "success:" : "GLE", GetLastError());
}
#endif

int
usage(char *us)
{
    fprintf(stderr, "\
Usage: %s\n\
drdel -f <file> -d <directory> -r \n\
    -ms <size> target min size available\n\
    -us <size> target max size used\n\
    -f <file>  work on one file only\n\
    -d <directory>  work on a specified directory\n\
    -r use cache directories from registry\n\
\n\
    -c         check if in use and skip\n\
    -m         mark for deletion when closed\n\
    -t         .tmp renaming\n\
    -o         on close delete\n\
    -b         delete on next boot\n\
\n\
    -v         verbose\n\
\n",
            us);
    return 0;
}

/* FIXME: should move this to share/utils.c */
/* and merge/reuse or at least cf. with  */
/* file_exists() */
/* get_unique_filename() */
/* delete_file_rename_in_use() */
/* copy_file_permissions()
 * delete_tree()
 */

DWORD
is_file_in_use(WCHAR *filename)
{
    HANDLE hFile;

    hFile = CreateFile(filename,      // file to open
                       GENERIC_READ,  // open for reading
                       0,             // EXCLUSIVE access, should fail if in use
                                      // note will fail anyone trying to use at this time
                       NULL,          // default security
                       OPEN_EXISTING, // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template

    /* FIXME: is there a better way to check if a file is currently in
     * use that doesn't get in the way of others.  Admittedly very
     * short race.  Can we count on the handle properties, e.g. if
     * QueryObject SYSTEM_OBJECT_INFORMATION.HandleCount and
     * PointerCount are one number vs another.
     */

    /*
    0:000> !handle 10 f
    Handle 10
      Type                 File
      Attributes           0
      GrantedAccess        0x120089:
             ReadControl,Synch
             Read/List,ReadEA,ReadAttr
      HandleCount          2
      PointerCount         3
      No Object Specific Information available
    */

    if (verbose) {
        print_status(hFile != INVALID_HANDLE_VALUE);
    }

    if (hFile == INVALID_HANDLE_VALUE)
        return true; /* in use */
    CloseHandle(hFile);
    return false;
}

/* see in utils.c file_exists() that one cannot
 * open the root directory (and in fact "\\remote\share" as well)
 */
bool
is_file_present(WCHAR *filename)
{
    HANDLE hFile;

    hFile = CreateFile(filename,              // file to open
                       0,                     // just existence check
                       FILE_SHARE_READ,       // share for reading FIXME: do we need this
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template
    if (verbose) {
        print_status(hFile != INVALID_HANDLE_VALUE);
    }

    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(hFile);
    return true;
}

bool
delete_file_on_close(WCHAR *filename)
{
    HANDLE hFile;

    hFile = CreateFile(
        filename,                            // file to open
        0,                                   // just existence check
        FILE_SHARE_READ | FILE_SHARE_DELETE, // share for reading FIXME: do we need this
        NULL,                                // default security
        OPEN_EXISTING,                       // existing file only
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, // normal file, delete on close
        NULL);                                             // no attr. template
    if (verbose) {
        print_status(hFile != INVALID_HANDLE_VALUE);
    }

    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(hFile);
    return true;
}

DWORD
delete_file(WCHAR *filename)
{
    BOOL success = DeleteFile(filename);
    if (verbose) {
        print_status(success);
    }

    if (success)
        return ERROR_SUCCESS;
    else {
        return GetLastError();
        /*
         * For memory mapped files - e.g. after an NtCreateSection() we get
         * (NTSTATUS) 0xc0000121 (3221225761) - An attempt has been made to
         * remove a file or directory that cannot be deleted.
         */
    }
}

int check_in_use = 0;
int delete = 0;
int temprename = 0;
int onboot = 0;
int onclose = 0;

/*
 * Possible states of a file
 * {existing, not existing} x {in use, or not} x {DELETED, or not}
 *        x {.used, or not} x {pending reboot removal, or not}
 *
reboot removal - MoveFileEx(MOVEFILE_DELAY_UNTIL_REBOOT) adds to
PendingFileRenameOperations HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session
Manager\PendingFileRenameOperations

 * - not existing
 * - existing, hard link x {in use, not in use}
 * - existing, sym link x {in use, not in use} (Longhorn)
 * - existing, not in use
 * - existing, not in use, previously renamed to .used
 * - existing, in use
 * - existing, DELETED, still in use
 * - existing, in use, renamed to .used
 * - existing, in use, DELETED, renamed to .used
 * - existing, in use, DELETED, listed for reboot removal
 * - existing, in use, DELETED, listed for reboot removal, renamed to .used
 */

int
process_file(WCHAR *filename)
{
    int in_use = 0;
    int deleted = 0;

    if (verbose) {
        printf("processing %S\n", filename);

        if (is_file_present(filename)) {
            if (is_file_in_use(filename)) {
                printf("file %ls is in use\n", filename);
            } else {
                printf("file %ls exists and is not in use\n", filename);
            }
        } else {
            printf("file %ls doesn't exists\n", filename);
        }
    }

    if (check_in_use) {
        in_use = is_file_in_use(filename);
        if (verbose && in_use) {
            printf("file %ls is in use\n", filename);
        }
        return 1;
    }

    if (delete) {
        deleted = delete_file(filename) == ERROR_SUCCESS;
    }

    if (onboot) {
        delete_file_on_boot(filename);
    }

    if (onclose) {
        delete_file_on_close(filename);
    }

    if (!deleted) {
        if (temprename) {
            delete_file_rename_in_use(filename);
            /* note this will also put in PendingFileRenameOperations */
        }
    }

    return 0;
}

int
process_directory(WCHAR *dirname)
{

    if (verbose) {
        printf("delete_tree %S\n", dirname);
    }

    delete_tree(dirname);
    return 0;
}

int
main(int argc, char *argv[])
{
    LPVOID *pv = 0;
    LPVOID *pc = 0;

    int argidx = 1;

    int file = 0;
    int dir = 0;
    WCHAR filebuf[MAX_PATH];
    WCHAR dirbuf[MAX_PATH];

    if (argc < 2)
        usage(argv[0]);

    while (argidx < argc) {
        if (!strcmp(argv[argidx], "-help")) {
            usage(argv[0]);
            exit(0);
        } else if (!strcmp(argv[argidx], "-f")) {
            _snwprintf(filebuf, MAX_PATH, L"%S", argv[++argidx]);
            file = 1;
        } else if (!strcmp(argv[argidx], "-d")) {
            _snwprintf(dirbuf, MAX_PATH, L"%S", argv[++argidx]);
            dir = 1;
        } else if (!strcmp(argv[argidx], "-m")) {
            delete = 1;
        } else if (!strcmp(argv[argidx], "-o")) {
            onclose = 1;
        } else if (!strcmp(argv[argidx], "-b")) {
            onboot = 1;
        } else if (!strcmp(argv[argidx], "-t")) {
            temprename = 1;
        } else if (!strcmp(argv[argidx], "-c")) {
            check_in_use = 1;
        } else if (!strcmp(argv[argidx], "-v")) {
            verbose = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage(argv[0]);
            exit(0);
        }
        argidx++;
    }

    if (file) {
        process_file(filebuf);
    }
    if (dir) {
        process_directory(dirbuf);
    }
}
