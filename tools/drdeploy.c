/* **********************************************************
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

/* compile with make VMAP=1 for a vmap version (makefile defaults to VMSAFE version) */

#include "configure.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include "globals_shared.h"
#include "dr_config.h" /* MUST be before share.h (it sets HOT_PATCHING_INTERFACE) */
#include "config.h"
#include "share.h"

typedef enum _action_t {
    action_none,
    action_nudge,
    action_register,
    action_unregister,
    action_list,
} action_t;

#define fatal(msg, ...) \
    fprintf(stderr, "ERROR: " msg "\n", __VA_ARGS__);    \
    exit(1);

#define warn(msg, ...) \
    fprintf(stderr, "WARNING: " msg "\n", __VA_ARGS__);

const char *usage_str = 
    "usage: drdeploy [options]\n"
    "       -v                 Display version information\n"
    "       -reg <process>     Register <process> to run under DR\n"
    "       -unreg <process>   Unregister <process> from running under DR\n"
    "       -isreg <process>   Display whether <process> is registered and if so its\n"
    "                          configuration\n"
    "       -list_registered   Display all registered processes and their configuration\n"
    "       -root <root>       DR root directory\n"
#if defined(MF_API) && defined(PROBE_API)
    "       -mode <mode>       DR mode (code, probe, or security)\n"
#elif defined(PROBE_API)
    "       -mode <mode>       DR mode (code or probe)\n"
#elif defined(MF_API)
    "       -mode <mode>       DR mode (code or security)\n"
#else
    /* No mode argument, is always code. */
#endif
    "       -debug             Use the DR debug library\n"
    "       -32                Target 32-bit or WOW64 applications\n"
    "       -64                Target 64-bit (non-WOW64) applications\n"
    "\n"
    "       -ops \"<options>\"   Additional DR control options.  When specifying\n"
    "                          multiple options, enclose the entire list of\n"
    "                          options in quotes.\n"
    "\n"
    "       -client <path> <ID> \"<options>\"\n"
    "                          Register one or more clients to run alongside DR.\n"
    "                          This option is only valid when registering a\n"
    "                          process.  The -client option takes three arguments:\n"
    "                          the full path to a client library, a unique 8-digit\n"
    "                          hex ID, and an optional list of client options\n"
    "                          (use \"\" to specify no options).  Multiple clients\n"
    "                          can be installed via multiple -client options.  In\n"
    "                          this case, clients specified first on the command\n"
    "                          line have higher priority.  Neither the path nor\n"
    "                          the options may contain semicolon characters.\n"
    "\n"
    "       Note that nudging 64-bit processes is not yet supported.\n"
    "       -nudge <process> <client ID> <argument>\n"
    "                          Nudge the client with ID <client ID> in all running\n"
    "                          processes with name <process>, and pass <argument>\n"
    "                          to the nudge callback.  <client ID> must be the\n"
    "                          8-digit hex ID of the target client.  <argument>\n"
    "                          should be a hex literal (0, 1, 3f etc.).\n"
    "       -nudge_pid <process_id> <client ID> <argument>\n"
    "                          Nudge the client with ID <client ID> in the process with\n"
    "                          id <process_id>, and pass <argument> to the nudge\n"
    "                          callback.  <client ID> must be the 8-digit hex ID\n"
    "                          of the target client.  <argument> should be a hex\n"
    "                          literal (0, 1, 3f etc.).\n"
    "       -nudge_all <client ID> <argument>\n"
    "                          Nudge the client with ID <client ID> in all running\n"
    "                          processes and pass <argument> to the nudge callback.\n"
    "                          <client ID> must be the 8-digit hex ID of the target\n"
    "                          client.  <argument> should be a hex literal\n"
    "                          (0, 1, 3f etc.)\n"
    "       -nudge_timeout <ms> Max time (in milliseconds) to wait for a nudge to\n"
    "                          finish before continuing.  The default is an infinite\n"
    "                          wait.  A value of 0 means don't wait for nudges to\n"
    "                          complete.";

#define usage(msg, ...)                                         \
    fprintf(stderr, "\n");                                      \
    fprintf(stderr, "ERROR: " msg "\n\n", __VA_ARGS__);         \
    fprintf(stderr, "%s\n", usage_str);                         \
    exit(1);


