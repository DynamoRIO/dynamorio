/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2008-2010 VMware, Inc.  All rights reserved.
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

#include "configure.h"

#ifdef WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    define UNICODE
#    define _UNICODE
#    include <windows.h>
#    include <io.h>
#    include "config.h"
#    include "share.h"
#endif

#ifdef UNIX
#    include <errno.h>
#    include <fcntl.h>
#    include <unistd.h>
#    include <sys/stat.h>
#    include <sys/mman.h>
#    include <sys/wait.h>
#endif

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include "globals_shared.h"
#include "dr_config.h" /* MUST be before share.h (it sets HOT_PATCHING_INTERFACE) */
#include "dr_inject.h"
#include "dr_frontend.h"

typedef enum _action_t {
    action_none,
    action_nudge,
    action_register,
    action_unregister,
    action_list,
} action_t;

static bool verbose;
static bool quiet;
static bool DR_dll_not_needed = false;
static bool nocheck;

#define die() exit(1)

#define fatal(msg, ...)                                     \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
        exit(1);                                            \
    } while (0)

/* up to caller to call die() if necessary */
#define error(msg, ...)                                     \
    do {                                                    \
        fprintf(stderr, "ERROR: " msg "\n", ##__VA_ARGS__); \
        fflush(stderr);                                     \
    } while (0)

#define warn(msg, ...)                                            \
    do {                                                          \
        if (!quiet) {                                             \
            fprintf(stderr, "WARNING: " msg "\n", ##__VA_ARGS__); \
            fflush(stderr);                                       \
        }                                                         \
    } while (0)

#define info(msg, ...)                                         \
    do {                                                       \
        if (verbose) {                                         \
            fprintf(stderr, "INFO: " msg "\n", ##__VA_ARGS__); \
            fflush(stderr);                                    \
        }                                                      \
    } while (0)

#ifdef DRCONFIG
#    define TOOLNAME "drconfig"
#elif defined(DRRUN)
#    define TOOLNAME "drrun"
#elif defined(DRINJECT)
#    define TOOLNAME "drinject"
#endif

const char *usage_str =
#ifdef DRCONFIG
    "USAGE: " TOOLNAME " [options]\n"
    "   or: " TOOLNAME " [options] [-ops \"<DR options>\"] -c <client> [client options]\n"
    "   or: " TOOLNAME " [options] [-ops \"<DR options>\"] -t <tool> [tool options]\n";
#elif defined(DRRUN) || defined(DRINJECT)
    "USAGE: " TOOLNAME " [options] <app and args to run>\n"
    "   or: " TOOLNAME " [options] -- <app and args to run>\n"
#    if defined(DRRUN)
    "   or: " TOOLNAME " [options] [DR options] -- <app and args to run>\n"
    "   or: " TOOLNAME " [options] [DR options] -c <client> [client options]"
    " -- <app and args to run>\n"
    "   or: " TOOLNAME " [options] [DR options] -t <tool> [tool options]"
    " -- <app and args to run>\n"
    "   or: " TOOLNAME " [options] [DR options] -c32 <32-bit-client> [client options]"
    " -- -c64 <64-bit-client> [client options] -- <app and args to run>\n"
#    endif
    ;
#endif

const char *options_list_str =
    "\n" TOOLNAME " options (these are distinct from DR runtime options):\n"
    "       -version           Display version information\n"
    "       -verbose           Display additional information\n"
    "       -quiet             Do not display warnings\n"
    "       -nocheck           Do not fail due to invalid DynamoRIO installation or app\n"
#ifdef DRCONFIG
    "       -reg <process>     Register <process> to run under DR\n"
    "       -unreg <process>   Unregister <process> from running under DR\n"
    "       -isreg <process>   Display whether <process> is registered and if so its\n"
    "                          configuration\n"
#    ifdef WINDOWS
    "       -list_registered   Display all registered processes and their configuration\n"
#    endif /* WINDOWS */
#endif     /* DRCONFIG */
    "       -root <root>       DR root directory\n"
#if defined(DRCONFIG) || defined(DRRUN)
#    if defined(MF_API) && defined(PROBE_API)
    "       -mode <mode>       DR mode (code, probe, or security)\n"
#    elif defined(PROBE_API)
    "       -mode <mode>       DR mode (code or probe)\n"
#    elif defined(MF_API)
    "       -mode <mode>       DR mode (code or security)\n"
#    else
/* No mode argument, is always code. */
#    endif
#endif
#ifdef DRCONFIG
/* FIXME i#840: Syswide NYI on Linux. */
#    ifdef WINDOWS
    "       -syswide_on        Set up systemwide injection so that registered\n"
    "                          applications will run under DR however they are\n"
    "                          launched.  Otherwise, drinject must be used to\n"
    "                          launch a target configured application under DR.\n"
    "                          This option requires administrative privileges.\n"
    "       -syswide_off       Disable systemwide injection.\n"
    "                          This option requires administrative privileges.\n"
#    endif
    "       -global            Use global configuration files instead of local\n"
    "                          user-private configuration files.  The global\n"
    "                          config dir must be set up ahead of time.\n"
    "                          This option may require administrative privileges.\n"
    "                          If a local file already exists it will take precedence.\n"
    "       -norun             Create a configuration that excludes the application\n"
    "                          from running under DR control.  Useful for following\n"
    "                          all child processes except a handful (blocklist).\n"
#endif
    "       -debug             Use the DR debug library\n"
    "       -32                Target 32-bit or WOW64 applications\n"
    "       -64                Target 64-bit (non-WOW64) applications\n"
#if defined(DRCONFIG) || defined(DRRUN)
    "\n"
    "       -ops \"<options>\"   Specify DR runtime options.  When specifying\n"
    "                          multiple options, enclose the entire list of\n"
    "                          options in quotes, or repeat the -ops.\n"
    "                          Alternatively, if the application is separated\n"
    "                          by \"--\" or if -c or -t is specified, the -ops may be\n"
    "                          omitted and DR options listed prior to \"--\", -c,\n"
    "                          and -t, without quotes.\n"
    "\n"
    "        -t <toolname>      Registers a pre-configured tool to run alongside DR.\n"
    "                           A tool is a client with a configuration file\n"
    "                           that sets the client options and path, providing a\n"
    "                           convenient launching command via this -t parameter.\n"
#    ifdef DRRUN
    "                           Available tools include: %s.\n"
#    endif
    "\n"
    "        -c <path> <options>*\n"
    "                           Registers one client to run alongside DR.  Assigns\n"
    "                           the client an id of 0.  All remaining arguments\n"
    "                           until the -- arg before the app are interpreted as\n"
    "                           client options.  Must come after all drrun and DR\n"
    "                           ops.  Incompatible with -client.  Requires using --\n"
    "                           to separate the app executable.  Neither the path nor\n"
    "                           the options may contain semicolon characters or\n"
    "                           all 3 quote characters (\", \', `).\n"
    "\n"
    "        -c32 <32-bit-path> <options>* -- -c64 <64-bit-path> <options>*\n"
    "                           Registers two versions of one client to run alongside\n"
    "                           DR.  Use this to specify two versions of a client to\n"
    "                           handle applications which create other-bitwidth child\n"
    "                           processes.  The options for the 32-bit client are\n"
    "                           separated from the \"-c64\" by \"--\".  The behavior\n"
    "                           otherwise matches \"-c\".\n"
    "\n"
    "       -client <path> <ID> \"<options>\"\n"
    "                          Use -c instead, unless you need to set the client ID.\n"
    "                          Registers one or more clients to run alongside DR.\n"
    "                          This option is only valid when registering a\n"
    "                          process.  The -client option takes three arguments:\n"
    "                          the full path to a client library, a unique 8-digit\n"
    "                          hex ID, and an optional list of client options\n"
    "                          (use \"\" to specify no options).  Multiple clients\n"
    "                          can be installed via multiple -client options.  In\n"
    "                          this case, clients specified first on the command\n"
    "                          line have higher priority.  Neither the path nor\n"
    "                          the options may contain semicolon characters or\n"
    "                          all 3 quote characters (\", \', `).\n"
    "                          This option must precede any options to DynamoRIO.\n"
#endif
#ifdef DRCONFIG
    "\n"
    "       -detach <pid> \n"
    "                         Detach from the process with the given pid.\n"
    "\n"
#    ifdef WINDOWS
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
#    else  /* WINDOWS */
    /* FIXME i#840: integrate drnudgeunix into drconfig on Unix */
    "Note: please use the drnudgeunix tool to nudge processes on Unix.\n";
#    endif /* !WINDOWS */
#else      /* DRCONFIG */
    "       -no_wait           Return immediately: do not wait for application exit.\n"
    "       -s <seconds>       Kill the application if it runs longer than the\n"
    "                          specified number of seconds.\n"
    "       -m <minutes>       Kill the application if it runs longer than the\n"
    "                          specified number of minutes.\n"
    "       -h <hours>         Kill the application if it runs longer than the\n"
    "                          specified number of hours.\n"
#    ifdef UNIX
    "       -killpg            Create a new process group for the app.  If the app\n"
    "                          times out, kill the entire process group.  This forces\n"
    "                          the child to be a new process with a new pid, rather\n"
    "                          than reusing the parent's pid.\n"
#    endif
    "       -stats             Print /usr/bin/time-style elapsed time and memory used.\n"
    "       -mem               Print memory usage statistics.\n"
    "       -pidfile <file>    Print the pid of the child process to the given file.\n"
    "       -no_inject         Run the application natively.\n"
    "       -static            Do not inject under the assumption that the application\n"
    "                          is statically linked with DynamoRIO.  Instead, trigger\n"
    "                          automated takeover.\n"
#    ifndef MACOS /* XXX i#1285: private loader NYI on MacOS */
    "       -late              Requests late injection.\n"
#    endif
#    ifdef UNIX       /* FIXME i#725: Windows attach NYI */
#        ifndef MACOS /* XXX i#1285: private loader NYI on MacOS */
    "       -early             Requests early injection (the default).\n"
#        endif
    "       -logdir <dir>      Logfiles will be stored in this directory.\n"
#    endif
#    ifdef UNIX
    "       -attach <pid>      Attach to the process with the given pid.\n"
    "                          Attaching is an experimental feature and is not yet\n"
    "                          as well-supported as launching a new process.\n"
#    endif
#    ifdef WINDOWS
    "       -attach <pid>      Attach to the process with the given pid.\n"
    "                          Attaching is an experimental feature and is not yet\n"
    "                          as well-supported as launching a new process.\n"
    "                          Attaching to a process in the middle of a blocking\n"
    "                          system call could fail.\n"
    "                          Try takeover_sleep and larger takeovers to increase\n"
    "                          the chances of success:\n"
    "       -takeover_sleep    Sleep 1 millisecond between takeover attempts.\n"
    "       -takeovers <num>   Number of takeover attempts. Defaults to 8.\n"
    "                          The larger, the more likely attach will succeed,\n"
    "                          however, the attach process will take longer.\n"
#    endif
    "       -use_dll <dll>     Inject given dll instead of configured DR dll.\n"
    "       -use_alt_dll <dll> Use the given dll as the alternate-bitwidth DR dll.\n"
    "       -tool_dir <dir>    Directory containing tool configuration files.\n"
    "       -force             Inject regardless of configuration.\n"
    "       -exit0             Return a 0 exit code instead of the app's exit code.\n"
    "\n"
    "       <app and args>     Application command line to execute under DR.\n"
#endif /* !DRCONFIG */
    ;

static bool
does_file_exist(const char *path)
{
    bool ret = false;
    return (drfront_access(path, DRFRONT_EXIST, &ret) == DRFRONT_SUCCESS && ret);
}

#if defined(DRRUN) || defined(DRINJECT)
static bool
search_env(const char *fname, const char *env_var, char *full_path,
           const size_t full_path_size)
{
    bool ret = false;
    if (drfront_searchenv(fname, env_var, full_path, full_path_size, &ret) !=
            DRFRONT_SUCCESS ||
        !ret) {
        full_path[0] = '\0';
        return false;
    }
    return true;
}
#endif

#ifdef UNIX
#    ifndef DRCONFIG
static int
GetLastError(void)
{
    return errno;
}
#    endif /* DRCONFIG */
#endif     /* UNIX */

static void
get_absolute_path(const char *src, char *buf, size_t buflen /*# elements*/)
{
    drfront_status_t sc = drfront_get_absolute_path(src, buf, buflen);
    if (sc != DRFRONT_SUCCESS)
        fatal("failed (status=%d) to convert %s to an absolute path", sc, src);
}

/* Opens a filename and mode that are in utf8 */
static FILE *
fopen_utf8(const char *path, const char *mode)
{
#ifdef WINDOWS
    TCHAR wpath[MAXIMUM_PATH];
    TCHAR wmode[MAXIMUM_PATH];
    if (drfront_char_to_tchar(path, wpath, BUFFER_SIZE_ELEMENTS(wpath)) !=
            DRFRONT_SUCCESS ||
        drfront_char_to_tchar(mode, wmode, BUFFER_SIZE_ELEMENTS(wmode)) !=
            DRFRONT_SUCCESS)
        return NULL;
    return _tfopen(wpath, wmode);
#else
    return fopen(path, mode);
#endif
}

static char tool_list[MAXIMUM_PATH];

static void
print_tool_list(FILE *stream)
{
#ifdef DRRUN
    if (tool_list[0] != '\0')
        fprintf(stream, "       available tools include: %s\n", tool_list);
#endif
}

/* i#1509: we want to list the available tools for the -t option.
 * Since we don't have a dir iterator we use a list of tools
 * in a text file tools/list{32,64} which we create at
 * install time.  Thus we only expect to have it for a package build.
 */
static void
read_tool_list(const char *dr_toolconfig_dir, dr_platform_t dr_platform)
{
    FILE *f;
    char list_file[MAXIMUM_PATH];
    size_t sofar = 0;
    const char *arch = IF_X64_ELSE("64", "32");
    /* clear global tool list on re-read */
    tool_list[0] = '\0';

    if (dr_platform == DR_PLATFORM_32BIT)
        arch = "32";
    else if (dr_platform == DR_PLATFORM_64BIT)
        arch = "64";
    _snprintf(list_file, BUFFER_SIZE_ELEMENTS(list_file), "%s/list%s", dr_toolconfig_dir,
              arch);
    NULL_TERMINATE_BUFFER(list_file);
    f = fopen_utf8(list_file, "r");
    if (f == NULL) {
        /* no visible error: we only expect to have a list for a package build */
        return;
    }
    while (fgets(tool_list + sofar,
                 (int)(BUFFER_SIZE_ELEMENTS(tool_list) - sofar - 1 /*space*/),
                 f) != NULL) {
        NULL_TERMINATE_BUFFER(tool_list);
        sofar += strlen(tool_list + sofar);
        tool_list[sofar - 1] = ','; /* replace newline with comma */
        /* add space */
        if (sofar < BUFFER_SIZE_ELEMENTS(tool_list))
            tool_list[sofar++] = ' ';
    }
    fclose(f);
    tool_list[sofar - 2] = '\0';
    NULL_TERMINATE_BUFFER(tool_list);
}

#define usage(list_ops, msg, ...)                                                \
    do {                                                                         \
        FILE *stream = (list_ops == true) ? stdout : stderr;                     \
        if ((msg)[0] != '\0')                                                    \
            fprintf(stderr, "ERROR: " msg "\n\n", ##__VA_ARGS__);                \
        fprintf(stream, "%s", usage_str);                                        \
        print_tool_list(stream);                                                 \
        if (list_ops) {                                                          \
            fprintf(stream, options_list_str, tool_list);                        \
            exit(0);                                                             \
        } else {                                                                 \
            fprintf(stream, "Run with -help to see " TOOLNAME " option list\n"); \
        }                                                                        \
        die();                                                                   \
    } while (0)

/* Unregister a process */
bool
unregister_proc(const char *process, process_id_t pid, bool global,
                dr_platform_t dr_platform)
{
    dr_config_status_t status = dr_unregister_process(process, pid, global, dr_platform);
    if (status == DR_PROC_REG_INVALID) {
        error("no existing registration for %s", process == NULL ? "<null>" : process);
        return false;
    } else if (status == DR_FAILURE) {
        error("unregistration failed for %s", process == NULL ? "<null>" : process);
        return false;
    }
    return true;
}

/* Check if the provided root directory actually has the files we
 * expect.  Returns whether a fatal problem.  On success,
 * fills in dr_lib_path and dr_alt_lib_path if they are non-NULL.
 */
static bool
expand_dr_root(const char *dr_root, bool debug, dr_platform_t dr_platform, bool preinject,
               bool report, OUT char *dr_lib_path, size_t dr_lib_path_sz,
               OUT char *dr_alt_lib_path, size_t dr_alt_lib_path_sz)
{
    int i;
    char buf[MAXIMUM_PATH];
    bool ok = true;
    /* FIXME i#1569: port DynamoRIO to AArch64 so we can enable the check warning */
    bool nowarn = IF_X86_ELSE(false, true);

    typedef struct _file_entry_t {
        const char *suffix;
        bool is_core;
        bool is_debug;
        bool is_preinject;
        dr_platform_t platform;
    } file_entry_t;
    const file_entry_t checked_files[] = {
#ifdef WINDOWS
        { "lib32\\drpreinject.dll", false, false, true, DR_PLATFORM_32BIT },
        { "lib32\\release\\dynamorio.dll", true, false, false, DR_PLATFORM_32BIT },
        { "lib32\\debug\\dynamorio.dll", true, true, false, DR_PLATFORM_32BIT },
        { "lib64\\drpreinject.dll", false, false, true, DR_PLATFORM_64BIT },
        { "lib64\\release\\dynamorio.dll", true, false, false, DR_PLATFORM_64BIT },
        { "lib64\\debug\\dynamorio.dll", true, true, false, DR_PLATFORM_64BIT },
#elif defined(MACOS)
        { "lib32/debug/libdrpreload.dylib", false, true, true, DR_PLATFORM_32BIT },
        { "lib32/debug/libdynamorio.dylib", true, true, false, DR_PLATFORM_32BIT },
        { "lib32/release/libdrpreload.dylib", false, false, true, DR_PLATFORM_32BIT },
        { "lib32/release/libdynamorio.dylib", true, false, false, DR_PLATFORM_32BIT },
        { "lib64/debug/libdrpreload.dylib", true, false, true, DR_PLATFORM_64BIT },
        { "lib64/debug/libdynamorio.dylib", true, true, false, DR_PLATFORM_64BIT },
        { "lib64/release/libdrpreload.dylib", false, false, true, DR_PLATFORM_64BIT },
        { "lib64/release/libdynamorio.dylib", true, false, false, DR_PLATFORM_64BIT },
#else /* LINUX */
        /* With early injection the default, we don't require preload to exist. */
        { "lib32/debug/libdynamorio.so", true, true, false, DR_PLATFORM_32BIT },
        { "lib32/release/libdynamorio.so", true, false, false, DR_PLATFORM_32BIT },
        { "lib64/debug/libdynamorio.so", true, true, false, DR_PLATFORM_64BIT },
        { "lib64/release/libdynamorio.so", true, false, false, DR_PLATFORM_64BIT },
#endif
    };

    if (dr_platform == DR_PLATFORM_DEFAULT)
        dr_platform = IF_X64_ELSE(DR_PLATFORM_64BIT, DR_PLATFORM_32BIT);

    if (DR_dll_not_needed) {
        /* An explicit path was passed so don't require a regular installation. */
        nowarn = true;
    }
    /* don't warn if running from a build dir (i#458) which we attempt to detect
     * by looking for CMakeCache.txt in the root dir
     * (warnings can also be suppressed via -quiet)
     */
    _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s/%s", dr_root, "CMakeCache.txt");
    if (does_file_exist(buf))
        nowarn = true;

    bool found_lib = false;
    if (dr_alt_lib_path != NULL)
        dr_alt_lib_path[0] = '\0';
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(checked_files); i++) {
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%s/%s", dr_root,
                  checked_files[i].suffix);
        if (does_file_exist(buf)) {
            if (checked_files[i].is_core &&
                ((debug && checked_files[i].is_debug) ||
                 (!debug && !checked_files[i].is_debug))) {
                if (checked_files[i].platform == dr_platform) {
                    found_lib = true;
                    if (dr_lib_path != NULL) {
                        _snprintf(dr_lib_path, dr_lib_path_sz, "%s", buf);
                        dr_lib_path[dr_lib_path_sz - 1] = '\0';
                    }
                } else {
                    if (dr_alt_lib_path != NULL) {
                        _snprintf(dr_alt_lib_path, dr_alt_lib_path_sz, "%s", buf);
                        dr_alt_lib_path[dr_alt_lib_path_sz - 1] = '\0';
                    }
                }
            }
        } else {
            ok = false;
            if (!nocheck &&
                ((preinject && checked_files[i].is_preinject) ||
                 (!preinject && debug && checked_files[i].is_debug) ||
                 (!preinject && !debug && !checked_files[i].is_debug)) &&
                checked_files[i].platform == dr_platform) {
                /* We don't want to create a .1config file that won't be freed
                 * b/c the core is never injected
                 */
                if (report) {
                    error("cannot find required file %s\n"
                          "Use -root to specify a proper DynamoRIO root directory.",
                          buf);
                }
                return false;
            } else {
                if (checked_files[i].platform == DR_PLATFORM_DEFAULT) {
                    /* Support a single-bitwidth package. */
                    ok = true;
                } else if (!nowarn)
                    warn("cannot find %s: is this an incomplete installation?", buf);
            }
        }
    }
    assert(found_lib);
    if (!ok && !nowarn)
        warn("%s does not appear to be a valid DynamoRIO root", dr_root);
    return true;
}

/* Check if the provided root directory actually has the files we
 * expect.  Returns whether a fatal problem.
 */
static bool
check_dr_root(const char *dr_root, bool debug, dr_platform_t dr_platform, bool preinject,
              bool report)
{
    return expand_dr_root(dr_root, debug, dr_platform, preinject, report, NULL, 0, NULL,
                          0);
}

/* Register a process to run under DR */
bool
register_proc(const char *process, process_id_t pid, bool global, const char *dr_root,
              const dr_operation_mode_t dr_mode, bool debug, dr_platform_t dr_platform,
              const char *extra_ops, const char *custom_dll, const char *custom_alt_dll)
{
    dr_config_status_t status;

    assert(dr_root != NULL);
    if (!does_file_exist(dr_root)) {
        error("cannot access DynamoRIO root directory %s", dr_root);
        return false;
    }
    if (dr_mode == DR_MODE_NONE) {
        error("you must provide a DynamoRIO mode");
        return false;
    }

    char dr_lib_path[MAXIMUM_PATH];
    char dr_alt_lib_path[MAXIMUM_PATH];
    if (custom_dll != NULL) {
        strncpy(dr_lib_path, custom_dll, BUFFER_SIZE_ELEMENTS(dr_lib_path));
        NULL_TERMINATE_BUFFER(dr_lib_path);
    } else {
        bool check_ok =
            expand_dr_root(dr_root, debug, dr_platform, false /*!pre*/,
                           dr_mode != DR_MODE_DO_NOT_RUN /*report*/, dr_lib_path,
                           BUFFER_SIZE_ELEMENTS(dr_lib_path), dr_alt_lib_path,
                           BUFFER_SIZE_ELEMENTS(dr_alt_lib_path));
        /* Warn if the DR root directory doesn't look right, unless -norun,
         * in which case don't bother.
         */
        if (dr_mode != DR_MODE_DO_NOT_RUN && !check_ok)
            return false;
    }
    if (custom_alt_dll != NULL) {
        /* Overwrite what expand_dr_root found if an alt is specified. */
        strncpy(dr_alt_lib_path, custom_alt_dll, BUFFER_SIZE_ELEMENTS(dr_alt_lib_path));
        NULL_TERMINATE_BUFFER(dr_alt_lib_path);
    }

    if (dr_process_is_registered(process, pid, global, dr_platform, NULL, NULL, NULL,
                                 NULL)) {
        warn("overriding existing registration");
        if (!unregister_proc(process, pid, global, dr_platform))
            return false;
    }

    status = dr_register_process(process, pid, global, dr_root, dr_mode, debug,
                                 dr_platform, extra_ops);

    if (status != DR_SUCCESS) {
        /* USERPROFILE is not set by default over cygwin ssh */
        char buf[MAXIMUM_PATH];
#ifdef WINDOWS
        if (drfront_get_env_var("USERPROFILE", buf, BUFFER_SIZE_ELEMENTS(buf)) ==
                DRFRONT_ERROR &&
            drfront_get_env_var("DYNAMORIO_CONFIGDIR", buf, BUFFER_SIZE_ELEMENTS(buf)) ==
                DRFRONT_ERROR) {
            error("process %s registration failed: "
                  "neither USERPROFILE nor DYNAMORIO_CONFIGDIR env var set!",
                  process == NULL ? "<null>" : process);
        } else {
#endif
            if (status == DR_CONFIG_DIR_NOT_FOUND) {
                dr_get_config_dir(global, true /*tmp*/, buf, BUFFER_SIZE_ELEMENTS(buf));
                error("process %s registration failed: check config dir %s permissions",
                      process == NULL ? "<null>" : process, buf);
#ifdef ANDROID
                error("for Android apps, set TMPDIR to /data/data/com.your.app");
#endif
            } else {
                error("process %s registration failed",
                      process == NULL ? "<null>" : process);
            }
#ifdef WINDOWS
        }
#endif
        return false;
    }
    status = dr_register_inject_paths(
        process, pid, global, dr_platform, dr_lib_path[0] == '\0' ? NULL : dr_lib_path,
        dr_alt_lib_path[0] == '\0' ? NULL : dr_alt_lib_path);
    if (status != DR_SUCCESS)
        warn("failed to specify DynamoRIO library paths: falling back to defaults");

    return true;
}

/* Check if the specified client library actually exists. */
void
check_client_lib(const char *client_lib)
{
    if (!does_file_exist(client_lib)) {
        warn("%s does not exist", client_lib);
    }
}

bool
register_client(const char *process_name, process_id_t pid, bool global,
                dr_platform_t dr_platform, client_id_t client_id, const char *path,
                bool is_alt_bitwidth, const char *options)
{
    size_t priority;
    dr_config_status_t status;

    if (!dr_process_is_registered(process_name, pid, global, dr_platform, NULL, NULL,
                                  NULL, NULL)) {
        error("can't register client: process %s is not registered",
              process_name == NULL ? "<null>" : process_name);
        return false;
    }

    check_client_lib(path);

    /* just append to the existing client list */
    priority = dr_num_registered_clients(process_name, pid, global, dr_platform);

    info("registering client with id=%d path=|%s| ops=|%s|%s", client_id, path, options,
         is_alt_bitwidth ? " alt-bitwidth" : "");
    dr_config_client_t info;
    info.struct_size = sizeof(info);
    info.id = client_id;
    info.priority = priority;
    info.path = (char *)path;
    info.options = (char *)options;
    info.is_alt_bitwidth = is_alt_bitwidth;
    status = dr_register_client_ex(process_name, pid, global, dr_platform, &info);
    if (status != DR_SUCCESS) {
        if (status == DR_CONFIG_STRING_TOO_LONG) {
            error("client %s registration failed: option string too long: \"%s\"", path,
                  options);
        } else if (status == DR_CONFIG_OPTIONS_INVALID) {
            error("client %s registration failed: options cannot contain ';' or all "
                  "3 quote types: %s",
                  path, options);
        } else {
            error("client %s registration failed with error code %d", path, status);
        }
        return false;
    }
    return true;
}

#if defined(WINDOWS) || defined(DRRUN) || defined(DRCONFIG)
static const char *
platform_name(dr_platform_t platform)
{
    return (platform == DR_PLATFORM_64BIT IF_X64(|| platform == DR_PLATFORM_DEFAULT))
        ? "64-bit"
        : "32-bit/WOW64";
}
#endif

/* FIXME i#840: Port registered process iterator. */
#ifdef WINDOWS
static void
list_process(char *name, bool global, dr_platform_t platform,
             dr_registered_process_iterator_t *iter)
{
    char name_buf[MAXIMUM_PATH] = { 0 };
    char root_dir_buf[MAXIMUM_PATH] = { 0 };
    dr_operation_mode_t dr_mode;
    bool debug;
    char dr_options[DR_MAX_OPTIONS_LENGTH] = { 0 };
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

    if (dr_mode == DR_MODE_DO_NOT_RUN) {
        printf("Process %s registered to NOT RUN on %s\n", name, platform_name(platform));
    } else {
        printf("Process %s registered for %s\n", name, platform_name(platform));
    }
    printf("\tRoot=\"%s\" Debug=%s\n\tOptions=\"%s\"\n", root_dir_buf,
           debug ? "yes" : "no", dr_options);

    c_iter = dr_client_iterator_start(name, 0, global, platform);
    while (dr_client_iterator_hasnext(c_iter)) {
        client_id_t id;
        size_t client_pri;
        char client_path[MAXIMUM_PATH] = { 0 };
        char client_opts[DR_MAX_OPTIONS_LENGTH] = { 0 };

        dr_client_iterator_next(c_iter, &id, &client_pri, client_path, client_opts);

        printf("\tClient=0x%08x Priority=%d\n\t\tPath=\"%s\"\n\t\tOptions=\"%s\"\n", id,
               (uint)client_pri, client_path, client_opts);
    }
    dr_client_iterator_stop(c_iter);
}
#endif /* WINDOWS */

#ifndef DRCONFIG
/* i#200/PR 459481: communicate child pid via file */
static void
write_pid_to_file(const char *pidfile, process_id_t pid)
{
    FILE *f = fopen_utf8(pidfile, "w");
    if (f == NULL) {
        warn("cannot open %s: %d\n", pidfile, GetLastError());
    } else {
        char pidbuf[16];
        ssize_t written;
        _snprintf(pidbuf, BUFFER_SIZE_ELEMENTS(pidbuf), PIDFMT "\n", pid);
        NULL_TERMINATE_BUFFER(pidbuf);
        written = fwrite(pidbuf, 1, strlen(pidbuf), f);
        assert(written == strlen(pidbuf));
        fclose(f);
    }
}
#endif /* DRCONFIG */

#if defined(DRCONFIG) || defined(DRRUN)
static void
append_client(const char *client, int id, const char *client_ops, bool is_alt_bitwidth,
              char client_paths[MAX_CLIENT_LIBS][MAXIMUM_PATH],
              client_id_t client_ids[MAX_CLIENT_LIBS],
              const char *client_options[MAX_CLIENT_LIBS],
              bool alt_bitwidth[MAX_CLIENT_LIBS], size_t *num_clients)
{
    size_t index = *num_clients;
    /* Handle "-c32 -c64" order for native 64-bit where we want to swap the 32
     * and 64 to get the alt last.
     */
    if (index > 0 && id == client_ids[index - 1] && alt_bitwidth[index - 1] &&
        !is_alt_bitwidth) {
        /* Insert this one before the prior one by first copying the prior to index. */
        client_ids[index] = client_ids[index - 1];
        alt_bitwidth[index] = alt_bitwidth[index - 1];
        _snprintf(client_paths[index], BUFFER_SIZE_ELEMENTS(client_paths[index]), "%s",
                  client_paths[index - 1]);
        NULL_TERMINATE_BUFFER(client_paths[index]);
        client_options[index] = client_options[index - 1];
        index = index - 1;
    }
    /* We support an empty client for native -t usage */
    if (client[0] != '\0') {
        get_absolute_path(client, client_paths[index],
                          BUFFER_SIZE_ELEMENTS(client_paths[index]));
        NULL_TERMINATE_BUFFER(client_paths[index]);
        info("client %d path: %s", (int)index, client_paths[index]);
    }
    client_ids[index] = id;
    client_options[index] = client_ops;
    alt_bitwidth[index] = is_alt_bitwidth;
    (*num_clients)++;
}
#endif

/* Appends a space-separated option string to buf.  A space is appended only if
 * the buffer is non-empty.  Aborts on buffer overflow.  Always null terminates
 * the string.
 * XXX: Use print_to_buffer.
 */
static void
add_extra_option(char *buf, size_t bufsz, size_t *sofar, const char *fmt, ...)
{
    ssize_t len;
    va_list ap;
    if (*sofar > 0 && *sofar < bufsz)
        buf[(*sofar)++] = ' '; /* Add a space. */

    va_start(ap, fmt);
    len = vsnprintf(buf + *sofar, bufsz - *sofar, fmt, ap);
    va_end(ap);

    if (len < 0 || (size_t)len >= bufsz) {
        error("option string too long, buffer overflow");
        die();
    }
    *sofar += len;
    /* be paranoid: though usually many calls in a row and could delay until end */
    buf[bufsz - 1] = '\0';
}

#if defined(DRCONFIG) || defined(DRRUN)
/* Returns the path to the client library.  Appends to extra_ops.
 * A tool config file must contain one of these line types, or two if
 * they are a pair of CLIENT32_* and CLIENT64_* specifiers:
 *   CLIENT_ABS=<absolute path to client>
 *   CLIENT_REL=<path to client relative to DR root>
 *   CLIENT32_ABS=<absolute path to 32-bit client>
 *   CLIENT32_REL=<path to 32-bit client relative to DR root>
 *   CLIENT64_ABS=<absolute path to 64-bit client>
 *   CLIENT64_REL=<path to 64-bit client relative to DR root>
 * It can contain as many DR_OP= lines as desired.  Each must contain
 * one DynamoRIO option token:
 *   DR_OP=<DR option token>
 * It can also contain TOOL_OP= lines for tool options, though normally
 * tool default options should just be set in the tool:
 *   TOOL_OP=<tool option token>
 * We take one token per line rather than a string of options to avoid
 * having to do any string parsing.
 * DR ops go last (thus, user can't override); tool ops go first.
 *
 * We also support tools with their own frontend launcher via the following
 * tool config file lines:
 *   FRONTEND_ABS=<absolute path to frontend>
 *   FRONTEND_REL=<path to frontend relative to DR root>
 * If either is present, drrun will launch the frontend and pass it the
 * tool options followed by the app and its options.
 * The path to DR can be included in the frontend options via this line:
 *   TOOL_OP_DR_PATH
 * The options to DR can be included in a single token, preceded by a prefix,
 * via this line:
 *   TOOL_OP_DR_BUNDLE=<prefix>
 *
 * A notification message can be presented to the user with:
 *   USER_NOTICE=This tool is currently experimental.  Please report issues to <url>.
 */
static bool
read_tool_file(const char *toolname, const char *dr_root, const char *dr_toolconfig_dir,
               dr_platform_t dr_platform, char *client, size_t client_size,
               char *alt_client, size_t alt_size, char *ops, size_t ops_size,
               size_t *ops_sofar, char *tool_ops, size_t tool_ops_size,
               size_t *tool_ops_sofar, char *native_path OUT, size_t native_path_size)
{
    FILE *f;
    char config_file[MAXIMUM_PATH];
    char line[MAXIMUM_PATH];
    bool found_client = false;
    const char *arch = IF_X64_ELSE("64", "32");
    if (dr_platform == DR_PLATFORM_32BIT)
        arch = "32";
    else if (dr_platform == DR_PLATFORM_64BIT)
        arch = "64";
    _snprintf(config_file, BUFFER_SIZE_ELEMENTS(config_file), "%s/%s.drrun%s",
              dr_toolconfig_dir, toolname, arch);
    NULL_TERMINATE_BUFFER(config_file);
    info("reading tool config file %s", config_file);
    f = fopen_utf8(config_file, "r");
    if (f == NULL) {
        error("cannot find tool config file %s", config_file);
        return false;
    }
    while (fgets(line, BUFFER_SIZE_ELEMENTS(line), f) != NULL) {
        ssize_t len;
        NULL_TERMINATE_BUFFER(line);
        len = strlen(line) - 1;
        while (len >= 0 && (line[len] == '\n' || line[len] == '\r')) {
            line[len] = '\0';
            len--;
        }
        if (line[0] == '#') {
            continue;
        } else if (strstr(line, "CLIENT_REL=") == line) {
            _snprintf(client, client_size, "%s/%s", dr_root,
                      line + strlen("CLIENT_REL="));
            client[client_size - 1] = '\0';
            found_client = true;
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 client);
            }
        } else if (strstr(line, IF_X64_ELSE("CLIENT64_REL=", "CLIENT32_REL=")) == line) {
            _snprintf(client, client_size, "%s/%s", dr_root,
                      line + strlen(IF_X64_ELSE("CLIENT64_REL=", "CLIENT32_REL=")));
            client[client_size - 1] = '\0';
            found_client = true;
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 client);
            }
        } else if (strstr(line, IF_X64_ELSE("CLIENT32_REL=", "CLIENT64_REL=")) == line) {
            _snprintf(alt_client, alt_size, "%s/%s", dr_root,
                      line + strlen(IF_X64_ELSE("CLIENT32_REL=", "CLIENT64_REL=")));
            alt_client[alt_size - 1] = '\0';
            if (!does_file_exist(alt_client)) {
                alt_client[0] = '\0';
            }
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 alt_client);
            }
        } else if (strstr(line, "CLIENT_ABS=") == line) {
            strncpy(client, line + strlen("CLIENT_ABS="), client_size);
            found_client = true;
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 client);
            }
        } else if (strstr(line, IF_X64_ELSE("CLIENT64_ABS=", "CLIENT32_ABS=")) == line) {
            strncpy(client, line + strlen(IF_X64_ELSE("CLIENT64_ABS=", "CLIENT32_ABS=")),
                    client_size);
            found_client = true;
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 client);
            }
        } else if (strstr(line, IF_X64_ELSE("CLIENT32_ABS=", "CLIENT64_ABS=")) == line) {
            strncpy(alt_client,
                    line + strlen(IF_X64_ELSE("CLIENT32_ABS=", "CLIENT64_ABS=")),
                    alt_size);
            if (!does_file_exist(alt_client)) {
                alt_client[0] = '\0';
            }
            if (native_path[0] != '\0') {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 alt_client);
            }
        } else if (strstr(line, "DR_OP=") == line) {
            if (strcmp(line, "DR_OP=") != 0) {
                add_extra_option(ops, ops_size, ops_sofar, "\"%s\"",
                                 line + strlen("DR_OP="));
            }
        } else if (strstr(line, "TOOL_OP=") == line) {
            if (strcmp(line, "TOOL_OP=") != 0) {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"",
                                 line + strlen("TOOL_OP="));
            }
