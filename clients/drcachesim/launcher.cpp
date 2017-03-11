/* **********************************************************
 * Copyright (c) 2015-2017 Google, Inc.  All rights reserved.
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

/* launcher: the front end for the trace analyzer */

/* For internationalization support we use wide-char versions of all Windows-
 * facing routines and convert to UTF-8 for DR API routines.
 */
#ifdef WINDOWS
# define UNICODE
# define _UNICODE
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>  /* for fork */
#endif

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include "analyzer_multi.h"
#include "dr_api.h"
#include "dr_inject.h"
#include "dr_config.h"
#include "dr_frontend.h"
#include "droption.h"
#include "common/options.h"
#include "common/utils.h"

#define FATAL_ERROR(msg, ...) do { \
    fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__);    \
    fflush(stderr); \
    exit(1); \
} while (0)

#define NOTIFY(level, prefix, msg, ...) do {     \
    if (op_verbose.get_value() >= level) {                                   \
        fprintf(stderr, "%s: " msg "\n", prefix, ##__VA_ARGS__);        \
        fflush(stderr); \
    } \
} while (0)

#define CLIENT_ID 0

static bool
file_is_readable(const char *path)
{
    bool ret = false;
    return (drfront_access(path, DRFRONT_READ, &ret) == DRFRONT_SUCCESS && ret);
}

static bool
file_is_writable(const char *path)
{
    bool ret = false;
    return (drfront_access(path, DRFRONT_WRITE, &ret) == DRFRONT_SUCCESS && ret);
}

static void
get_full_path(const char *app, char *buf, size_t buflen/*# elements*/)
{
    drfront_status_t sc = drfront_get_app_full_path(app, buf, buflen);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("drfront_get_app_full_path failed on %s: %d\n", app, sc);
}

static bool
configure_application(char *app_name, char **app_argv,
                      std::string tracer_ops, void **inject_data)
{
    int errcode;
    char *process;
    process_id_t pid;

#ifdef UNIX
    errcode = dr_inject_prepare_to_exec(app_name, (const char **)app_argv, inject_data);
#else
    errcode = dr_inject_process_create(app_name, (const char **)app_argv, inject_data);
#endif
    if (errcode != 0 && errcode != WARN_IMAGE_MACHINE_TYPE_MISMATCH_EXE) {
        std::string msg =
            std::string("failed to create process for \"") + app_name + "\"";
#ifdef WINDOWS
        char buf[MAXIMUM_PATH];
        int sofar = _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s", msg.c_str());
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) buf + sofar,
                      BUFFER_SIZE_ELEMENTS(buf) - sofar*sizeof(char), NULL);
#endif
        FATAL_ERROR("%s", msg.c_str());
    }

    pid = dr_inject_get_process_id(*inject_data);

    process = dr_inject_get_image_name(*inject_data);
    NOTIFY(1, "INFO", "configuring %s pid=%d dr_ops=\"%s\"", process, pid,
           op_dr_ops.get_value().c_str());
    if (dr_register_process(process, pid,
                            false/*local*/, op_dr_root.get_value().c_str(),
                            DR_MODE_CODE_MANIPULATION,
                            op_dr_debug.get_value(), DR_PLATFORM_DEFAULT,
                            op_dr_ops.get_value().c_str()) != DR_SUCCESS) {
        FATAL_ERROR("failed to register DynamoRIO configuration");
    }
    NOTIFY(1, "INFO", "configuring client \"%s\" ops=\"%s\"",
           op_tracer.get_value().c_str(), tracer_ops.c_str());
    if (dr_register_client(process, pid,
                           false/*local*/, DR_PLATFORM_DEFAULT, CLIENT_ID,
                           0, op_tracer.get_value().c_str(),
                           tracer_ops.c_str()) != DR_SUCCESS) {
        FATAL_ERROR("failed to register DynamoRIO client configuration");
    }
    return true;
}

