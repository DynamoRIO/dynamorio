/* **********************************************************
 * Copyright (c) 2016-2017 Google, Inc.  All rights reserved.
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
#include "drmemtrace/drmemtrace.h"
#include "drvector.h"
#include "../../../suite/tests/client_tools.h"
#include <assert.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool
my_setenv(const char *var, const char *value)
{
#ifdef UNIX
    return setenv(var, value, 1 /*override*/) == 0;
#else
    return SetEnvironmentVariable(var, value) == TRUE;
#endif
}

/* We have combine all the chunks into one global list, and at process exit
 * we split them into thread files.  To identify which file we map a sentinel
 * for the module list and the thread id's to file_t.
 */

drvector_t all_buffers;
#define ALL_BUFFERS_INIT_SIZE 256

#define MODULE_FILENO 0

typedef struct _buf_entry_t {
    file_t id; /* MODULE_FILENO or thread id */
    void *data;
    size_t data_size;
    size_t alloc_size;
} buf_entry_t;

static void
free_entry(void *e)
{
    buf_entry_t *entry = (buf_entry_t *)e;
    dr_global_free(entry, sizeof(*entry));
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
    static bool called;
    if (!called) {
        called = true;
        /* This is where we initialize because DR is now initialized. */
        drvector_init(&all_buffers, ALL_BUFFERS_INIT_SIZE, false, free_entry);
        return MODULE_FILENO;
    }
    return (file_t)dr_get_thread_id(dr_get_current_drcontext());
}

static ssize_t
local_read_file(file_t file, void *data, size_t count)
{
    return 0; /* not used */
}

static ssize_t
local_write_file(file_t file, const void *data, size_t size)
{
    if (file == MODULE_FILENO) {
        void *copy = dr_raw_mem_alloc(size, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
        memcpy(copy, data, size);
        drvector_lock(&all_buffers);
        buf_entry_t *entry = (buf_entry_t *)dr_global_alloc(sizeof(*entry));
        entry->id = file;
        entry->data = copy;
        entry->data_size = size;
        entry->alloc_size = size;
        drvector_append(&all_buffers, entry);
        drvector_unlock(&all_buffers);
        return size;
    }
    ASSERT(false); /* Shouldn't be called. */
    return 0;
}

static bool
handoff_cb(file_t file, void *data, size_t data_size, size_t alloc_size)
{
    drvector_lock(&all_buffers);
    buf_entry_t *entry = (buf_entry_t *)dr_global_alloc(sizeof(*entry));
    entry->id = file;
    entry->data = data;
    entry->data_size = data_size;
    entry->alloc_size = alloc_size;
    drvector_append(&all_buffers, entry);
    drvector_unlock(&all_buffers);
    return true;
}

static void
local_close_file(file_t file)
{
    /* Nothing. */
}

static bool
local_create_dir(const char *dir)
{
    return dr_create_dir(dir);
}

static void
exit_cb(void *arg)
{
    assert(arg == (void *)&all_buffers);
    uint i;
    file_t f;
    char path[MAXIMUM_PATH];
    const char *modlist_path;
    drmemtrace_status_t mem_res = drmemtrace_get_modlist_path(&modlist_path);
    assert(mem_res == DRMEMTRACE_SUCCESS);
    const char *slash = strrchr(modlist_path, '/');
#ifdef WINDOWS
    const char *bslash = strrchr(modlist_path, '\\');
    if (bslash != NULL && bslash > slash)
        slash = bslash;
#endif
    assert(slash != NULL);
    int len = dr_snprintf(path, BUFFER_SIZE_ELEMENTS(path), "%.*s", slash - modlist_path,
                          modlist_path);
    assert(len > 0);
    NULL_TERMINATE_BUFFER(path);

    std::cerr << "processing " << all_buffers.entries << " buffers\n";
    drvector_lock(&all_buffers);
    for (i = 0; i < all_buffers.entries; ++i) {
        buf_entry_t *entry = (buf_entry_t *)drvector_get_entry(&all_buffers, i);
        if (entry->id == MODULE_FILENO) {
            std::cerr << "creating module file " << modlist_path << "\n";
            f = dr_open_file(modlist_path, DR_FILE_WRITE_OVERWRITE);
        } else {
            char fname[MAXIMUM_PATH];
            len = dr_snprintf(fname, BUFFER_SIZE_ELEMENTS(fname), "%s/%d.raw", path,
                              entry->id);
            assert(len > 0);
            NULL_TERMINATE_BUFFER(fname);
            f = dr_open_file(fname, DR_FILE_WRITE_APPEND);
        }
        assert(f != INVALID_FILE);
        dr_write_file(f, entry->data, entry->data_size);
        dr_close_file(f);
        dr_raw_mem_free(entry->data, entry->alloc_size);
    }
    drvector_unlock(&all_buffers);
    drvector_delete(&all_buffers);
}

int
main(int argc, const char *argv[])
{
    static int outer_iters = 2048;
    /* We trace a 4-iter burst of execution. */
    static int iter_start = outer_iters / 3;
    static int iter_stop = iter_start + 4;

    if (!my_setenv("DYNAMORIO_OPTIONS", "-stderr_mask 0xc -client_lib ';;-offline'"))
        std::cerr << "failed to set env var!\n";

    std::cerr << "replace all file functions\n";
    drmemtrace_status_t res =
        drmemtrace_replace_file_ops(local_open_file, local_read_file, local_write_file,
                                    local_close_file, local_create_dir);
    assert(res == DRMEMTRACE_SUCCESS);
    res = drmemtrace_buffer_handoff(handoff_cb, exit_cb, (void *)&all_buffers);
    assert(res == DRMEMTRACE_SUCCESS);
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