#    ifdef DRRUN /* native only supported for drrun */
        } else if (strstr(line, "FRONTEND_ABS=") == line) {
            _snprintf(native_path, native_path_size, "%s",
                      line + strlen("FRONTEND_ABS="));
            native_path[native_path_size - 1] = '\0';
            found_client = true;
        } else if (strstr(line, "FRONTEND_REL=") == line) {
            _snprintf(native_path, native_path_size, "%s/%s", dr_root,
                      line + strlen("FRONTEND_REL="));
            native_path[native_path_size - 1] = '\0';
            found_client = true;
        } else if (strstr(line, "TOOL_OP_DR_PATH") == line) {
            add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "\"%s\"", dr_root);
        } else if (strstr(line, "TOOL_OP_DR_BUNDLE=") == line) {
            if (strcmp(line, "TOOL_OP_DR_BUNDLE=") != 0) {
                add_extra_option(tool_ops, tool_ops_size, tool_ops_sofar, "%s `%s`",
                                 line + strlen("TOOL_OP_DR_BUNDLE="), ops);
            }
#    else
        } else if (strstr(line, "FRONTEND_ABS=") == line ||
                   strstr(line, "FRONTEND_REL=") == line ||
                   strstr(line, "TOOL_OP_DR_PATH") == line ||
                   strstr(line, "TOOL_OP_DR_BUNDLE=") == line) {
            usage(false, "this tool's config only works with drrun, not drconfig");
            return false;
#    endif
        } else if (strstr(line, "USER_NOTICE=") == line) {
            warn("%s", line + strlen("USER_NOTICE="));
        } else if (line[0] != '\0') {
            error("tool config file is malformed: unknown line %s", line);
            return false;
        }
    }
    fclose(f);
    return found_client;
}
#endif /* DRCONFIG || DRRUN */

