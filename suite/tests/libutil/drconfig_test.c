/* **********************************************************
 * Copyright (c) 2020 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dr_api.h"
#include "dr_config.h"

static void
check(bool condition, const char *error_msg)
{
    if (!condition) {
        fprintf(stderr, "ERROR: %s\n", error_msg);
        exit(1);
    }
}

static void
test_register(const char *name, process_id_t pid, bool global, dr_platform_t dr_platform,
              const char *dr_root)
{
    dr_config_status_t status;
    bool query_debug;
    dr_operation_mode_t query_mode;
    char query_root[MAXIMUM_PATH];
    char query_ops[DR_MAX_OPTIONS_LENGTH];
    /* Unregister first, in case a stale file from an old aborted test is still there. */
    status = dr_unregister_process(name, pid, global, dr_platform);
    check(status == DR_SUCCESS || status == DR_PROC_REG_INVALID,
          "dr_unregister_process should succeed or not find");

    status = dr_register_client(name, pid, global, dr_platform, 0, 0, "/any/path",
                                "-any -ops");
    check(status == DR_PROC_REG_INVALID,
          "dr_register_client w/o first dr_register_process should fail");

    check(
        !dr_process_is_registered(name, pid, global, dr_platform, NULL, NULL, NULL, NULL),
        "should not be registered\n");

    status = dr_register_process(name, pid, global, dr_root, DR_MODE_CODE_MANIPULATION,
                                 true, dr_platform, "-disable_traces");
    check(status == DR_SUCCESS, "dr_register_process should succeed");
    check(dr_process_is_registered(name, pid, global, dr_platform, query_root,
                                   &query_mode, &query_debug, query_ops),
          "should be registered\n");
    check(strcmp(query_root, dr_root) == 0, "root should match");
    check(query_mode == DR_MODE_CODE_MANIPULATION, "mode should match");
    check(query_debug, "debug should match");
    check(strcmp(query_ops, "-disable_traces") == 0, "ops should match");

    /* Test non-debug. */
    status = dr_unregister_process(name, pid, global, dr_platform);
    check(status == DR_SUCCESS, "dr_unregister_process should succeed");
    status = dr_register_process(name, pid, global, dr_root, DR_MODE_CODE_MANIPULATION,
                                 false, dr_platform, "-disable_traces");
    check(status == DR_SUCCESS, "dr_register_process should succeed");
    check(dr_process_is_registered(name, pid, global, dr_platform, query_root,
                                   &query_mode, &query_debug, query_ops),
          "should be registered\n");
    check(strcmp(query_root, dr_root) == 0, "root should match");
    check(query_mode == DR_MODE_CODE_MANIPULATION, "mode should match");
    check(!query_debug, "debug should match");
    check(strcmp(query_ops, "-disable_traces") == 0, "ops should match");

    status = dr_register_process(name, pid, global, dr_root, DR_MODE_CODE_MANIPULATION,
                                 false, dr_platform, "-disable_traces");
    if (pid == 0)
        check(status != DR_SUCCESS, "dr_register_process duplicate 0-pid should fail");
    else {
        check(status == DR_SUCCESS,
              "dr_register_process duplicate non-0-pid should succeed");
    }

    client_id_t my_id = 19;
    size_t my_priority = 0;
    const char *my_path = "/path/to/libclient.so";
    const char *my_alt_path = "/path/to/libclient-alt.so";
    const char *my_ops = "-foo -bar";
    check(dr_num_registered_clients(name, pid, global, dr_platform) == 0,
          "should be 0 clients pre-reg");

    status = dr_register_client(name, pid, global, dr_platform, my_id, my_priority,
                                my_path, my_ops);
    check(status == DR_SUCCESS, "dr_register_client should succeed");
    check(dr_num_registered_clients(name, pid, global, dr_platform) == 1,
          "should be 1 client post-reg");

    size_t client_pri;
    char client_path[MAXIMUM_PATH];
    char client_ops[MAXIMUM_PATH];
    status = dr_get_client_info(name, pid, global, dr_platform, my_id, &client_pri,
                                client_path, client_ops);
    check(status == DR_SUCCESS, "dr_get_client_info should succeed");
    check(client_pri == my_priority, "priority query doesn't match");
    check(strcmp(client_path, my_path) == 0, "path doesn't match");
    check(strcmp(client_ops, my_ops) == 0, "options don't match");

    status = dr_unregister_client(name, pid, global, dr_platform, my_id);
    check(status == DR_SUCCESS, "dr_unregister_client should succeed");

    dr_config_client_t client;
    client.struct_size = 0;
    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_CONFIG_INVALID_PARAMETER, "bad struct_size should fail");
    client.struct_size = sizeof(client);

    client.id = my_id;
    client.priority = my_priority;
    client.path = (char *)my_alt_path;
    client.options = (char *)my_ops;
    client.is_alt_bitwidth = true;
    client.priority = 0;
    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_CONFIG_CLIENT_NOT_FOUND, "register alt first should fail");

    client.id = my_id;
    client.priority = my_priority;
    client.path = (char *)my_path;
    client.options = (char *)my_ops;
    client.is_alt_bitwidth = false;
    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_SUCCESS, "dr_register_client_ex should succeed");
    check(dr_num_registered_clients(name, pid, global, dr_platform) == 1,
          "should be 1 client post-reg");

    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_ID_CONFLICTING, "dup id should fail");

    client.id = my_id + 1;
    client.priority = my_priority + 1; /* Append. */
    client.path = (char *)my_alt_path;
    client.options = (char *)my_ops;
    client.is_alt_bitwidth = true;
    client.priority = 0;
    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_CONFIG_CLIENT_NOT_FOUND, "register diff-ID alt first should fail");

    client.id = my_id;
    client.path = (char *)my_alt_path;
    client.options = (char *)my_ops;
    client.is_alt_bitwidth = true;
    client.priority = my_priority + 1; /* Append. */
    status = dr_register_client_ex(name, pid, global, dr_platform, &client);
    check(status == DR_SUCCESS, "dr_register_client_ex alt should succeed");
    check(dr_num_registered_clients(name, pid, global, dr_platform) == 2,
          "should be 2 clients post-alt-reg");

    /* Plain query should still find the non-alt info. */
    status = dr_get_client_info(name, pid, global, dr_platform, my_id, &client_pri,
                                client_path, client_ops);
    check(status == DR_SUCCESS, "dr_get_client_info should succeed");
    check(client_pri == my_priority, "priority query doesn't match");
    check(strcmp(client_path, my_path) == 0, "path doesn't match");
    check(strcmp(client_ops, my_ops) == 0, "options don't match");
    client.path = client_path;
    client.options = client_ops;
    client.id = my_id;
    status = dr_get_client_info_ex(name, pid, global, dr_platform, &client);
    check(status == DR_SUCCESS, "dr_get_client_info_ex should succeed");
    check(client.priority == my_priority, "priority query doesn't match");
    check(strcmp(client.path, my_path) == 0, "path doesn't match");
    check(strcmp(client.options, my_ops) == 0, "options don't match");

    dr_client_iterator_t *iter = dr_client_iterator_start(name, pid, global, dr_platform);
    check(iter != NULL, "iter should instantiate");
    unsigned int client_id;
    int count = 0;
    while (dr_client_iterator_hasnext(iter)) {
        dr_client_iterator_next(iter, &client_id, &client_pri, client_path, client_ops);
        check(client_id == my_id, "id doesn't match");
        if (count == 0) {
            check(client_pri == my_priority, "priority doesn't match");
            check(strcmp(client_path, my_path) == 0, "path doesn't match");
            check(strcmp(client_ops, my_ops) == 0, "options don't match");
        } else {
            check(client_pri == my_priority + 1, "priority doesn't match");
            check(strcmp(client_path, my_alt_path) == 0, "alt path doesn't match");
            check(strcmp(client_ops, my_ops) == 0, "options don't match");
        }
        ++count;
    }
    dr_client_iterator_stop(iter);

    iter = dr_client_iterator_start(name, pid, global, dr_platform);
    check(iter != NULL, "iter should instantiate");
    count = 0;
    client.struct_size = sizeof(client);
    client.path = client_path;
    client.options = client_ops;
    while (dr_client_iterator_hasnext(iter)) {
        dr_client_iterator_next_ex(iter, &client);
        check(client.id == my_id, "id doesn't match");
        if (count == 0) {
            check(client.priority == my_priority, "priority doesn't match");
            check(strcmp(client.path, my_path) == 0, "path doesn't match");
            check(strcmp(client.options, my_ops) == 0, "options don't match");
            check(!client.is_alt_bitwidth, "is_alt_bitwidth doesn't match");
        } else {
            check(client.priority == my_priority + 1, "priority doesn't match");
            check(strcmp(client.path, my_alt_path) == 0, "alt path doesn't match");
            check(strcmp(client.options, my_ops) == 0, "options don't match");
            check(client.is_alt_bitwidth, "is_alt_bitwidth doesn't match");
        }
        ++count;
    }
    dr_client_iterator_stop(iter);

    /* A single unregister should remove both b/c they have the same ID. */
    status = dr_unregister_client(name, pid, global, dr_platform, my_id);
    check(status == DR_SUCCESS, "dr_unregister_client should succeed");
    check(dr_num_registered_clients(name, pid, global, dr_platform) == 0,
          "should be 0 clients post-unreg");

    status = dr_unregister_process(name, pid, global, dr_platform);
    check(status == DR_SUCCESS, "dr_unregister_process should succeed");
}

int
main(int argc, const char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Requires 1 argument: DR root dir\n");
        return 1;
    }
    const char *dr_root = argv[1];

#define PROC_NAME "fake_process"

    test_register(PROC_NAME, 0, false, DR_PLATFORM_32BIT, dr_root);
    test_register(PROC_NAME, 0, false, DR_PLATFORM_64BIT, dr_root);
    test_register(PROC_NAME, 42, false, DR_PLATFORM_32BIT, dr_root);
    test_register(PROC_NAME, 42, false, DR_PLATFORM_64BIT, dr_root);

    fprintf(stderr, "all done\n");
    return 0;
}
