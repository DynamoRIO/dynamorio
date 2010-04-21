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
#include <time.h>
#include <assert.h>
#include "globals_shared.h"
#include "dr_config.h" /* MUST be before share.h (it sets HOT_PATCHING_INTERFACE) */
#include "dr_inject.h"
#include "config.h"
#include "share.h"

#define MAX_CMDLINE 4096

typedef enum _action_t {
    action_none,
    action_nudge,
    action_register,
    action_unregister,
    action_list,
} action_t;

static bool verbose;
static bool quiet;
static bool DR_dll_not_needed;
static bool nocheck;

#define fatal(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", __VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

#define warn(msg, ...) do { \
    if (!quiet) { \
        fprintf(stderr, "WARNING: " msg "\n", __VA_ARGS__); \
        fflush(stderr); \
    } \
} while (0)

#define info(msg, ...) do { \
    if (verbose) { \
        fprintf(stderr, "INFO: " msg "\n", __VA_ARGS__); \
        fflush(stderr); \
    } \
} while (0)

#ifdef DRCONFIG
# define TOOLNAME "drconfig"
#elif defined(DRRUN)
# define TOOLNAME "drrun"
#elif defined(DRINJECT)
# define TOOLNAME "drinject"
#endif

const char *usage_str = 
#ifdef DRCONFIG
    "usage: "TOOLNAME" [options]\n"
#elif defined(DRRUN) || defined (DRINJECT)
    "usage: "TOOLNAME" [options] <app and args to run>\n"
#endif
    "       -v                 Display version information\n"
    "       -verbose           Display additional information\n"
    "       -quiet             Do not display warnings\n"
    "       -nocheck           Do not fail due to invalid DynamoRIO installation\n"
#ifdef DRCONFIG
    "       -reg <process>     Register <process> to run under DR\n"
    "       -unreg <process>   Unregister <process> from running under DR\n"
    "       -isreg <process>   Display whether <process> is registered and if so its\n"
    "                          configuration\n"
    "       -list_registered   Display all registered processes and their configuration\n"
#endif
    "       -root <root>       DR root directory\n"
#if defined(DRCONFIG) || defined(DRRUN)
# if defined(MF_API) && defined(PROBE_API)
    "       -mode <mode>       DR mode (code, probe, or security)\n"
# elif defined(PROBE_API)
    "       -mode <mode>       DR mode (code or probe)\n"
# elif defined(MF_API)
    "       -mode <mode>       DR mode (code or security)\n"
# else
    /* No mode argument, is always code. */
# endif
#endif
#ifdef DRCONFIG
    "       -syswide_on        Set up systemwide injection so that registered\n"
    "                          applications will run under DR however they are\n"
    "                          launched.  Otherwise, drinject must be used to\n"
    "                          launch a target configured application under DR.\n"
    "                          This option requires administrative privileges.\n"
    "       -syswide_off       Disable systemwide injection.\n"
    "                          This option requires administrative privileges.\n"
    "       -global            Use global configuration files instead of local\n"
    "                          user-private configuration files.  The global\n"
    "                          config dir must be set up ahead of time.\n"
    "                          This option may require administrative privileges.\n"
    "                          If a local file already exists it will take precedence.\n"
#endif
    "       -debug             Use the DR debug library\n"
    "       -32                Target 32-bit or WOW64 applications\n"
    "       -64                Target 64-bit (non-WOW64) applications\n"
#if defined(DRCONFIG) || defined(DRRUN)
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
#endif
#ifdef DRCONFIG
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
    "                          complete."
#else
    "       -no_wait           Return immediately: do not wait for application exit.\n"
    "       -s <seconds>       Kill the application if it runs longer than the\n"
    "                          specified number of seconds.\n"
    "       -m <minutes>       Kill the application if it runs longer than the\n"
    "                          specified number of minutes.\n"
    "       -h <hours>         Kill the application if it runs longer than the\n"
    "                          specified number of hours.\n"
    "       -stats             Print /usr/bin/time-style elapsed time and memory used.\n"
    "       -mem               Print memory usage statistics.\n"
    "       -pidfile <file>    Print the pid of the child process to the given file.\n"
    "       -no_inject         Run the application natively.\n"
    "       -use_dll <dll>     Inject given dll instead of configured DR dll.\n"
    "       -force             Inject regardless of configuration.\n"
    "       -exit0             Return a 0 exit code instead of the app's exit code.\n"
    "\n"
    "       <app and args>     Application command line to execute under DR.\n"