#ifdef DRRUN
/* This parser modifies the string, adding nulls to split it up in place.
 * Caller should continue iterating until *token == NULL.
 */
static char *
split_option_token(char *s, char **token OUT, bool split)
{
    bool quoted = false;
    char endquote = '\0';
    if (s == NULL) {
        *token = NULL;
        return NULL;
    }
    /* first skip leading whitespace */
    while (*s != '\0' && isspace(*s))
        s++;
    if (*s == '\"' || *s == '\'' || *s == '`') {
        quoted = true;
        endquote = *s;
        s++;
    }
    *token = (*s == '\0' ? NULL : s);
    while (*s != '\0' && ((!quoted && !isspace(*s)) || (quoted && *s != endquote)))
        s++;
    if (*s == '\0')
        return NULL;
    else {
        if (quoted && !split)
            s++;
        if (split && *s != '\0')
            *s++ = '\0';
        return s;
    }
}

/* Caller must free() the returned argv array.
 * This routine writes to tool_ops.
 */
static const char **
switch_to_native_tool(const char **app_argv, const char *native_tool, char *tool_ops)
{
    const char **new_argv, **arg;
    char *s, *token;
    uint count, i;
    for (arg = app_argv, count = 0; *arg != NULL; arg++, count++)
        ; /* empty */
    for (s = split_option_token(tool_ops, &token, false /*do not mutate*/); token != NULL;
         s = split_option_token(s, &token, false /*do not mutate*/)) {
        count++;
    }
    count++; /* for native_tool path */
    count++; /* for "--" */
    count++; /* for NULL */
    new_argv = (const char **)malloc(count * sizeof(char *));
    i = 0;
    new_argv[i++] = native_tool;
    for (s = split_option_token(tool_ops, &token, true); token != NULL;
         s = split_option_token(s, &token, true)) {
        new_argv[i++] = token;
    }
    new_argv[i++] = "--";
    for (arg = app_argv; *arg != NULL; arg++)
        new_argv[i++] = *arg;
    new_argv[i++] = NULL;
    assert(i == count);
    if (verbose) {
        char buf[MAXIMUM_PATH * 2];
        char *c = buf;
        for (i = 0; i < count - 1; i++) {
            ssize_t len = _snprintf(c, BUFFER_SIZE_ELEMENTS(buf) - (c - buf), " \"%s\"",
                                    new_argv[i]);
            if (len < 0 || (size_t)len >= BUFFER_SIZE_ELEMENTS(buf) - (c - buf))
                break;
            c += len;
        }
        NULL_TERMINATE_BUFFER(buf);
        info("native tool cmdline: %s", buf);
    }
    return new_argv;
}
#endif /* DRRUN */