/* Unregister a process */
void unregister_proc(const char *process, dr_platform_t dr_platform)
{
    dr_config_status_t status = dr_unregister_process(process, dr_platform);
    if (status == DR_PROC_REG_INVALID) {
        fatal("no existing registration");
    }
    else if (status == DR_FAILURE) {
        fatal("unregistration failed");
    }
}


/* Check if the provided root directory actually has the files we
 * expect.
 */
void check_dr_root(const char *dr_root)
{
    int i;
    char buf[MAX_PATH];

    const char *checked_files[] = {
        "lib32\\drpreinject.dll",
        "lib32\\release\\dynamorio.dll",
        "lib32\\debug\\dynamorio.dll",
        "lib64\\drpreinject.dll",
        "lib64\\release\\dynamorio.dll",
        "lib64\\debug\\dynamorio.dll"
    };

    for (i=0; i<_countof(checked_files); i++) {
        sprintf_s(buf, _countof(buf), "%s/%s", dr_root, checked_files[i]);
        if (_access(buf, 0) == -1) {
            warn("%s does not appear to be a valid DynamoRIO root", dr_root);
            return;
        }
    }
}

/* Register a process to run under DR */
void register_proc(const char *process,
                   const char *dr_root,
                   const dr_operation_mode_t dr_mode,
                   bool debug,
                   dr_platform_t dr_platform,
                   const char *extra_ops)
{
    dr_config_status_t status;

    if (dr_root == NULL) {
        usage("you must provide the DynamoRIO root directory");
    }
    if (dr_mode == DR_MODE_NONE) {
        usage("you must provide a DynamoRIO mode");
    }

    /* If this is the first setting of AppInit on NT, warn about reboot */
    if (!is_autoinjection_set()) {
        DWORD platform;
        if (get_platform(&platform) == ERROR_SUCCESS &&
            platform == PLATFORM_WIN_NT_4) {
            warn("on Windows NT, applications will not be taken over until reboot");
        }
    }

    if (dr_process_is_registered(process, dr_platform, NULL, NULL, NULL, NULL)) {
        warn("overriding existing registration");
        unregister_proc(process, dr_platform);
    }

    status = dr_register_process(process, dr_root, dr_mode,
                                 debug, dr_platform, extra_ops);

    if (status != DR_SUCCESS) {
        /* PR 233108: try to give more info on whether a privilege failure */
        if (create_root_key() == ERROR_ACCESS_DENIED)
            warn("insufficient privileges: re-run as administrator");
        fatal("process registration failed");
    }
    
    /* warn if the DR root directory doesn't look right */
    check_dr_root(dr_root);
}


/* Check if the specified client library actually exists. */
void check_client_lib(const char *client_lib)
{
    if (_access(client_lib, 0) == -1) {
        warn("%s does not exist", client_lib);
    }
}


void register_client(const char *process_name,
                     dr_platform_t dr_platform,
                     client_id_t client_id,
                     const char *path,
                     const char *options)
{
    size_t priority;
    dr_config_status_t status;

    if (!dr_process_is_registered(process_name, dr_platform,
                                  NULL, NULL, NULL, NULL)) {
        fatal("can't register client: process is not registered");
    }

    check_client_lib(path);

    /* just append to the existing client list */
    priority = dr_num_registered_clients(process_name, dr_platform);

    status = dr_register_client(process_name, dr_platform, client_id,
                                priority, path, options);

    if (status != DR_SUCCESS) {
        fatal("client registration failed");
    }
}

static const char *
platform_name(dr_platform_t platform)
{
    return (platform == DR_PLATFORM_64BIT
            IF_X64(|| platform == DR_PLATFORM_DEFAULT)) ?
        "64-bit" : "32-bit/WOW64";
}

