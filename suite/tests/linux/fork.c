/* **********************************************************
 * Copyright (c) 2014-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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

/*
 * test of fork
 */

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h> /* for wait and mmap */
#include <sys/wait.h>  /* for wait */
#include <assert.h>
#include <stdio.h>
#include <string.h>

/***************************************************************************/
/* a hopefuly portable /proc/@self/maps reader */

/* these are defined in /usr/src/linux/fs/proc/array.c */
#define MAPS_LINE_LENGTH 4096
/* for systems with sizeof(void*) == 4: */
#define MAPS_LINE_FORMAT4 "%08lx-%08lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX4 49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
/* for systems with sizeof(void*) == 8: */
#define MAPS_LINE_FORMAT8 "%016lx-%016lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX8 73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */

#define MAPS_LINE_MAX MAPS_LINE_MAX8

int
find_dynamo_library()
{
    pid_t pid = getpid();
    char proc_pid_maps[64]; /* file name */

    FILE *maps;
    char line[MAPS_LINE_LENGTH];
    int count = 0;

    // open file's /proc/id/maps virtual map description
    int n = snprintf(proc_pid_maps, sizeof(proc_pid_maps), "/proc/%d/maps", pid);
    if (n < 0 || n == sizeof(proc_pid_maps))
        assert(0); /* paranoia */

    maps = fopen(proc_pid_maps, "r");

    while (!feof(maps)) {
        void *vm_start, *vm_end;
        char perm[16];
        char comment_buffer[MAPS_LINE_LENGTH];
        int len;

        if (NULL == fgets(line, sizeof(line), maps))
            break;
        len = sscanf(line, sizeof(void *) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
                     (unsigned long *)&vm_start, (unsigned long *)&vm_end, perm,
                     comment_buffer);
        if (len < 4)
            comment_buffer[0] = '\0';
        if (strstr(comment_buffer, "dynamorio") != 0) {
            fclose(maps);
            return 1;
        }
    }

    fclose(maps);
    return 0;
}

/***************************************************************************/

int
main(int argc, char **argv)
{
    pid_t child;

    if (find_dynamo_library())
        printf("parent is running under DynamoRIO\n");
    else
        printf("parent is running natively\n");
    child = fork();
    if (child < 0) {
        perror("ERROR on fork");
    } else if (child > 0) {
        pid_t result;
        printf("parent waiting for child\n");
        result = waitpid(child, NULL, 0);
        assert(result == child);
        printf("child has exited\n");
    } else {
        if (find_dynamo_library())
            printf("child is running under DynamoRIO\n");
        else
            printf("child is running natively\n");
    }
}