int
_tmain(int argc, TCHAR *targv[])
{
    char *dr_root = NULL;
    char client_paths[MAX_CLIENT_LIBS][MAXIMUM_PATH];
#if defined(DRCONFIG) || defined(DRRUN)
    char *process = NULL;
    const char *client_options[MAX_CLIENT_LIBS] = {
        NULL,
    };
    client_id_t client_ids[MAX_CLIENT_LIBS] = {
        0,
    };
    bool alt_bitwidth[MAX_CLIENT_LIBS];
    size_t num_clients = 0;
    char single_client_ops[DR_MAX_OPTIONS_LENGTH];
#endif
#ifndef DRINJECT
#    if defined(MF_API) || defined(PROBE_API)
    /* must set -mode */
    dr_operation_mode_t dr_mode = DR_MODE_NONE;
#    else
    /* only one choice so no -mode */
    dr_operation_mode_t dr_mode = DR_MODE_CODE_MANIPULATION;
#    endif
#endif /* !DRINJECT */
    char extra_ops[MAX_OPTIONS_STRING];
    size_t extra_ops_sofar = 0;
#ifdef DRCONFIG
    action_t action = action_none;
#endif
    bool use_debug = false;
    dr_platform_t dr_platform = DR_PLATFORM_DEFAULT;
#ifdef WINDOWS
    /* FIXME i#840: Implement nudges on Linux. */
    bool nudge_all = false;
    bool use_late_injection = false;
    process_id_t nudge_pid = 0;
    client_id_t nudge_id = 0;
    uint64 nudge_arg = 0;
    bool list_registered = false;
    uint nudge_timeout = INFINITE;
    uint detach_timeout = DETACH_RECOMMENDED_TIMEOUT;
    bool syswide_on = false;
    bool syswide_off = false;
#endif /* WINDOWS */
    bool global = false;
    int exitcode;
#if defined(DRRUN) || defined(DRINJECT)
    char *pidfile = NULL;
    bool force_injection = false;
    bool inject = true;
    int limit = 0; /* in seconds */
#    ifdef WINDOWS
    bool showstats = false;
    bool showmem = false;
    time_t start_time, end_time;
#    else
    bool use_ptrace = false;
    bool kill_group = false;
#    endif
    process_id_t attach_pid = 0;
    char *app_name = NULL;
    char full_app_name[MAXIMUM_PATH];
    const char **app_argv;
    int errcode;
    void *inject_data;
    bool success;
    bool exit0 = false;
#endif
#if defined(DRCONFIG)
#    ifdef WINDOWS
    process_id_t detach_pid = 0;
#    endif
#endif
    char *drlib_path = NULL;
#if defined(DRCONFIG) || defined(DRRUN)
    char *drlib_alt_path = NULL;
    char custom_alt_dll[MAXIMUM_PATH];
#endif
    char custom_dll[MAXIMUM_PATH];
    char *dr_toolconfig_dir = NULL;
    char custom_toolconfig_dir[MAXIMUM_PATH];
    int i;
#ifndef DRINJECT
    size_t j;
#endif
    char buf[MAXIMUM_PATH];
    char default_root[MAXIMUM_PATH];
    char default_toolconfig_dir[MAXIMUM_PATH];
    char *c;
#if defined(DRCONFIG) || defined(DRRUN)
    char native_tool[MAXIMUM_PATH];
#endif
#ifdef DRRUN
    char exe[MAXIMUM_PATH];
    void *tofree = NULL;
    bool configure = true;
#endif
    char **argv;
    drfront_status_t sc;

#if defined(WINDOWS) && !defined(_UNICODE)
#    error _UNICODE must be defined
#else
    /* Convert to UTF-8 if necessary */
    sc = drfront_convert_args((const TCHAR **)targv, &argv, argc);
    if (sc != DRFRONT_SUCCESS)
        fatal("failed to process args: %d", sc);
#endif

    memset(client_paths, 0, sizeof(client_paths));
    extra_ops[0] = '\0';
#if defined(DRCONFIG) || defined(DRRUN)
    native_tool[0] = '\0';
#endif

    /* Quick pass to set verbose for info() logs before main parsing. */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
            break;
        }
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-t") == 0 ||
            strcmp(argv[i], "-c32") == 0 || strcmp(argv[i], "-c64") == 0 ||
            strcmp(argv[i], "--") == 0)
            break;
    }

    /* default root: we assume this tool is in <root>/bin{32,64}/dr*.exe */
    get_absolute_path(argv[0], buf, BUFFER_SIZE_ELEMENTS(buf));
    NULL_TERMINATE_BUFFER(buf);
    c = buf + strlen(buf) - 1;
    while (*c != '\\' && *c != '/' && c > buf)
        c--;
    _snprintf(c + 1, BUFFER_SIZE_ELEMENTS(buf) - (c + 1 - buf), "..");
    NULL_TERMINATE_BUFFER(buf);
    get_absolute_path(buf, default_root, BUFFER_SIZE_ELEMENTS(default_root));
    NULL_TERMINATE_BUFFER(default_root);
    dr_root = default_root;
    info("default root: %s", default_root);

    _snprintf(default_toolconfig_dir, BUFFER_SIZE_ELEMENTS(default_toolconfig_dir),
              "%s/%s", dr_root, "tools");
    NULL_TERMINATE_BUFFER(default_toolconfig_dir);
    dr_toolconfig_dir = default_toolconfig_dir;
    info("default toolconfig dir: %s", default_toolconfig_dir);

    /* we re-read the tool list if the root, platform or toolconfig dir change */
    read_tool_list(dr_toolconfig_dir, dr_platform);

    /* parse command line */
    for (i = 1; i < argc; i++) {

        /* params with no arg */
        if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = true;
            continue;
        } else if (strcmp(argv[i], "-quiet") == 0) {
            quiet = true;
            continue;
        } else if (strcmp(argv[i], "-nocheck") == 0) {
            nocheck = true;
            continue;
        } else if (strcmp(argv[i], "-debug") == 0) {
            use_debug = true;
            continue;
        } else if (!strcmp(argv[i], "-version")) {
#if defined(BUILD_NUMBER) && defined(VERSION_NUMBER)
            printf(TOOLNAME " version %s -- build %d\n", STRINGIFY(VERSION_NUMBER),
                   BUILD_NUMBER);
#elif defined(BUILD_NUMBER)
            printf(TOOLNAME " custom build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
            printf(TOOLNAME " custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
            exit(0);
        }
#ifdef DRCONFIG
#    ifdef WINDOWS
        /* FIXME i#840: These are NYI for Linux. */
        else if (!strcmp(argv[i], "-list_registered")) {
            action = action_list;
            list_registered = true;
            continue;
        } else if (strcmp(argv[i], "-syswide_on") == 0) {
            syswide_on = true;
            continue;
        } else if (strcmp(argv[i], "-syswide_off") == 0) {
            syswide_off = true;
            continue;
        }
#    endif
        else if (strcmp(argv[i], "-global") == 0) {
            global = true;
            continue;
        } else if (strcmp(argv[i], "-norun") == 0) {
            dr_mode = DR_MODE_DO_NOT_RUN;
            continue;
        }
#endif
        else if (strcmp(argv[i], "-32") == 0) {
            dr_platform = DR_PLATFORM_32BIT;
            read_tool_list(dr_toolconfig_dir, dr_platform);
            continue;
        } else if (strcmp(argv[i], "-64") == 0) {
            dr_platform = DR_PLATFORM_64BIT;
            read_tool_list(dr_toolconfig_dir, dr_platform);
            continue;
        }
#if defined(DRRUN) || defined(DRINJECT)
#    ifdef WINDOWS
        else if (strcmp(argv[i], "-stats") == 0) {
            showstats = true;
            continue;
        } else if (strcmp(argv[i], "-mem") == 0) {
            showmem = true;
            continue;
        }
#    endif
        else if (strcmp(argv[i], "-no_inject") == 0 ||
                 /* support old drinjectx param name */
                 strcmp(argv[i], "-noinject") == 0 || strcmp(argv[i], "-static") == 0) {
            DR_dll_not_needed = true;
            inject = false;
            continue;
        } else if (strcmp(argv[i], "-force") == 0) {
            force_injection = true;
            continue;
        } else if (strcmp(argv[i], "-no_wait") == 0) {
            limit = -1;
            continue;
        }
#    ifndef MACOS /* XXX i#1285: private loader NYI on MacOS */
        else if (strcmp(argv[i], "-late") == 0) {
            /* Appending -no_early_inject to extra_ops communicates our intentions
             * to drinjectlib on UNIX, as well as the core for all platforms.
             */
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "-no_early_inject");
#        ifdef WINDOWS
            use_late_injection = true;
#        endif
            continue;
        }
#    endif
        else if (strcmp(argv[i], "-attach") == 0) {
            if (i + 1 >= argc)
                usage(false, "attach requires a process id");
            const char *pid_str = argv[++i];
            process_id_t pid = strtoul(pid_str, NULL, 10);
            if (pid == ULONG_MAX)
                usage(false, "attach expects an integer pid: '%s'", pid_str);
            if (pid == 0) {
                usage(false, "attach passed an invalid pid: '%s'", pid_str);
            }
            attach_pid = pid;
#    ifdef UNIX
            use_ptrace = true;
#    endif
#    ifdef WINDOWS
            use_late_injection = true;
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "-skip_terminating_threads");
#    endif
            continue;
        } else if (strcmp(argv[i], "-takeovers") == 0) {
            const char *num_attemps = argv[++i];
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "-takeover_attempts %s", num_attemps);
            continue;
        } else if (strcmp(argv[i], "-takeover_sleep") == 0) {
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "-sleep_between_takeovers");
            continue;
        }