static void
list_process(char *name, dr_platform_t platform, dr_registered_process_iterator_t *iter)
{
    char name_buf[MAX_PATH] = {0};
    char root_dir_buf[MAX_PATH] = {0};
    dr_operation_mode_t dr_mode;
    bool debug;
    char dr_options[DR_MAX_OPTIONS_LENGTH] = {0};
    dr_client_iterator_t *c_iter;

    if (name == NULL) {
        dr_registered_process_iterator_next(iter, name_buf, root_dir_buf, &dr_mode,
                                            &debug, dr_options);
        name = name_buf;
    } else if (!dr_process_is_registered(name, platform, root_dir_buf, &dr_mode,
                                         &debug, dr_options)) {
        printf("Process %s not registered for %s\n", name, platform_name(platform));
        return;
    }

    printf("Process %s registered for %s\n", name, platform_name(platform));
    printf("\tRoot=\"%s\" Debug=%s\n\tOptions=\"%s\"\n",
           root_dir_buf, debug ? "yes" : "no", dr_options);
    
    c_iter = dr_client_iterator_start(name, platform);
    while (dr_client_iterator_hasnext(c_iter)) {
        client_id_t id;
        size_t client_pri;
        char client_path[MAX_PATH] = {0};
        char client_opts[DR_MAX_OPTIONS_LENGTH] = {0};
        
        dr_client_iterator_next(c_iter, &id, &client_pri, client_path, client_opts);
        
        printf("\tClient=0x%08x Priority=%d\n\t\tPath=\"%s\"\n\t\tOptions=\"%s\"\n",
               id, (uint)client_pri, client_path, client_opts);
    }
    dr_client_iterator_stop(c_iter);
}


