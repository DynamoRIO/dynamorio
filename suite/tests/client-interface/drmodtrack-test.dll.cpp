/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

/* Tests the drmodtrack extension. */

#include "dr_api.h"
#include "drcovlib.h"
#include "drx.h"
#include "client_tools.h"
#include "string.h"
#include "stddef.h"

#ifdef WINDOWS
#    pragma warning(disable : 4100) /* unreferenced formal parameter */
#    pragma warning(disable : 4127) /* conditional expression is constant */
#endif

#define CHECK(x, msg)                                                                \
    do {                                                                             \
        if (!(x)) {                                                                  \
            dr_fprintf(STDERR, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, msg); \
            dr_abort();                                                              \
        }                                                                            \
    } while (0);

static client_id_t client_id;

static void *
load_cb(module_data_t *module)
{
    return (void *)module->start;
}

static int
print_cb(void *data, char *dst, size_t max_len)
{
    return dr_snprintf(dst, max_len, PFX ",", data);
}

static const char *
parse_cb(const char *src, OUT void **data)
{
    const char *res;
    if (dr_sscanf(src, PIFX ",", data) != 1)
        return NULL;
    res = strchr(src, ',');
    return (res == NULL) ? NULL : res + 1;
}

static void
free_cb(void *data)
{
    /* Nothing. */
}

static void
event_exit(void)
{
    /* First test online features. */
    char cwd[MAXIMUM_PATH];
    char fname[MAXIMUM_PATH];
    bool ok = dr_get_current_directory(cwd, BUFFER_SIZE_ELEMENTS(cwd));
    CHECK(ok, "dr_get_current_directory failed");
#ifdef ANDROID
    /* On Android cwd is / where we have no write privs. */
    const char *cpath = dr_get_client_path(client_id);
    const char *dir = strrchr(cpath, '/');
    dr_snprintf(cwd, BUFFER_SIZE_ELEMENTS(cwd), "%.*s", dir - cpath, cpath);
    NULL_TERMINATE_BUFFER(cwd);
#endif
    file_t f = drx_open_unique_file(cwd, "drmodtrack-test", "log", 0, fname,
                                    BUFFER_SIZE_ELEMENTS(fname));
    CHECK(f != INVALID_FILE, "drx_open_unique_file failed");

    drcovlib_status_t res = drmodtrack_dump(f);
    CHECK(res == DRCOVLIB_SUCCESS, "module dump failed");
    dr_close_file(f);

    char *buf_online;
    size_t size_online = 8192;
    size_t wrote;
    do {
        buf_online = (char *)dr_global_alloc(size_online);
        res = drmodtrack_dump_buf(buf_online, size_online, &wrote);
        if (res == DRCOVLIB_SUCCESS)
            break;
        dr_global_free(buf_online, size_online);
        size_online *= 2;
    } while (res == DRCOVLIB_ERROR_BUF_TOO_SMALL);
    CHECK(res == DRCOVLIB_SUCCESS, "module dump to buf failed");
    CHECK(wrote == strlen(buf_online) + 1 /*null*/, "returned size off");

    /* Now test offline features. */
    void *modhandle;
    uint num_mods;
    f = dr_open_file(fname, DR_FILE_READ);
    res = drmodtrack_offline_read(f, NULL, NULL, &modhandle, &num_mods);
    CHECK(res == DRCOVLIB_SUCCESS, "read failed");

    for (uint i = 0; i < num_mods; ++i) {
        drmodtrack_info_t info = {
            sizeof(info),
        };
        res = drmodtrack_offline_lookup(modhandle, i, &info);
        CHECK(res == DRCOVLIB_SUCCESS, "lookup failed");
        CHECK(((app_pc)info.custom) == info.start || info.containing_index != i,
              "custom field doesn't match");
        CHECK(info.index == i, "index field doesn't match");
#ifndef WINDOWS
        if (info.struct_size > offsetof(drmodtrack_info_t, offset)) {
            module_data_t *data = dr_lookup_module(info.start);
            for (uint j = 0; j < data->num_segments; j++) {
                module_segment_data_t *seg = data->segments + j;
                if (seg->start == info.start) {
                    CHECK(seg->offset == info.offset,
                          "Module data offset and drmodtrack offset don't match");
                }
            }
            dr_free_module_data(data);
        }
#endif
    }

    char *buf_offline;
    size_t size_offline = 8192;
    do {
        buf_offline = (char *)dr_global_alloc(size_offline);
        res = drmodtrack_offline_write(modhandle, buf_offline, size_offline, &wrote);
        if (res == DRCOVLIB_SUCCESS)
            break;
        dr_global_free(buf_offline, size_offline);
        size_offline *= 2;
    } while (res == DRCOVLIB_ERROR_BUF_TOO_SMALL);
    CHECK(res == DRCOVLIB_SUCCESS, "offline write failed");
    CHECK(size_online == size_offline, "sizes do not match");
    CHECK(wrote == strlen(buf_offline) + 1 /*null*/, "returned size off");
    CHECK(strcmp(buf_online, buf_offline) == 0, "buffers do not match");

    dr_close_file(f);
    ok = dr_delete_file(fname);
    CHECK(ok, "failed to delete file");

    res = drmodtrack_offline_exit(modhandle);
    CHECK(res == DRCOVLIB_SUCCESS, "exit failed");

    dr_global_free(buf_online, size_online);
    dr_global_free(buf_offline, size_offline);

    res = drmodtrack_exit();
    CHECK(res == DRCOVLIB_SUCCESS, "module exit failed");
}

DR_EXPORT void
dr_init(client_id_t id)
{
    client_id = id;
    drcovlib_status_t res = drmodtrack_init();
    CHECK(res == DRCOVLIB_SUCCESS, "init failed");
    res = drmodtrack_add_custom_data(load_cb, print_cb, parse_cb, free_cb);
    CHECK(res == DRCOVLIB_SUCCESS, "customization failed");
    dr_register_exit_event(event_exit);
}