#    ifdef UNIX
        else if (strcmp(argv[i], "-use_ptrace") == 0) {
            /* Undocumented option for using ptrace on a fresh process. */
            use_ptrace = true;
            continue;
        }
#        ifndef MACOS /* XXX i#1285: private loader NYI on MacOS */
        else if (strcmp(argv[i], "-early") == 0) {
            /* Now the default: left here just for back-compat */
            continue;
        }
#        endif
#    endif /* UNIX */
        else if (strcmp(argv[i], "-exit0") == 0) {
            exit0 = true;
            continue;
        }
#endif
        else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ||
                 strcmp(argv[i], "-h") == 0) {
            usage(true, "" /* no error msg */);
            continue;
        }
        /* all other flags have an argument -- make sure it exists */
        else if (argv[i][0] == '-' && i == argc - 1) {
            usage(false, "invalid arguments");
        }

        /* params with an arg */
        if (strcmp(argv[i], "-root") == 0 ||
            /* support -dr_home alias used by script */
            strcmp(argv[i], "-dr_home") == 0) {
            dr_root = argv[++i];
            /* Modify the default toolconfig dir only.
             * If toolconfig dir is specified explicitly, dr_toolconfig_dir points
             * to other buffer, hence is not affected by overwriting the default.
             */
            _snprintf(default_toolconfig_dir,
                      BUFFER_SIZE_ELEMENTS(default_toolconfig_dir), "%s/%s", dr_root,
                      "tools");
            NULL_TERMINATE_BUFFER(default_toolconfig_dir);
            read_tool_list(dr_toolconfig_dir, dr_platform);
        } else if (strcmp(argv[i], "-logdir") == 0) {
            /* Accept this for compatibility with the old drrun shell script. */
            const char *dir = argv[++i];
            if (!does_file_exist(dir))
                usage(false, "-logdir %s does not exist", dir);
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "-logdir `%s`", dir);
            continue;
        }
