/* ***************************************************************************
 * Copyright (c) 2012-2016 Google, Inc.  All rights reserved.
 * ***************************************************************************/

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

/* drcov: DynamoRIO Code Coverage Tool
 *
 * Collects information about basic blocks that have been executed.
 * It simply stores the information of basic blocks seen in bb callback event
 * into a table without any instrumentation, and dumps the buffer into log files
 * on thread/process exit.
 * To collect per-thread basic block execution information, run DR with
 * a thread private code cache (i.e., -thread_private).
 *
 * The runtime options for this client include:
 * -dump_text         Dumps the log file in text format
 * -dump_binary       Dumps the log file in binary format
 * -[no_]nudge_kills  On by default.
 *                    Uses nudge to notify a child process being terminated
 *                    by its parent, so that the exit event will be called.
 * -logdir <dir>      Sets log directory, which by default is ".".
 */

#include "dr_api.h"
#include "drx.h"
#include "drcovlib.h"
#include "../common/utils.h"
#include <string.h>

static uint verbose;
static bool nudge_kills;
static client_id_t client_id;

#define NOTIFY(level, ...)                   \
    do {                                     \
        if (verbose >= (level))              \
            dr_fprintf(STDERR, __VA_ARGS__); \
    } while (0)

#define OPTION_MAX_LENGTH MAXIMUM_PATH

/****************************************************************************
 * Nudges
 */

enum {
    NUDGE_TERMINATE_PROCESS = 1,
};

static void
event_nudge(void *drcontext, uint64 argument)
{
    int nudge_arg = (int)argument;
    int exit_arg = (int)(argument >> 32);
    if (nudge_arg == NUDGE_TERMINATE_PROCESS) {
        static int nudge_term_count;
        /* handle multiple from both NtTerminateProcess and NtTerminateJobObject */
        uint count = dr_atomic_add32_return_sum(&nudge_term_count, 1);
        if (count == 1) {
            dr_exit_process(exit_arg);
        }
    }
    ASSERT(nudge_arg == NUDGE_TERMINATE_PROCESS, "unsupported nudge");
    ASSERT(false, "should not reach"); /* should not reach */
}

static bool
event_soft_kill(process_id_t pid, int exit_code)
{
    /* we pass [exit_code, NUDGE_TERMINATE_PROCESS] to target process */
    dr_config_status_t res;
    res = dr_nudge_client_ex(pid, client_id,
                             NUDGE_TERMINATE_PROCESS | (uint64)exit_code << 32, 0);
    if (res == DR_SUCCESS) {
        /* skip syscall since target will terminate itself */
        return true;
    }
    /* else failed b/c target not under DR control or maybe some other
     * error: let syscall go through
     */
    return false;
}

/****************************************************************************
 * Event Callbacks
 */

static void
event_exit(void)
{
    drcovlib_exit();
}

static void
options_init(client_id_t id, int argc, const char *argv[], drcovlib_options_t *ops)
{
    int i;
    const char *token;
    /* default values */
    nudge_kills = true;

    for (i = 1 /*skip client*/; i < argc; i++) {
        token = argv[i];
        if (strcmp(token, "-dump_text") == 0)
            ops->flags |= DRCOVLIB_DUMP_AS_TEXT;
        else if (strcmp(token, "-dump_binary") == 0)
            ops->flags &= ~DRCOVLIB_DUMP_AS_TEXT;
        else if (strcmp(token, "-no_nudge_kills") == 0)
            nudge_kills = false;
        else if (strcmp(token, "-nudge_kills") == 0)
            nudge_kills = true;
        else if (strcmp(token, "-logdir") == 0) {
            USAGE_CHECK((i + 1) < argc, "missing logdir path");
            ops->logdir = argv[++i];
        } else if (strcmp(token, "-logprefix") == 0) {
            USAGE_CHECK((i + 1) < argc, "missing logprefix string");
            ops->logprefix = argv[++i];
        } else if (strcmp(token, "-native_until_thread") == 0) {
            USAGE_CHECK((i + 1) < argc, "missing -native_until_thread number");
            token = argv[++i];
            if (dr_sscanf(token, "%d", &ops->native_until_thread) != 1 ||
                ops->native_until_thread < 0) {
                ops->native_until_thread = 0;
                USAGE_CHECK(false, "invalid -native_until_thread number");
            }
        } else if (strcmp(token, "-verbose") == 0) {
            /* XXX: should drcovlib expose its internal verbose param? */
            USAGE_CHECK((i + 1) < argc, "missing -verbose number");
            token = argv[++i];
            if (dr_sscanf(token, "%u", &verbose) != 1) {
                USAGE_CHECK(false, "invalid -verbose number");
            }
        } else {
            NOTIFY(0, "UNRECOGNIZED OPTION: \"%s\"\n", token);
            USAGE_CHECK(false, "invalid option");
        }
    }
    if (dr_using_all_private_caches())
        ops->flags |= DRCOVLIB_THREAD_PRIVATE;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    drcovlib_options_t ops = {
        sizeof(ops),
    };
    dr_set_client_name("DrCov", "http://dynamorio.org/issues");
    client_id = id;

    options_init(id, argc, argv, &ops);
    if (drcovlib_init(&ops) != DRCOVLIB_SUCCESS) {
        NOTIFY(0, "fatal error: drcovlib failed to initialize\n");
        dr_abort();
    }
    if (!dr_using_all_private_caches()) {
        const char *logname;
        if (drcovlib_logfile(NULL, &logname) == DRCOVLIB_SUCCESS)
            NOTIFY(1, "<created log file %s>\n", logname);
    }

    if (nudge_kills) {
        drx_register_soft_kills(event_soft_kill);
        dr_register_nudge_event(event_nudge, id);
    }

    dr_register_exit_event(event_exit);
}
