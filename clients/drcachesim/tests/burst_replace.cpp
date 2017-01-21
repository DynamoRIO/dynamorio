/* **********************************************************
 * Copyright (c) 2016 Google, Inc.  All rights reserved.
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
 * a "burst" of execution in the middle of the application.  It then detaches.
 */

/* We deliberately do not include configure.h here to simulate what an
 * actual app will look like.  configure_DynamoRIO_static sets DR_APP_EXPORTS
 * for us.
 */
#include "dr_api.h"
#include "drmemtrace.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1/*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

static int
do_some_work(int i)
{
    static int iters = 512;
    double val = (double)i;
    for (int i = 0; i < iters; ++i) {
        val += sin(val);
    }
    return (val > 0);
}

static file_t
local_open_file(const char *fname, uint mode_flags)
{
    /* This is called within the DR context, so we use DR
     * functions for transparency.
     */
    file_t f = dr_open_file(fname, mode_flags);
    dr_fprintf(STDERR, "open file %s with flag 0x%x @ %d\n",
               fname, mode_flags, f);
    return f;
}

static ssize_t
local_read_file(file_t file, void *data, size_t count)
{
    /* This is called within the DR context, so we use DR
     * functions for transparency.
     */
    ssize_t res = dr_read_file(file, data, count);
    dr_fprintf(STDERR, "reading %u bytes from file %d to @ " PFX
               ", actual read %d bytes\n",
               count, file, data, res);
    return res;
}

static ssize_t
local_write_file(file_t file, const void *data, size_t size)
{
    static int count = 0;
    /* This is called within the DR context, so we use DR
     * functions for transparency.
     */
    ssize_t res = dr_write_file(file, data, size);
    dr_fprintf(STDERR, "%d: writing %u bytes @ " PFX
               " to file %d, actual write %d bytes\n",
               count, size, data, file, res);
    /* Assuming it is a single-threaded app, we do not worry about
     * racy access on count.
     */
    if (count++ == 1) {
        dr_fprintf(STDERR, "restore the write file function\n");
        drmemtrace_replace_file_ops(NULL, NULL, dr_write_file, NULL, NULL);
    }
    return res;
}

static void
local_close_file(file_t file)
{
    /* This is called within the DR context, so we use DR
     * functions for transparency.
     */
    dr_fprintf(STDERR, "close file %d\n", file);
    dr_close_file(file);
}

static bool
local_create_dir(const char *dir)
{
    bool res = dr_create_dir(dir);
    /* This is called within the DR context, so we use DR
     * functions for transparency.
     */
    dr_fprintf(STDERR, "create dir %s %s\n",
               res ? "successfully" : "failed to", dir);
    return res;
}

int
main(int argc, const char *argv[])
{
    static int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static int iter_start = outer_iters/3;
    static int iter_stop = iter_start + 4;

    if (!my_setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    std::cerr << "replace all file functions\n";
    drmemtrace_replace_file_ops(local_open_file, local_read_file, local_write_file,
                                local_close_file, local_create_dir);
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
    return 0;
}