#ifdef DRCONFIG
        else if (strcmp(argv[i], "-reg") == 0) {
            if (action != action_none) {
                usage(false, "more than one action specified");
            }
            action = action_register;
            process = argv[++i];
        } else if (strcmp(argv[i], "-unreg") == 0) {
            if (action != action_none) {
                usage(false, "more than one action specified");
            }
            action = action_unregister;
            process = argv[++i];
        } else if (strcmp(argv[i], "-isreg") == 0) {
            if (action != action_none) {
                usage(false, "more than one action specified");
            }
            action = action_list;
            process = argv[++i];
        }
#    ifdef WINDOWS
        /* FIXME i#840: Nudge is NYI for Linux. */
        else if (strcmp(argv[i], "-nudge_timeout") == 0) {
            nudge_timeout = strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "-nudge") == 0 || strcmp(argv[i], "-nudge_pid") == 0 ||
                   strcmp(argv[i], "-nudge_all") == 0) {
            if (action != action_none) {
                usage(false, "more than one action specified");
            }
            if (i + 2 >= argc || (strcmp(argv[i], "-nudge_all") != 0 && i + 3 >= argc)) {
                usage(false, "too few arguments to -nudge");
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
        } else if (strcmp(argv[i], "-detach") == 0) {
            if (i + 1 >= argc)
                usage(false, "detach requires a process id");
            const char *pid_str = argv[++i];
            process_id_t pid = strtoul(pid_str, NULL, 10);
            if (pid == ULONG_MAX)
                usage(false, "detach expects an integer pid: '%s'", pid_str);
            if (pid == 0) {
                usage(false, "detach passed an invalid pid: '%s'", pid_str);
            }
            detach_pid = pid;
        }
#    endif
#endif
#if defined(DRCONFIG) || defined(DRRUN)
#    if defined(MF_API) || defined(PROBE_API)
        else if (strcmp(argv[i], "-mode") == 0) {
            char *mode_str = argv[++i];
            if (dr_mode == DR_MODE_DO_NOT_RUN)
                usage(false, "cannot combine -norun with -mode");
            if (strcmp(mode_str, "code") == 0) {
                dr_mode = DR_MODE_CODE_MANIPULATION;
            }
#        ifdef MF_API
            else if (strcmp(mode_str, "security") == 0) {
                dr_mode = DR_MODE_MEMORY_FIREWALL;
            }
#        endif
#        ifdef PROBE_API
            else if (strcmp(mode_str, "probe") == 0) {
                dr_mode = DR_MODE_PROBE;
            }
#        endif
            else {
                usage(false, "unknown mode: %s", mode_str);
            }
        }
#    endif
        else if (strcmp(argv[i], "-client") == 0) {
            if (num_clients == MAX_CLIENT_LIBS) {
                error("Maximum number of clients is %d", MAX_CLIENT_LIBS);
                die();
            } else {
                const char *client;
                int id;
                const char *ops;
                if (i + 3 >= argc) {
                    usage(false, "too few arguments to -client");
                }

                /* Support relative client paths: very useful! */
                client = argv[++i];
                id = strtoul(argv[++i], NULL, 16);
                ops = argv[++i];
                append_client(client, id, ops, false, client_paths, client_ids,
                              client_options, alt_bitwidth, &num_clients);
            }
        } else if (strcmp(argv[i], "-ops") == 0) {
            /* support repeating the option (i#477) */
            add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                             "%s", argv[++i]);
        }
#endif
        else if (strcmp(argv[i], "-use_dll") == 0) {
            DR_dll_not_needed = true;
            /* Support relative path: very useful! */
            get_absolute_path(argv[++i], custom_dll, BUFFER_SIZE_ELEMENTS(custom_dll));
            NULL_TERMINATE_BUFFER(custom_dll);
            drlib_path = custom_dll;
        }
#if defined(DRCONFIG) || defined(DRRUN)
        else if (strcmp(argv[i], "-use_alt_dll") == 0) {
            get_absolute_path(argv[++i], custom_alt_dll,
                              BUFFER_SIZE_ELEMENTS(custom_alt_dll));
            NULL_TERMINATE_BUFFER(custom_alt_dll);
            drlib_alt_path = custom_alt_dll;
        }
#endif
        else if (strcmp(argv[i], "-tool_dir") == 0) {
            get_absolute_path(argv[++i], custom_toolconfig_dir,
                              BUFFER_SIZE_ELEMENTS(custom_toolconfig_dir));
            NULL_TERMINATE_BUFFER(custom_toolconfig_dir);
            dr_toolconfig_dir = custom_toolconfig_dir;
            read_tool_list(dr_toolconfig_dir, dr_platform);
        }
#if defined(DRRUN) || defined(DRINJECT)
        else if (strcmp(argv[i], "-pidfile") == 0) {
            pidfile = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            limit = atoi(argv[++i]);
            if (limit <= 0)
                usage(false, "invalid time");
        } else if (strcmp(argv[i], "-m") == 0) {
            limit = atoi(argv[++i]) * 60;
            if (limit <= 0)
                usage(false, "invalid time");
        } else if (strcmp(argv[i], "-h") == 0) {
            limit = atoi(argv[++i]) * 3600;
            if (limit <= 0)
                usage(false, "invalid time");
        }
#    ifdef UNIX
        else if (strcmp(argv[i], "-killpg") == 0) {
            kill_group = true;
        }
