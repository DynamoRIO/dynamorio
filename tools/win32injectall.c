/* **********************************************************
 * Copyright (c) 2001-2003 VMware, Inc.  All rights reserved.
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

/* build like this:
 *   cl win32injectall.c advapi32.lib
 */

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* This routine assumes an environment variable named
 * DYNAMO_AUTO_INJECT is set and that it points to something like:
 * c:\iye\rio\inject\win32dynamo.dll
 */

#define INJECT_ALL_HIVE HKEY_LOCAL_MACHINE
#define INJECT_ALL_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"
#define INJECT_ALL_SUBKEY "AppInit_DLLs"

void
print_error(int error)
{
    LPVOID lpMsgBuf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR)&lpMsgBuf, 0, NULL);
    // Display the string.
    fprintf(stderr, "%s\n", (LPCTSTR)lpMsgBuf);
    // Free the buffer.
    LocalFree(lpMsgBuf);
}

int
main(int argc, char *argv[])
{
    int res;
    HKEY hk;
    char *val;
    char data[1024];
    unsigned long size = 0;

    if (argc != 2) {
        printf("Usage: %s set|unset|view\n", argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "view") == 0) {
        res = RegOpenKeyEx(INJECT_ALL_HIVE, INJECT_ALL_KEY, 0, KEY_READ, &hk);
        assert(res == ERROR_SUCCESS);
        res = RegQueryValueEx(hk, INJECT_ALL_SUBKEY, 0, NULL, (LPBYTE)data, &size);
        /* for some reason we get error code "More data is available" here
         * (doing exact same thing in win32gui always gets ERROR_SUCCESS)
         */
        /* assert(res == ERROR_SUCCESS); */
        RegCloseKey(hk);
        // assumption: only we use this key, so if it is non-empty it must be us!
        if (size > 1)
            printf("Inject all is set to %s\n", data);
        else
            printf("Inject all is NOT set\n");
    } else {
        if (strcmp(argv[1], "set") == 0) {
            const char *subdir = "\\bin\\drpreinject.dll";
            size = GetEnvironmentVariable((LPCTSTR) "DYNAMORIO_HOME", data,
                                          1023 - strlen(subdir));
            strcpy(data + strlen(data), subdir);
            val = data;
            printf("Setting key to %s\n", val);
        } else if (strcmp(argv[1], "unset") == 0) {
            val = "";
        } else {
            printf("Usage: %s set|unset|view\n", argv[0]);
            return 0;
        }
        res = RegOpenKeyEx(INJECT_ALL_HIVE, INJECT_ALL_KEY, 0, KEY_WRITE, &hk);
        assert(res == ERROR_SUCCESS);
        res =
            RegSetValueEx(hk, INJECT_ALL_SUBKEY, 0, REG_SZ, (LPBYTE)val, strlen(val) + 1);
        assert(res == ERROR_SUCCESS);
        RegCloseKey(hk);
    }

    return 0;
}
