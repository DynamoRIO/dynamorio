/* **********************************************************
 * Copyright (c) 2017-2023 Google, Inc.  All rights reserved.
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

/* This application links in drmemtrace_static and acquires a trace during
 * a "burst" of execution in the middle of the application.  Before attaching
 * it allocates a lot of heap, preventing the statically linked client from
 * being 32-bit reachable from any available space for the code cache.
 */

/* Like burst_static we deliberately do not include configure.h here. */
#include "dr_api.h"
#include "../../common/utils.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#define _GNU_SOURCE 1 /* for mremap */
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

namespace dynamorio {
namespace drmemtrace {

/* XXX: share these with suite/tests/tools.c and the core? */
#define MAPS_LINE_LENGTH 4096
#define MAPS_LINE_FORMAT4 "%08lx-%08lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX4 49 /* sum of 8  1  8  1 4 1 8 1 5 1 10 1 */
#define MAPS_LINE_FORMAT8 "%016lx-%016lx %s %*x %*s %*u %4096s"
#define MAPS_LINE_MAX8 73 /* sum of 16  1  16  1 4 1 16 1 5 1 10 1 */
#define MAPS_LINE_MAX MAPS_LINE_MAX8

char exe_path[MAPS_LINE_LENGTH];

void *
find_exe_base()
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
    while (!feof(maps)) {
        void *vm_start, *vm_end;
        char perm[16];
        char comment_buffer[MAPS_LINE_LENGTH];
        if (fgets(line, sizeof(line), maps) == NULL)
            break;
        len = sscanf(line, sizeof(void *) == 4 ? MAPS_LINE_FORMAT4 : MAPS_LINE_FORMAT8,
                     (unsigned long *)&vm_start, (unsigned long *)&vm_end, perm,
                     comment_buffer);
        if (len < 4)
            comment_buffer[0] = '\0';
        if (strstr(comment_buffer, "burst_maps") != 0) {
            strncpy(exe_path, comment_buffer, BUFFER_SIZE_ELEMENTS(exe_path));
            NULL_TERMINATE_BUFFER(exe_path);
            fclose(maps);
            return vm_start;
        }
    }
    fclose(maps);
    return NULL;
}

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static int
do_some_work(int arg)
{
    static int iters = 512;
    double val = (double)arg;
    for (int i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

static void
copy_and_remap(void *base, size_t offs, size_t size)
{
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

static void
clobber_mapping()
{
    /* Test placing anonymous regions in the exe mapping (i#2566) */
    const size_t clobber_size = 4096;
    void *exe = find_exe_base();
    assert(exe != NULL);
    copy_and_remap(exe, 0, clobber_size);
    copy_and_remap(exe, 4 * clobber_size, clobber_size);
    copy_and_remap(exe, 8 * clobber_size, clobber_size);
}

extern "C" {
extern DR_EXPORT void
drmemtrace_client_main(client_id_t id, int argc, const char *argv[]);

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    /* Test the full_path used by DR when the maps file comments can't be used. */
    module_data_t *exe = dr_get_main_module();
    assert(exe != nullptr);
    assert(exe->full_path != nullptr);
    assert(strcmp(exe->full_path, exe_path) == 0);
    dr_free_module_data(exe);
    drmemtrace_client_main(id, argc, argv);
}
}

int
test_main(int argc, const char *argv[])
{
    static int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static int iter_start = outer_iters / 3;
    static int iter_stop = iter_start + 4;

    clobber_mapping();

    if (!my_setenv("DYNAMORIO_OPTIONS",
                   "-stderr_mask 0xc -vm_size 512M "
                   "-client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    /* We use an outer loop to test re-attaching (i#2157). */
    for (int j = 0; j < 3; ++j) {
        std::cerr << "pre-DR init\n";
        dr_app_setup();
        assert(!dr_app_running_under_dynamorio());

        for (int i = 0; i < outer_iters; ++i) {
            if (i == iter_start) {
                std::cerr << "pre-DR start\n";
                dr_app_start();
            }
            if (i >= iter_start && i <= iter_stop)
                assert(dr_app_running_under_dynamorio());
            else
                assert(!dr_app_running_under_dynamorio());
            if (do_some_work(i) < 0)
                std::cerr << "error in computation\n";
            if (i == iter_stop) {
                std::cerr << "pre-DR detach\n";
                dr_app_stop_and_cleanup();
            }
        }
        std::cerr << "all done\n";
    }
    return 0;
}

} // namespace drmemtrace
} // namespace dynamorio