#endif
    ;

#define usage(msg, ...) do {                                    \
    fprintf(stderr, "\n");                                      \
    fprintf(stderr, "ERROR: " msg "\n\n", __VA_ARGS__);         \
    fprintf(stderr, "%s\n", usage_str);                         \
    exit(1);                                                    \
} while (0)

/* Unregister a process */
void unregister_proc(const char *process, process_id_t pid,
                     bool global, dr_platform_t dr_platform)
{
    dr_config_status_t status = dr_unregister_process(process, pid, global, dr_platform);
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
static void check_dr_root(const char *dr_root, bool debug,
                          dr_platform_t dr_platform, bool preinject)
{
    int i;
    char buf[MAX_PATH];
    bool ok = true;

    const char *checked_files[] = {
        "lib32\\drpreinject.dll",
        "lib32\\release\\dynamorio.dll",
        "lib32\\debug\\dynamorio.dll",
        "lib64\\drpreinject.dll",
        "lib64\\release\\dynamorio.dll",
        "lib64\\debug\\dynamorio.dll"
    };

    const char *arch = IF_X64_ELSE("lib64", "lib32");
    if (dr_platform == DR_PLATFORM_32BIT)
        arch = "lib32";
    else if (dr_platform == DR_PLATFORM_64BIT)
        arch = "lib64";

    if (DR_dll_not_needed) {
        /* assume user knows what he's doing */
        return;
    }

    for (i=0; i<_countof(checked_files); i++) {
        sprintf_s(buf, _countof(buf), "%s/%s", dr_root, checked_files[i]);
        if (_access(buf, 0) == -1) {
            ok = false;
            if (!nocheck &&
                ((preinject && strstr(checked_files[i], "drpreinject")) ||
                 (!preinject && debug && strstr(checked_files[i], "debug") != NULL) ||
                 (!preinject && !debug && strstr(checked_files[i], "release") != NULL)) &&
                strstr(checked_files[i], arch) != NULL) {
                /* We don't want to create a .1config file that won't be freed
                 * b/c the core is never injected
                 */
                fatal("cannot find required file %s\n"
                      "Use -root to specify a proper DynamoRIO root directory.", buf);
            } else {
                warn("cannot find %s: is this an incomplete installation?", buf);
            }
        }
    }
    if (!ok)
        warn("%s does not appear to be a valid DynamoRIO root", dr_root);
}

/* Register a process to run under DR */
void register_proc(const char *process,
                   process_id_t pid,
                   bool global,
                   const char *dr_root,
                   const dr_operation_mode_t dr_mode,
                   bool debug,
                   dr_platform_t dr_platform,
                   const char *extra_ops)
{
    dr_config_status_t status;

    assert(dr_root != NULL);
    if (_access(dr_root, 0) == -1) {
        usage("cannot access DynamoRIO root directory %s", dr_root);
    }
    if (dr_mode == DR_MODE_NONE) {
        usage("you must provide a DynamoRIO mode");
    }
    
    /* warn if the DR root directory doesn't look right */
    check_dr_root(dr_root, debug, dr_platform, false);

    if (dr_process_is_registered(process, pid, global, dr_platform,
                                 NULL, NULL, NULL, NULL)) {
        warn("overriding existing registration");
        unregister_proc(process, pid, global, dr_platform);
    }

    status = dr_register_process(process, pid, global, dr_root, dr_mode,
                                 debug, dr_platform, extra_ops);

    if (status != DR_SUCCESS) {
        fatal("process registration failed");
    }
}


/* Check if the specified client library actually exists. */
void check_client_lib(const char *client_lib)
{
    if (_access(client_lib, 0) == -1) {
        warn("%s does not exist", client_lib);
    }
}


void register_client(const char *process_name,
                     process_id_t pid,
                     bool global,
                     dr_platform_t dr_platform,
                     client_id_t client_id,
                     const char *path,
                     const char *options)
{
    size_t priority;
    dr_config_status_t status;

    if (!dr_process_is_registered(process_name, pid, global, dr_platform,
                                  NULL, NULL, NULL, NULL)) {
        fatal("can't register client: process is not registered");
    }

    check_client_lib(path);

    /* just append to the existing client list */
    priority = dr_num_registered_clients(process_name, pid, global, dr_platform);

    status = dr_register_client(process_name, pid, global, dr_platform, client_id,
                                priority, path, options);

    if (status != DR_SUCCESS) {
        fatal("client registration failed with error code %d", status);
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
list_process(char *name, bool global, dr_platform_t platform,
             dr_registered_process_iterator_t *iter)
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
    } else if (!dr_process_is_registered(name, 0, global, platform, root_dir_buf,
                                         &dr_mode, &debug, dr_options)) {
        printf("Process %s not registered for %s\n", name, platform_name(platform));
        return;
    }

    printf("Process %s registered for %s\n", name, platform_name(platform));
    printf("\tRoot=\"%s\" Debug=%s\n\tOptions=\"%s\"\n",
           root_dir_buf, debug ? "yes" : "no", dr_options);
    
    c_iter = dr_client_iterator_start(name, 0, global, platform);
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

/* i#200/PR 459481: communicate child pid via file */
static void
write_pid_to_file(const char *pidfile, process_id_t pid)
{
    HANDLE f = CreateFile(pidfile, GENERIC_WRITE, FILE_SHARE_READ,
                          NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        warn("cannot open %s: %d\n", pidfile, GetLastError());
    } else {
        TCHAR pidbuf[16];
        BOOL ok;
        DWORD written;
        _snprintf(pidbuf, BUFFER_SIZE_ELEMENTS(pidbuf), "%d\n", pid);
        NULL_TERMINATE_BUFFER(pidbuf);
        ok = WriteFile(f, pidbuf, (DWORD)strlen(pidbuf), &written, NULL);
        assert(ok && written == strlen(pidbuf));
        CloseHandle(f);
    }
}

int main(int argc, char *argv[])
{
    char *process = NULL;
    char *dr_root = NULL;
    char *client_paths[MAX_CLIENT_LIBS][MAXIMUM_PATH];
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
    bool syswide_on = false;
    bool syswide_off = false;
    bool global = false;
#if defined(DRRUN) || defined(DRINJECT)
    char *pidfile = NULL;
    bool showstats = false;
    bool showmem = false;
    bool force_injection = false;
    bool inject = true;
    int limit = 0; /* in seconds */
    bool debugger_key_injection = false;
    char *app_name;
    char full_app_name[MAXIMUM_PATH];
    char app_cmdline[MAX_CMDLINE];
    char custom_dll[MAXIMUM_PATH];
    int errcode;
    void *inject_data;
    int wait_result;
    time_t start_time, end_time;
    bool success;
    bool exit0;
#endif
    int i;
#ifndef DRINJECT
    size_t j;
#endif
    char buf[MAXIMUM_PATH];
    char default_root[MAXIMUM_PATH];
    char *c;
    char *drlib_path = NULL;

    memset(client_paths, 0, sizeof(client_paths));

    /* default root: we assume this tool is in <root>/bin{32,64}/dr*.exe */
    GetFullPathName(argv[0], BUFFER_SIZE_ELEMENTS(buf), buf, NULL);
    NULL_TERMINATE_BUFFER(buf);
    c = buf + strlen(buf) - 1;
    while (*c != '\\' && *c != '/' && c > buf)
        c--;
    _snprintf(c+1, BUFFER_SIZE_ELEMENTS(buf) - (c+1-buf), "..");
    NULL_TERMINATE_BUFFER(buf);
    GetFullPathName(buf, BUFFER_SIZE_ELEMENTS(default_root), default_root, NULL);
    NULL_TERMINATE_BUFFER(default_root);
    dr_root = default_root;
    info("default root: %s", default_root);

    /* parse command line */
    for (i=1; i<argc; i++) {

        /* params with no arg */
        if (strcmp(argv[i], "-verbose") == 0) {
            verbose = true;
            continue;
        }
        else if (strcmp(argv[i], "-quiet") == 0) {
            quiet = true;
            continue;
        }
        else if (strcmp(argv[i], "-debug") == 0) {
            use_debug = true;
            continue;
        }
        else if (!strcmp(argv[i], "-v")) {
#if defined(BUILD_NUMBER) && defined(VERSION_NUMBER)
          printf(TOOLNAME" version %s -- build %d\n", STRINGIFY(VERSION_NUMBER), BUILD_NUMBER);
#elif defined(BUILD_NUMBER)
          printf(TOOLNAME" custom build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
          printf(TOOLNAME" custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
          exit(0);
        }
#ifdef DRCONFIG
        else if (!strcmp(argv[i], "-list_registered")) {
            action = action_list;
            list_registered = true;
            continue;
        }
        else if (strcmp(argv[i], "-syswide_on") == 0) {
            syswide_on = true;
            continue;
        }
        else if (strcmp(argv[i], "-syswide_off") == 0) {
            syswide_off = true;
            continue;
        }
        else if (strcmp(argv[i], "-global") == 0) {
            global = true;
            continue;
        }
#endif
        else if (strcmp(argv[i], "-32") == 0) {
	    dr_platform = DR_PLATFORM_32BIT;
            continue;
	}
        else if (strcmp(argv[i], "-64") == 0) {
	    dr_platform = DR_PLATFORM_64BIT;
            continue;
        }
#if defined(DRRUN) || defined(DRINJECT)
        else if (strcmp(argv[i], "-stats") == 0) {
	    showstats = true;
            continue;
        }
        else if (strcmp(argv[i], "-mem") == 0) {
	    showmem = true;
            continue;
        }
        else if (strcmp(argv[i], "-no_inject") == 0 ||
                 /* support old drinjectx param name */
                 strcmp(argv[i], "-noinject") == 0) {
            DR_dll_not_needed = true;
	    inject = false;
            continue;
        }
        else if (strcmp(argv[i], "-force") == 0) {
	    force_injection = true;
            continue;
        }
        else if (strcmp(argv[i], "-no_wait") == 0) {
            limit = -1;
            continue;
        }
        else if (strcmp(argv[i], "-exit0") == 0) {
            exit0 = true;
            continue;
        }
#endif
        /* all other flags have an argument -- make sure it exists */
        else if (argv[i][0] == '-' && i == argc - 1) {
            usage("invalid arguments");
        }

        /* params with an arg */
        if (strcmp(argv[i], "-root") == 0) {
            dr_root = argv[++i];
        }
#ifdef DRCONFIG
        else if (strcmp(argv[i], "-reg") == 0) {
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
#endif
#if defined(DRCONFIG) || defined(DRRUN)
# if defined(MF_API) || defined(PROBE_API)
        else if (strcmp(argv[i], "-mode") == 0) {
            char *mode_str = argv[++i];
            if (strcmp(mode_str, "code") == 0) {
                dr_mode = DR_MODE_CODE_MANIPULATION;
            }
#  ifdef MF_API
            else if (strcmp(mode_str, "security") == 0) {
                dr_mode = DR_MODE_MEMORY_FIREWALL;
            }
#  endif
#  ifdef PROBE_API
            else if (strcmp(mode_str, "probe") == 0) {
                dr_mode = DR_MODE_PROBE;
            }
#  endif
            else {
                usage("unknown mode: %s", mode_str);
            }
        }
# endif
        else if (strcmp(argv[i], "-client") == 0) {
            if (num_clients == MAX_CLIENT_LIBS) {
                fatal("Maximum number of clients is %d", MAX_CLIENT_LIBS);
            }
            else {
                if (i + 3 >= argc) {
                    usage("too few arguments to -client");
                }

                /* Support relative client paths: very useful! */
                GetFullPathName(argv[++i],
                                BUFFER_SIZE_ELEMENTS(client_paths[num_clients]),
                                (char *)client_paths[num_clients], NULL);
                NULL_TERMINATE_BUFFER(client_paths[num_clients]);
                info("client %d path: %s", num_clients, client_paths[num_clients]);
                client_ids[num_clients] = strtoul(argv[++i], NULL, 16);
                client_options[num_clients] = argv[++i];
                num_clients++;
            }
        }
        else if (strcmp(argv[i], "-ops") == 0) {
            extra_ops = argv[++i];
	}
#endif
#if defined(DRRUN) || defined(DRINJECT)
        else if (strcmp(argv[i], "-pidfile") == 0) {
            pidfile = argv[++i];
	}
        else if (strcmp(argv[i], "-use_dll") == 0) {
            DR_dll_not_needed = true;
            /* Support relative path: very useful! */
            GetFullPathName(argv[++i], BUFFER_SIZE_ELEMENTS(custom_dll),
                            custom_dll, NULL);
            NULL_TERMINATE_BUFFER(custom_dll);
            drlib_path = custom_dll;
	}
        else if (strcmp(argv[i], "-s") == 0) {
            limit = atoi(argv[++i]);
            if (limit <= 0)
                usage("invalid time");
	}
        else if (strcmp(argv[i], "-m") == 0) {
            limit = atoi(argv[++i])*60;
            if (limit <= 0)
                usage("invalid time");
	}
        else if (strcmp(argv[i], "-h") == 0) {
            limit = atoi(argv[++i])*3600;
            if (limit <= 0)
                usage("invalid time");
	}
#endif
        else {
#ifdef DRCONFIG
            usage("unknown option: %s", argv[i]);
#else
            /* start of app and its args */
            break;
#endif
        }
    }

#if defined(DRRUN) || defined(DRINJECT)
    if (i >= argc)
        usage("%s", "no app specified");
    app_name = argv[i++];
    _searchenv(app_name, "PATH", full_app_name);
    NULL_TERMINATE_BUFFER(full_app_name);
    if (full_app_name[0] == '\0') {
        /* may need to append .exe, FIXME : other executable types */
        char tmp_buf[MAXIMUM_PATH];
        _snprintf(tmp_buf, BUFFER_SIZE_ELEMENTS(tmp_buf), 
                  "%s%s", app_name, ".exe");
        NULL_TERMINATE_BUFFER(tmp_buf);
        _searchenv(tmp_buf, "PATH", full_app_name);
    }
    if (full_app_name[0] == '\0') {
        /* last try */
        GetFullPathName(app_name, BUFFER_SIZE_ELEMENTS(full_app_name),
                        full_app_name, NULL);
        NULL_TERMINATE_BUFFER(full_app_name);
    }
    if (full_app_name[0] != '\0')
        app_name = full_app_name;
    info("targeting application: \"%s\"", app_name);

    /* note that we want target app name as part of cmd line
     * (FYI: if we were using WinMain, the pzsCmdLine passed in
     *  does not have our own app name in it)
     * it's easier to construct than to call GetCommandLine() and then
     * remove our own args.
     */
    c = app_cmdline;
    c += _snprintf(c, BUFFER_SIZE_ELEMENTS(app_cmdline) - (c - app_cmdline),
                   "\"%s\"", app_name);
    for (; i < argc; i++) {
        c += _snprintf(c, BUFFER_SIZE_ELEMENTS(app_cmdline) - (c - app_cmdline),
                       " \"%s\"", argv[i]);
    }
    NULL_TERMINATE_BUFFER(app_cmdline);
    assert(c - app_cmdline < BUFFER_SIZE_ELEMENTS(app_cmdline));
    info("app cmdline: %s", app_cmdline);
#endif

    /* PR 244206: set the registry view before any registry access */
    set_dr_platform(dr_platform);

#ifdef DRCONFIG
    if (action == action_register) {
        register_proc(process, 0, global, dr_root, dr_mode,
                      use_debug, dr_platform, extra_ops);
        for (j=0; j<num_clients; j++) {
            register_client(process, 0, global, dr_platform, client_ids[j],
                            (char *)client_paths[j], client_options[j]);
        }
    }
    else if (action == action_unregister) {
        unregister_proc(process, 0, global, dr_platform);
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
            list_process(process, global, dr_platform, NULL);
        else /* list all */ {
            dr_registered_process_iterator_t *iter =
                dr_registered_process_iterator_start(dr_platform, global);
            printf("Registered %s processes for %s\n",
                   global ? "global" : "local", platform_name(dr_platform));
            while (dr_registered_process_iterator_hasnext(iter))
                list_process(NULL, global, dr_platform, iter);
            dr_registered_process_iterator_stop(iter);
        }
    }
    else if (!syswide_on && !syswide_off) {
        usage("no action specified");
    }
    if (syswide_on) {
        check_dr_root(dr_root, false, dr_platform, true);
        /* If this is the first setting of AppInit on NT, warn about reboot */
        if (!is_autoinjection_set()) {
            DWORD platform;
            if (get_platform(&platform) == ERROR_SUCCESS &&
                platform == PLATFORM_WIN_NT_4) {
                warn("on Windows NT, applications will not be taken over until reboot");
            }
        }
        if (dr_register_syswide(dr_platform, dr_root) != ERROR_SUCCESS) {
            /* PR 233108: try to give more info on whether a privilege failure */
            warn("syswide set failed: re-run as administrator");
        }
    }
    if (syswide_off) {
        if (dr_unregister_syswide(dr_platform) != ERROR_SUCCESS) {
            /* PR 233108: try to give more info on whether a privilege failure */
            warn("syswide set failed: re-run as administrator");
        }
    }
    return 0;
#else /* DRCONFIG */
    errcode = dr_inject_process_create(app_name, app_cmdline, &inject_data);
    if (errcode != 0) {
        int sofar = _snprintf(app_cmdline, BUFFER_SIZE_ELEMENTS(app_cmdline), 
                              "Failed to create process for \"%s\": ", app_name);
        if (sofar > 0) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPTSTR) app_cmdline + sofar,
                          BUFFER_SIZE_ELEMENTS(app_cmdline) - sofar*sizeof(char), NULL);
        }
        NULL_TERMINATE_BUFFER(app_cmdline);
        fatal("%s", app_cmdline);
        goto error;
    }

    /* i#200/PR 459481: communicate child pid via file */
    if (pidfile != NULL)
        write_pid_to_file(pidfile, dr_inject_get_process_id(inject_data));

# ifdef DRRUN
    if (inject) {
        process = dr_inject_get_image_name(inject_data);
        register_proc(process, dr_inject_get_process_id(inject_data), global,
                      dr_root, dr_mode, use_debug, dr_platform, extra_ops);
        for (j=0; j<num_clients; j++) {
            register_client(process, dr_inject_get_process_id(inject_data), global,
                            dr_platform, client_ids[j],
                            (char *)client_paths[j], client_options[j]);
        }
    }
# endif

    if (inject && !dr_inject_process_inject(inject_data, force_injection, drlib_path)) {
        fatal("unable to inject: did you forget to run drconfig first?");
        goto error; /* actually won't get here */
    }

    success = FALSE;
    start_time = time(NULL);

    dr_inject_process_run(inject_data);

    if (limit == 0 && dr_inject_using_debug_key(inject_data)) {
        info("%s", "Using debugger key injection");
        limit = -1; /* no wait */
    }

    if (limit >= 0) {
        double wallclock;
        int exitcode;
        info("waiting %sfor app to exit...", (limit <= 0) ? "forever " : "");
        wait_result = WaitForSingleObject(dr_inject_get_process_handle(inject_data),
                                          (limit==0) ? INFINITE : (limit*1000));
        end_time = time(NULL);
        wallclock = difftime(end_time, start_time);
        if (wait_result == WAIT_OBJECT_0)
            success = TRUE;
        else
            info("timeout after %d seconds\n", limit);

        if (showstats || showmem)
            dr_inject_print_stats(inject_data, (int) wallclock, showstats, showmem);
        exitcode = dr_inject_process_exit(inject_data, !success/*kill process*/);
        if (exit0)
            exitcode = 0;
        return exitcode;
    } else {
        /* if we are using env -> registry our changes won't get undone!
         * we can't unset now, the app may still reference them */
        success = TRUE;
        return 0;
    }
 error:
    dr_inject_process_exit(inject_data, false);
    return 1;
#endif /* !DRCONFIG */
}