#    endif
#endif
#if defined(DRCONFIG) || defined(DRRUN)
        /* if there are still options, assume user is using -- to separate and pass
         * through options to DR.  we do not handle mixing DR options with tool
         * options: DR must come last.  we would need to generate code here from
         * optionsx.h to do otherwise, or to sanity check the DR options here.
         */
        else if (argv[i][0] == '-') {
            bool expect_extra_double_dash = false;
            while (i < argc) {
                if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-t") == 0 ||
                    strcmp(argv[i], "-c32") == 0 || strcmp(argv[i], "-c64") == 0 ||
                    strcmp(argv[i], "--") == 0) {
                    break;
                }
                add_extra_option(extra_ops, BUFFER_SIZE_ELEMENTS(extra_ops),
                                 &extra_ops_sofar, "\"%s\"", argv[i]);
                i++;
            }
            if (i < argc &&
                (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "-c") == 0 ||
                 strcmp(argv[i], "-c32") == 0 || strcmp(argv[i], "-c64") == 0)) {
                const char *client;
                char client_buf[MAXIMUM_PATH];
                char alt_buf[MAXIMUM_PATH];
                alt_buf[0] = '\0';
                size_t client_sofar = 0;
                bool is_alt_bitwidth = false;
                if (i + 1 >= argc)
                    usage(false, "too few arguments to %s", argv[i]);
                if (num_clients != 0 &&
                    (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-t") == 0))
                    usage(false, "Cannot use -client with %s.", argv[i]);
                if (strcmp(argv[i], "-c32") == 0)
                    expect_extra_double_dash = true;
                if (strcmp(argv[i], IF_X64_ELSE("-c32", "-c64")) == 0)
                    is_alt_bitwidth = true;
                client = argv[++i];
                single_client_ops[0] = '\0';

                if (strcmp(argv[i - 1], "-t") == 0) {
                    /* Client-requested DR default options come last, so they
                     * cannot be overridden by DR options passed here.
                     * The user must use -c or -client to do that.
                     */
                    if (!read_tool_file(client, dr_root, dr_toolconfig_dir, dr_platform,
                                        client_buf, BUFFER_SIZE_ELEMENTS(client_buf),
                                        alt_buf, BUFFER_SIZE_ELEMENTS(alt_buf), extra_ops,
                                        BUFFER_SIZE_ELEMENTS(extra_ops), &extra_ops_sofar,
                                        single_client_ops,
                                        BUFFER_SIZE_ELEMENTS(single_client_ops),
                                        &client_sofar, native_tool,
                                        BUFFER_SIZE_ELEMENTS(native_tool)))
                        usage(false, "unknown %s tool \"%s\" requested",
                              platform_name(dr_platform), client);
                    client = client_buf;
                }

                /* Treat everything up to -- or end of argv as client args. */
                i++;
                while (i < argc && strcmp(argv[i], "--") != 0) {
#    ifdef DRCONFIG
                    if (action == action_none && strcmp(argv[i], "-reg") == 0) {
                        warn("-reg is taken as a client option!");
                    }
#    endif /* DRCONFIG */
                    add_extra_option(single_client_ops,
                                     BUFFER_SIZE_ELEMENTS(single_client_ops),
                                     &client_sofar, "\"%s\"", argv[i]);
                    i++;
                }
                append_client(client, 0, single_client_ops, is_alt_bitwidth, client_paths,
                              client_ids, client_options, alt_bitwidth, &num_clients);
                if (alt_buf[0] != '\0') {
                    append_client(alt_buf, 0, single_client_ops, true, client_paths,
                                  client_ids, client_options, alt_bitwidth, &num_clients);
                }
            }
            if (i < argc && strcmp(argv[i], "--") == 0 &&
                (!expect_extra_double_dash ||
                 (i + 1 < argc && strcmp(argv[i + 1], "-c64") != 0))) {
                i++;
                goto done_with_options;
            }
        }
#else /* DRINJECT */
        else if (strcmp(argv[i], "--") == 0) {
            i++;
            goto done_with_options;
        }
#endif
        else {
#ifdef DRCONFIG
            usage(false, "unknown option: %s", argv[i]);
#else
            /* start of app and its args */
            break;
#endif
        }
    }

#if defined(DRCONFIG) || defined(DRRUN) || defined(DRINJECT)
done_with_options:
#endif

#if defined(DRRUN) || defined(DRINJECT)
#    ifdef DRRUN
    if (attach_pid != 0) {
        ssize_t size = 0;
#        ifdef UNIX
        char exe_str[MAXIMUM_PATH];
        _snprintf(exe_str, BUFFER_SIZE_ELEMENTS(exe_str), "/proc/%d/exe", attach_pid);
        NULL_TERMINATE_BUFFER(exe_str);
        size = readlink(exe_str, exe, BUFFER_SIZE_ELEMENTS(exe));
        if (size > 0) {
            if (size < BUFFER_SIZE_ELEMENTS(exe))
                exe[size] = '\0';
            else
                NULL_TERMINATE_BUFFER(exe);
        } else {
            usage(false, "attach to invalid pid");
        }
#        endif /* UNIX */
        app_name = exe;
    }
    /* Support no app if the tool has its own frontend, under the assumption
     * it may have post-processing or other features.
     */
    if (attach_pid == 0 && (i < argc || native_tool[0] == '\0')) {
#    endif
        if (i >= argc)
            usage(false, "%s", "no app specified");
        app_name = argv[i++];
        search_env(app_name, "PATH", full_app_name, BUFFER_SIZE_ELEMENTS(full_app_name));
        NULL_TERMINATE_BUFFER(full_app_name);
        if (full_app_name[0] == '\0') {
            /* may need to append .exe, FIXME : other executable types */
            char tmp_buf[MAXIMUM_PATH];
            _snprintf(tmp_buf, BUFFER_SIZE_ELEMENTS(tmp_buf), "%s%s", app_name, ".exe");
            NULL_TERMINATE_BUFFER(tmp_buf);
            search_env(tmp_buf, "PATH", full_app_name,
                       BUFFER_SIZE_ELEMENTS(full_app_name));
        }
        if (full_app_name[0] == '\0') {
            /* last try */
            get_absolute_path(app_name, full_app_name,
                              BUFFER_SIZE_ELEMENTS(full_app_name));
            NULL_TERMINATE_BUFFER(full_app_name);
        }
        if (full_app_name[0] != '\0')
            app_name = full_app_name;
        info("targeting application: \"%s\"", app_name);
#    ifdef DRRUN
    }
#    endif

    /* note that we want target app name as part of cmd line
     * (hence &argv[i - 1])
     * (FYI: if we were using WinMain, the pzsCmdLine passed in
     *  does not have our own app name in it)
     */
    app_argv = (const char **)&argv[i - 1];
    if (verbose) {
        c = buf;
        for (i = 0; app_argv[i] != NULL; i++) {
            c += _snprintf(c, BUFFER_SIZE_ELEMENTS(buf) - (c - buf), " \"%s\"",
                           app_argv[i]);
        }
        info("app cmdline: %s", buf);
    }
#    ifdef DRRUN
    if (native_tool[0] != '\0') {
        app_name = native_tool;
        inject = false;
        configure = false;
        app_argv = switch_to_native_tool(app_argv, native_tool,
                                         /* this will be changed, but we don't
                                          * need it again
                                          */
                                         (char *)client_options[0]);
        tofree = (void *)app_argv;
    }
#    endif
#else
    if (i < argc)
        usage(false, "%s", "invalid extra arguments specified");
#endif

#ifdef WINDOWS
    /* FIXME i#900: This doesn't work on Linux, and doesn't do the right thing
     * on Windows.
     */
    /* PR 244206: set the registry view before any registry access */
    set_dr_platform(dr_platform);
#endif

    /* support running out of a debug build dir */
    if (!use_debug &&
        !check_dr_root(dr_root, false, dr_platform, false /*!pre*/, false /*!report*/) &&
        check_dr_root(dr_root, true, dr_platform, false /*!pre*/, false /*!report*/)) {
        info("debug build directory detected: switching to debug build");
        use_debug = true;
    }

#ifdef DRCONFIG
    if (verbose) {
        dr_get_config_dir(global, true /*use temp*/, buf, BUFFER_SIZE_ELEMENTS(buf));
        info("configuration directory is \"%s\"", buf);
    }
    if (action == action_register) {
        if (!register_proc(process, 0, global, dr_root, dr_mode, use_debug, dr_platform,
                           extra_ops, drlib_path, drlib_alt_path))
            die();
        for (j = 0; j < num_clients; j++) {
            if (!register_client(process, 0, global, dr_platform, client_ids[j],
                                 client_paths[j], alt_bitwidth[j], client_options[j]))
                die();
        }
    } else if (action == action_unregister) {
        if (!unregister_proc(process, 0, global, dr_platform))
            die();
    }
#    ifndef WINDOWS
    else {
        usage(false, "no action specified");
    }
#    else /* WINDOWS */
    /* FIXME i#840: Nudge NYI on Linux. */
    else if (action == action_nudge) {
        int count = 1;
        dr_config_status_t res = DR_SUCCESS;
        if (nudge_all)
            res = dr_nudge_all(nudge_id, nudge_arg, nudge_timeout, &count);
        else if (nudge_pid != 0) {
            res = dr_nudge_pid(nudge_pid, nudge_id, nudge_arg, nudge_timeout);
            if (res == DR_NUDGE_PID_NOT_INJECTED)
                printf("process " PIDFMT " is not running under DR\n", nudge_pid);
            if (res != DR_SUCCESS && res != DR_NUDGE_TIMEOUT) {
                count = 0;
            }
        } else
            res = dr_nudge_process(process, nudge_id, nudge_arg, nudge_timeout, &count);

        printf("%d processes nudged\n", count);
        if (res == DR_NUDGE_TIMEOUT)
            printf("timed out waiting for nudge to complete\n");
        else if (res != DR_SUCCESS)
            printf("nudge operation failed, verify permissions and parameters.\n");
    }
