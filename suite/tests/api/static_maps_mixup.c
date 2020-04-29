/* **********************************************************
 * Copyright (c) 2019 Google, Inc.  All rights reserved.
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

#define _GNU_SOURCE 1 /* for mremap */
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define LINUX
#include "dr_api.h"
#include "client_tools.h"

/* Avoid using tools.h because it causes weird compile errors in this file. */
#define print(...) dr_fprintf(STDERR, __VA_ARGS__)

/* XXX: share these helpers with suite/tests/tools.c, the core, and
 * burst_maps.cpp? */
#define MAPS_LINE_LENGTH 4096
#define MAPS_LINE_FORMAT4 "%08lx-%08lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_FORMAT8 "%016lx-%016lx %s %*x %*s %*u %4096s"

/* Note: For debugging purposes, find_exe_bounds will print out the maps
 * file as it is read if the macro PRINT_MAPS is defined.
 */
bool
find_exe_bounds(app_pc *base, app_pc *end)
{
    pid_t pid = getpid();
    char proc_pid_maps[64]; /* file name */
    FILE *maps;
    char line[MAPS_LINE_LENGTH];
    int len = snprintf(proc_pid_maps, BUFFER_SIZE_ELEMENTS(proc_pid_maps),
                       "/proc/%d/maps", pid);
    if (len < 0 || len == sizeof(proc_pid_maps))
        assert(0);
    NULL_TERMINATE_BUFFER(proc_pid_maps);
    maps = fopen(proc_pid_maps, "r");
    bool found = false;
    while (!feof(maps)) {
        char perm[16];
        char comment_buffer[MAPS_LINE_LENGTH];
        if (fgets(line, sizeof(line), maps) == NULL)
            break;
#ifdef PRINT_MAPS
        print(line);
#endif
        unsigned long b1, e1;
        len = sscanf(line, sizeof(void *) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
                     &b1, &e1, perm, comment_buffer);
        if (len < 4)
            comment_buffer[0] = '\0';
        if (!found && strstr(comment_buffer, "static_maps_mixup") != 0) {
            found = true;
            *(unsigned long *)base = b1;
            *(unsigned long *)end = e1;
        }
    }
    fclose(maps);
    return found;
}

// Confusing the current logic in get_dynamo_library_bounds is as simple as
// overwriting our first mapping with an anonymously-mapped version.
static void
copy_and_remap(void *base, size_t offs, size_t size)
{
    print("remap base=" PFX ", offs=%zu, sz=%zu\n", base, offs, size);
    void *p =
        mmap(0, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(p != MAP_FAILED);
    void *dst = (byte *)base + offs;
    memcpy(p, dst, size);
    int res = mprotect(p, size, PROT_EXEC | PROT_READ);
    assert(res == 0);
    void *loc = mremap(p, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, dst);
    assert(loc == dst);
}

int
main(int argc, const char *argv[])
{
    app_pc base = 0, end = 0;
    assert(find_exe_bounds(&base, &end));
    print("mix up maps\n");
    copy_and_remap(base, 0, end - base);
    find_exe_bounds(&base, &end);
    print("pre-DR init\n");
    dr_app_setup();
    print("pre-DR start\n");
    dr_app_start();
    print("pre-DR detach\n");
    dr_app_stop_and_cleanup();
    assert(!dr_app_running_under_dynamorio());

    print("all done\n");
    return 0;
}