int
_tmain(int argc, const TCHAR *targv[])
{
    char **argv;
    char *app_name = NULL;
    char full_app_name[MAXIMUM_PATH];
    char **app_argv = NULL;
    int app_idx = 1;
    int errcode = 1;
    void *inject_data = NULL;
    char buf[MAXIMUM_PATH];
    drfront_status_t sc;
    bool is64, is32;
    analyzer_t *analyzer = NULL;
    std::string tracer_ops;
#ifdef UNIX
    pid_t child = 0;
#endif
    bool have_trace_file;

#if defined(WINDOWS) && !defined(_UNICODE)
# error _UNICODE must be defined
#else
    /* Convert to UTF-8 */
    sc = drfront_convert_args(targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("failed to process args: %d\n", sc);
#endif

    // This frontend exists mainly because we have a standalone application
    // to launch, the analyzer.  We are not currently looking for a polished
    // tool launcher independent of drrun.  Thus, we skip all the logic around
    // default root and client directories and assume we were invoked from
    // a pre-configured .drrun file with proper paths.

    std::string parse_err;
    if (!droption_parser_t::parse_argv(DROPTION_SCOPE_FRONTEND, argc,
                                       (const char **) argv,
                                       &parse_err, &app_idx)) {
        // We try to support no "--" separating the app
        if (argv[app_idx] != NULL && argv[app_idx][0] != '-') {
            // Treat as the app name
        } else {
            FATAL_ERROR("Usage error: %s\nUsage:\n%s", parse_err.c_str(),
                        droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        }
    }
    have_trace_file = !op_infile.get_value().empty() || !op_indir.get_value().empty();

    if (!have_trace_file) {
        if (app_idx >= argc) {
            FATAL_ERROR("Usage error: no application specified\nUsage:\n%s",
                        droption_parser_t::usage_short(DROPTION_SCOPE_ALL).c_str());
        }
        app_name = argv[app_idx];
        get_full_path(app_name, full_app_name, BUFFER_SIZE_ELEMENTS(full_app_name));
        if (full_app_name[0] != '\0')
            app_name = full_app_name;
        NOTIFY(1, "INFO", "targeting application: \"%s\"", app_name);
        if (!file_is_readable(full_app_name)) {
            FATAL_ERROR("cannot find application %s", full_app_name);
        }

        if (drfront_is_64bit_app(app_name, &is64, &is32) == DRFRONT_SUCCESS &&
            IF_X64_ELSE(!is64, is64 && !is32)) {
            /* FIXME i#1703: since drinjectlib doesn't support cross-arch
             * injection (DRi#803), we need to launch the other frontend.
             */
            FATAL_ERROR("application has bitwidth unsupported by this launcher");
        }

        app_argv = &argv[app_idx];

        if (!file_is_readable(op_tracer.get_value().c_str())) {
            FATAL_ERROR("tracer library %s is unreadable", op_tracer.get_value().c_str());
        }
        if (!file_is_readable(op_dr_root.get_value().c_str())) {
            FATAL_ERROR("invalid -dr_root %s", op_dr_root.get_value().c_str());
        }
    }

    if (op_offline.get_value() && !have_trace_file) {
        // Initial sanity check: may still be unwritable by this user, but this
        // serves as at least an existence check.
        if (!file_is_writable(op_outdir.get_value().c_str())) {
            FATAL_ERROR("invalid -outdir %s", op_outdir.get_value().c_str());
        }
    } else {
        analyzer = new analyzer_multi_t;
        if (!*analyzer) {
            FATAL_ERROR("failed to initialize analyzer");
        }
    }

    tracer_ops = op_tracer_ops.get_value();

    if (!have_trace_file) {
        /* i#1638: fall back to temp dirs if there's no HOME/USERPROFILE set */
        dr_get_config_dir(false/*local*/, true/*use temp*/, buf,
                          BUFFER_SIZE_ELEMENTS(buf));
        NOTIFY(1, "INFO", "DynamoRIO configuration directory is %s", buf);

#ifdef UNIX
        if (op_offline.get_value())
            child = 0;
        else
            child = fork();
        if (child < 0) {
            FATAL_ERROR("failed to fork");
            assert(false); // won't get here
        } else if (child == 0) {
            /* child, or offline where we exec this process */
            if (!configure_application(app_name, app_argv, tracer_ops, &inject_data) ||
                !dr_inject_process_inject(inject_data, false/*!force*/, NULL)) {
                FATAL_ERROR("unable to inject");
                assert(false); // won't get here
            }
            FATAL_ERROR("failed to exec application");
        }
        /* parent */
#else
        if (!configure_application(app_name, app_argv, tracer_ops, &inject_data) ||
            !dr_inject_process_inject(inject_data, false/*!force*/, NULL)) {
            FATAL_ERROR("unable to inject");
        }
        dr_inject_process_run(inject_data);
#endif
    }

    if (!op_offline.get_value() || have_trace_file) {
        if (!analyzer->run()) {
            FATAL_ERROR("failed to run analyzer");
        }
    }

    if (!have_trace_file) {
#ifdef WINDOWS
        NOTIFY(1, "INFO", "waiting for app to exit...");
        errcode = WaitForSingleObject(dr_inject_get_process_handle(inject_data),
                                      INFINITE);
        if (errcode != WAIT_OBJECT_0)
            NOTIFY(1, "INFO", "failed to wait for app: %d\n", errcode);
        errcode = dr_inject_process_exit(inject_data, false/*don't kill process*/);
#else
# ifndef NDEBUG
        pid_t result =
# endif
            waitpid(child, &errcode, 0);
        assert(result == child);
#endif

        if (!op_offline.get_value()) { // Skipping for offline to match UNIX.
            // XXX: we may want a prefix on our output
            std::cerr << "---- <application exited with code " << errcode <<
                "> ----" << std::endl;
        }
    } else
        errcode = 0;

    analyzer->print_stats();

    // release analyzer's space
    delete analyzer;

    sc = drfront_cleanup_args(argv, argc);
    if (sc != DRFRONT_SUCCESS)
        FATAL_ERROR("drfront_cleanup_args failed: %d\n", sc);
    return errcode;
}