int main(int argc, char *argv[])
{
    char *process = NULL;
    char *dr_root = NULL;
    char *client_paths[MAX_CLIENT_LIBS] = {NULL,};
    char *client_options[MAX_CLIENT_LIBS] = {NULL,};
    client_id_t client_ids[MAX_CLIENT_LIBS] = {0,};
    size_t num_clients = 0;
#if defined(MF_API) || defined(PROBE_API)
    /* must set -mode */
    dr_operation_mode_t dr_mode = DR_MODE_NONE;
#else
    /* only one choice so no -mode */
    dr_operation_mode_t dr_mode = DR_MODE_CODE_MANIPULATION;
#endif
    char *extra_ops = NULL;
    action_t action = action_none;
    bool use_debug = false;
    dr_platform_t dr_platform = DR_PLATFORM_DEFAULT;
    bool nudge_all = false;
    process_id_t nudge_pid = 0;
    client_id_t nudge_id = 0;
    uint64 nudge_arg = 0;
    bool list_registered = false;
    uint nudge_timeout = INFINITE;
    
    int i;

    /* parse command line */
    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-debug") == 0) {
            use_debug = true;
            continue;
        }
        else if (!strcmp(argv[i], "-v")) {
#if defined(BUILD_NUMBER) && defined(VERSION_NUMBER)
          printf("drdeploy.exe version %s -- build %d\n", STRINGIFY(VERSION_NUMBER), BUILD_NUMBER);
#elif defined(BUILD_NUMBER)
          printf("drdeploy.exe custom build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
          printf("drdeploy.exe custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
          exit(0);
        } else if (!strcmp(argv[i], "-list_registered")) {
            action = action_list;
            list_registered = true;
            continue;
        }
        /* all other flags have an argument -- make sure it exists */
        else if (i == argc - 1) {
            usage("invalid arguments");
        }

        if (strcmp(argv[i], "-reg") == 0) {
            if (action != action_none) {
                usage("more than one action specified");
            }
            action = action_register;
            process = argv[++i];
        }
        else if (strcmp(argv[i], "-unreg") == 0) {
            if (action != action_none) {
                usage("more than one action specified");
            }
            action = action_unregister;
            process = argv[++i];
        }
        else if (strcmp(argv[i], "-isreg") == 0) {
            if (action != action_none) {
                usage("more than one action specified");
            }
            action = action_list;
            process = argv[++i];
        }
        else if (strcmp(argv[i], "-nudge_timeout") == 0) {
            nudge_timeout = strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "-nudge") == 0 ||
                 strcmp(argv[i], "-nudge_pid") == 0 ||
                 strcmp(argv[i], "-nudge_all") == 0){
            if (action != action_none) {
                usage("more than one action specified");
            }
            if (i + 2 >= argc || 
                (strcmp(argv[i], "-nudge_all") != 0 && i + 3 >= argc)) {
                usage("too few arguments to -nudge");
            }
            action = action_nudge;
            if (strcmp(argv[i], "-nudge") == 0)
                process = argv[++i];
            else if (strcmp(argv[i], "-nudge_pid") == 0)
                nudge_pid = strtoul(argv[++i], NULL, 10);
            else
                nudge_all = true;
            nudge_id = strtoul(argv[++i], NULL, 16);
            nudge_arg = _strtoui64(argv[++i], NULL, 16);
        }        
        else if (strcmp(argv[i], "-root") == 0) {
            dr_root = argv[++i];
        }
#if defined(MF_API) || defined(PROBE_API)
        else if (strcmp(argv[i], "-mode") == 0) {
            char *mode_str = argv[++i];
            if (strcmp(mode_str, "code") == 0) {
                dr_mode = DR_MODE_CODE_MANIPULATION;
            }
# ifdef MF_API
            else if (strcmp(mode_str, "security") == 0) {
                dr_mode = DR_MODE_MEMORY_FIREWALL;
            }
# endif
# ifdef PROBE_API
            else if (strcmp(mode_str, "probe") == 0) {
                dr_mode = DR_MODE_PROBE;
            }
# endif
            else {
                usage("unknown mode: %s", mode_str);
            }
        }
#endif
        else if (strcmp(argv[i], "-client") == 0) {
            if (num_clients == MAX_CLIENT_LIBS) {
                fatal("Maximum number of clients is %d", MAX_CLIENT_LIBS);
            }
            else {
                if (i + 3 >= argc) {
                    usage("too few arguments to -client");
                }

                client_paths[num_clients] = argv[++i];
                client_ids[num_clients] = strtoul(argv[++i], NULL, 16);
                client_options[num_clients] = argv[++i];
                num_clients++;
            }
        }
        else if (strcmp(argv[i], "-ops") == 0) {
            extra_ops = argv[++i];
	}
        else if (strcmp(argv[i], "-32") == 0) {
	    dr_platform = DR_PLATFORM_32BIT;
	}
        else if (strcmp(argv[i], "-64") == 0) {
	    dr_platform = DR_PLATFORM_64BIT;
        }
        else {
            usage("unknown option: %s", argv[i]);
        }
    }

    /* PR 244206: set the registry view before any registry access */
    set_dr_platform(dr_platform);

    if (action == action_register) {
        size_t i;
        register_proc(process, dr_root, dr_mode, use_debug, dr_platform, extra_ops);
        for (i=0; i<num_clients; i++) {
            register_client(process, dr_platform, client_ids[i],
                            client_paths[i], client_options[i]);
        }
    }
    else if (action == action_unregister) {
        unregister_proc(process, dr_platform);
    }
    else if (action == action_nudge) {
        int count = 1;
        dr_config_status_t res = DR_SUCCESS;
        if (nudge_all)
            res = dr_nudge_all(nudge_id, nudge_arg, nudge_timeout, &count);
        else if (nudge_pid != 0) {
            res = dr_nudge_pid(nudge_pid, nudge_id, nudge_arg, nudge_timeout);
            if (res == DR_NUDGE_PID_NOT_INJECTED)
                printf("process %d is not running under DR\n", nudge_pid);
            if (res != DR_SUCCESS && res != DR_NUDGE_TIMEOUT) {
                count = 0;
                res = ERROR_SUCCESS;
            }
        } else
            res = dr_nudge_process(process, nudge_id, nudge_arg, nudge_timeout, &count);

        printf("%d processes nudged\n", count);
        if (res == DR_NUDGE_TIMEOUT)
            printf("timed out waiting for nudge to complete");
        else if (res != DR_SUCCESS)
            printf("nudge operation failed, verify adequate permissions for this operation.");
    }
    else if (action == action_list) {
        if (!list_registered)
            list_process(process, dr_platform, NULL);
        else /* list all */ {
            dr_registered_process_iterator_t *iter =
                dr_registered_process_iterator_start(dr_platform);
            printf("Registered processes for %s\n",  platform_name(dr_platform));
            while (dr_registered_process_iterator_hasnext(iter))
                list_process(NULL, dr_platform, iter);
            dr_registered_process_iterator_stop(iter);
        }
    }
    else  {
        usage("no action specified");
    }

    return 0;
}