#        ifdef WINDOWS
    /* FIXME i#840: Process iterator NYI for Linux. */
    else if (action == action_list) {
        if (!list_registered)
            list_process(process, global, dr_platform, NULL);
        else /* list all */ {
            dr_registered_process_iterator_t *iter =
                dr_registered_process_iterator_start(dr_platform, global);
            printf("Registered %s processes for %s\n", global ? "global" : "local",
                   platform_name(dr_platform));
            while (dr_registered_process_iterator_hasnext(iter))
                list_process(NULL, global, dr_platform, iter);
            dr_registered_process_iterator_stop(iter);
        }
    }
    /* FIXME i#95: Process detach NYI for UNIX. */
    else if (detach_pid != 0) {
        dr_config_status_t res = detach(detach_pid, TRUE, detach_timeout);
        if (res != DR_SUCCESS)
            error("unable to detach: check pid and system ptrace permissions");
    }
#        endif
    else if (!syswide_on && !syswide_off) {
        usage(false, "no action specified");
    }
    if (syswide_on) {
        DWORD platform;
        if (get_platform(&platform) != ERROR_SUCCESS)
            platform = PLATFORM_UNKNOWN;
        if (platform >= PLATFORM_WIN_8 &&
            IF_X64_ELSE(
                dr_platform != DR_PLATFORM_32BIT,
                (dr_platform == DR_PLATFORM_64BIT || !is_wow64(GetCurrentProcess())))) {
            /* FIXME i#1522: enable AppInit for non-WOW64 on win8+ */
            error("syswide_on is not yet supported on Windows 8+ non-WOW64");
            die();
        }
        if (!check_dr_root(dr_root, false, dr_platform, true /*pre*/, true /*report*/))
            die();
        /* If this is the first setting of AppInit on NT, warn about reboot */
        if (!dr_syswide_is_on(dr_platform, dr_root)) {
            if (platform == PLATFORM_WIN_NT_4) {
                warn("on Windows NT, applications will not be taken over until "
                     "reboot");
            } else if (platform >= PLATFORM_WIN_7) {
                /* i#323 will fix this but good to warn the user */
                warn("on Windows 7+, syswide_on relaxes system security by removing "
                     "certain code signing requirements");
            }
        }
        if (dr_register_syswide(dr_platform, dr_root) != ERROR_SUCCESS) {
            /* PR 233108: try to give more info on whether a privilege failure */
            warn("syswide set failed: re-run as administrator");
        }
    }
    if (syswide_off) {
        if (dr_unregister_syswide(dr_platform, dr_root) != ERROR_SUCCESS) {
            /* PR 233108: try to give more info on whether a privilege failure */
            warn("syswide set failed: re-run as administrator");
        }
    }
#    endif /* WINDOWS */
    exitcode = 0;
    goto cleanup;
#else /* DRCONFIG */
    if (!global) {
        /* i#939: attempt to work w/o any HOME/USERPROFILE by using a temp dir */
        dr_get_config_dir(global, true /*use temp*/, buf, BUFFER_SIZE_ELEMENTS(buf));
        info("configuration directory is \"%s\"", buf);
    }
#    ifdef UNIX
    /* i#1676: detect whether under gdb */
    char path_buf[MAXIMUM_PATH];
    _snprintf(path_buf, BUFFER_SIZE_ELEMENTS(path_buf), "/proc/%d/exe", getppid());
    NULL_TERMINATE_BUFFER(path_buf);
    i = readlink(path_buf, buf, BUFFER_SIZE_ELEMENTS(buf));
    if (i > 0) {
        if (i < BUFFER_SIZE_ELEMENTS(buf))
            buf[i] = '\0';
        else
            NULL_TERMINATE_BUFFER(buf);
    }
    /* On Linux, we use exec by default to create the app process.  This matches
     * our drrun shell script and makes scripting easier for the user.
     */
    if (limit == 0 && !use_ptrace && !kill_group) {
        info("will exec %s", app_name);
        errcode = dr_inject_prepare_to_exec(app_name, app_argv, &inject_data);
    } else if (attach_pid != 0) {
        /* We always try to avoid hanging on a blocked syscall. */
        errcode = dr_inject_prepare_to_attach(attach_pid, app_name,
                                              /*wait_syscall=*/false, &inject_data);
    } else
#    elif defined(WINDOWS)
    if (attach_pid != 0) {
        errcode = dr_inject_process_attach(attach_pid, &inject_data, &app_name);
    } else
#    endif /* WINDOWS */
    {
        errcode = dr_inject_process_create(app_name, app_argv, &inject_data);
        info("created child with pid " PIDFMT " for %s",
             dr_inject_get_process_id(inject_data), app_name);
    }
#    ifdef WINDOWS
    if (use_late_injection)
        dr_inject_use_late_injection(inject_data);
#    endif
#    ifdef UNIX
    if (limit != 0 && kill_group) {
        /* Move the child to its own process group. */
        process_id_t child_pid = dr_inject_get_process_id(inject_data);
        int res = setpgid(child_pid, child_pid);
        if (res < 0) {
            perror("ERROR in setpgid");
            goto error;
        }
    }
#    endif
    if (errcode ==
        ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE
            /* Check whether -32/64 is specified, but only for Linux as we do
             * not support cross-arch on Windows yet (i#803).
             */
            IF_UNIX(&&dr_platform != IF_X64_ELSE(DR_PLATFORM_32BIT, DR_PLATFORM_64BIT))) {
        if (nocheck) {
            /* Allow override for cases like i#1224 */
            warn("Target process %s appears to be for the wrong architecture.", app_name);
            warn("Attempting to run anyway, but it may run natively if injection fails.");
            errcode = 0;
        } else {
            /* For Windows, better error message than the FormatMessage */
            error("Target process %s is for the wrong architecture", app_name);
            goto error; /* the process was still created */
        }
    }
    if (errcode != 0 IF_UNIX(&&errcode != ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE)) {
        IF_WINDOWS(int sofar =)
        _snprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
                  "Failed to create process for \"%s\": ", app_name);
#    ifdef WINDOWS
        if (sofar > 0) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPTSTR)buf + sofar,
                          BUFFER_SIZE_ELEMENTS(buf) - sofar * sizeof(char), NULL);
        }
#    endif /* WINDOWS */
        NULL_TERMINATE_BUFFER(buf);
        error("%s", buf);
        goto error;
    }

    /* i#200/PR 459481: communicate child pid via file */
    if (pidfile != NULL)
        write_pid_to_file(pidfile, dr_inject_get_process_id(inject_data));

#    ifdef DRRUN
    /* even if !inject we create a config file, for use running standalone API
     * apps.  if user doesn't want a config file, should use "drinject -noinject".
     */
    if (configure) {
        process = dr_inject_get_image_name(inject_data);
        if (!register_proc(process, dr_inject_get_process_id(inject_data), global,
                           dr_root, dr_mode, use_debug, dr_platform, extra_ops,
                           drlib_path, drlib_alt_path))
            goto error;
        for (j = 0; j < num_clients; j++) {
            if (!register_client(process, dr_inject_get_process_id(inject_data), global,
                                 dr_platform, client_ids[j], client_paths[j],
                                 alt_bitwidth[j], client_options[j]))
                goto error;
        }
    }
#    endif

#    ifdef UNIX
    if (use_ptrace) {
        if (!dr_inject_prepare_to_ptrace(inject_data)) {
            error("unable to use ptrace");
            goto error;
        } else {
            info("using ptrace to inject");
        }
    }
    if (kill_group) {
        /* Move the child to its own process group. */
        bool res = dr_inject_prepare_new_process_group(inject_data);
        if (!res) {
            error("error moving child to new process group");
            goto error;
        }
    }
#    endif

    if (inject && !dr_inject_process_inject(inject_data, force_injection, drlib_path)) {
#    ifdef DRRUN
        if (attach_pid != 0) {
            error("unable to attach; check pid and system ptrace permissions");
            goto error;
        }
        error("unable to inject: exec of |%s| failed", drlib_path);
#    else
        error("unable to inject: did you forget to run drconfig first?");
#    endif
        goto error;
    }

    IF_WINDOWS(start_time = time(NULL);)

    if (!dr_inject_process_run(inject_data)) {
        error("unable to run");
        goto error;
    }

#    ifdef WINDOWS
    if (limit == 0 && dr_inject_using_debug_key(inject_data)) {
        info("%s", "Using debugger key injection");
        limit = -1; /* no wait */
    }
#    endif

    if (limit >= 0) {
#    ifdef WINDOWS
        double wallclock;
#    endif
        uint64 limit_millis = limit * 1000;
        info("waiting %sfor app to exit...", (limit <= 0) ? "forever " : "");
        success = dr_inject_wait_for_child(inject_data, limit_millis);
#    ifdef WINDOWS
        end_time = time(NULL);
        wallclock = difftime(end_time, start_time);
        if (showstats || showmem)
            dr_inject_print_stats(inject_data, (int)wallclock, showstats, showmem);
#    endif
        if (!success)
            info("timeout after %d seconds\n", limit);
    } else {
        success = true; /* Don't kill the child if we're not waiting. */
    }

    exitcode = dr_inject_process_exit(
        inject_data, attach_pid != 0 ? false : !success /*kill process*/);

    if (limit < 0)
        exitcode = 0; /* Return success if we didn't wait. */

    if (exit0)
        exitcode = 0;
    goto cleanup;

error:
    /* we created the process suspended so if we later had an error be sure
     * to kill it instead of leaving it hanging
     */
    if (inject_data != NULL) {
        dr_inject_process_exit(inject_data,
                               attach_pid != 0 ? false : true /*kill process*/);
    }
#    ifdef DRRUN
    if (tofree != NULL)
        free(tofree);
#    endif
    exitcode = 1;
#endif /* !DRCONFIG */

cleanup:
    sc = drfront_cleanup_args(argv, argc);
    if (sc != DRFRONT_SUCCESS)
        fatal("failed to free memory for args: %d", sc);
    /* FIXME i#840: We can't actually match exit status on Linux perfectly
     * since the kernel reserves most of the bits for signal codes.  At the
     * very least, we should ensure if the app exits with a signal we exit
     * non-zero.
     */
    return exitcode;
}
