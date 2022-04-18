/* **********************************************************
 * Copyright (c) 2013-2018 Google, Inc.  All rights reserved.
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

/* Code Manipulation API Sample:
 * stats.c
 *
 * Serves as an example of how to create custom statistics and export
 * them in shared memory.  It uses the Windows API, which will be redirected
 * by DynamoRIO in order to maintain isolation and transparency.
 * The current version only supports viewing statistics from processes
 * in the same session and by the same user.  Look at older versions of
 * the sources for statistics that can be read across sessions (but only
 * when the target process and the viewer are run as administrator).
 *
 * These statistics can be viewed using the provided statistics
 * viewer.  This code also documents the official shared memory layout
 * required by the statistics viewer.
 */

#define _CRT_SECURE_NO_DEPRECATE 1

#include "dr_api.h"
#include "drmgr.h"
#include "drx.h"
#include "utils.h"
#include <stddef.h> /* for offsetof */
#include <wchar.h>  /* _snwprintf */

#ifndef WINDOWS
#    error WINDOWS-only!
#endif

#define ALIGNED(x, alignment) ((((ptr_uint_t)x) & ((alignment)-1)) == 0)

/* We export a set of stats in shared memory.
 * drstats.exe reads and displays them.
 */
const char *stat_names[] = {
    /* drstats.exe displays CLIENTSTAT_NAME_MAX_LEN chars for each name */
    "Instructions",
    "Floating point instrs",
    "System calls",
};

/* We do not prefix "Global\", so these stats are NOT visible across sessions
 * (that requires running as admiministrator, on Vista+).
 * On NT these prefixes are not supported.
 */
#define CLIENT_SHMEM_KEY_NT_L L"DynamoRIO_Client_Statistics"
#define CLIENT_SHMEM_KEY_L L"Local\\DynamoRIO_Client_Statistics"
#define CLIENTSTAT_NAME_MAX_LEN 47
#define NUM_STATS (sizeof(stat_names) / sizeof(char *))

/* Statistics are all 64-bit for x64.  At some point we'll add per-stat
 * typing, but for now we have identical types dependent on the platform.
 */
#ifdef X86_64
typedef int64 stats_int_t;
#    define STAT_FORMAT_CODE UINT64_FORMAT_CODE
#else
typedef int stats_int_t;
#    define STAT_FORMAT_CODE "d"
#endif

/* We allocate this struct in the shared memory: */
typedef struct _client_stats_t {
    uint num_stats;
    bool exited;
    process_id_t pid;
    /* We need a copy of all the names here. */
    char names[NUM_STATS][CLIENTSTAT_NAME_MAX_LEN];
    stats_int_t num_instrs;
    stats_int_t num_flops;
    stats_int_t num_syscalls;
} client_stats_t;

/* Counters that we collect for each basic block. */
typedef struct _per_bb_data_t {
    uint num_instrs;
    uint num_flops;
    uint num_syscalls;
} per_bb_data_t;

/* we directly increment the global counters using a lock prefix */
static client_stats_t *stats;

/***************************************************************************/
/* Shared memory setup */

/* We have multiple shared memories: one that holds the count of
 * statistics instances, and then one per statistics struct.
 */
static HANDLE shared_map_count;
static PVOID shared_view_count;
static int *shared_count;
static HANDLE shared_map;
static PVOID shared_view;
#define KEYNAME_MAXLEN 128
static wchar_t shared_keyname[KEYNAME_MAXLEN];

/* Returns current contents of addr and replaces contents with value. */
static uint
atomic_swap(void *addr, uint value)
{
    return _InterlockedExchange((long *)addr, value);
}

static bool
is_windows_NT(void)
{
    dr_os_version_info_t ver;
    return (dr_get_os_version(&ver) && ver.version == DR_WINDOWS_VERSION_NT);
}

static client_stats_t *
shared_memory_init(void)
{
    bool is_NT = is_windows_NT();
    int num;
    int pos;
    /* We do not want to rely on the registry.
     * Instead, a piece of shared memory with the key base name holds the
     * total number of stats instances.
     */
    shared_map_count = CreateFileMappingW(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(client_stats_t),
        is_NT ? CLIENT_SHMEM_KEY_NT_L : CLIENT_SHMEM_KEY_L);
    DR_ASSERT(shared_map_count != NULL);
    shared_view_count =
        MapViewOfFile(shared_map_count, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    DR_ASSERT(shared_view_count != NULL);
    shared_count = (int *)shared_view_count;
    /* ASSUMPTION: memory is initialized to 0!
     * otherwise our protocol won't work
     * it's hard to build a protocol to initialize it to 0 -- if you want
     * to add one, feel free, but make sure it's correct
     */
    do {
        pos = (int)atomic_swap(shared_count, (uint)-1);
        /* if get -1 back, someone else is looking at it */
    } while (pos == -1);
    /* now increment it */
    atomic_swap(shared_count, pos + 1);

    num = 0;
    while (1) {
        _snwprintf(shared_keyname, KEYNAME_MAXLEN, L"%s.%03d",
                   is_NT ? CLIENT_SHMEM_KEY_NT_L : CLIENT_SHMEM_KEY_L, num);
        shared_map = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                        sizeof(client_stats_t), shared_keyname);
        if (shared_map != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
            dr_close_file(shared_map);
            shared_map = NULL;
        }
        if (shared_map != NULL)
            break;
        num++;
    }
    dr_log(NULL, DR_LOG_ALL, 1, "Shared memory key is: \"%S\"\n", shared_keyname);
#ifdef SHOW_RESULTS
    dr_fprintf(STDERR, "Shared memory key is: \"%S\"\n", shared_keyname);
#endif
    shared_view = MapViewOfFile(shared_map, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    DR_ASSERT(shared_view != NULL);
    return (client_stats_t *)shared_view;
}

static void
shared_memory_exit(void)
{
    int pos;
    stats->exited = true;
    /* close down statistics */
    UnmapViewOfFile(shared_view);
    dr_close_file(shared_map);
    /* decrement count, then unmap */
    do {
        pos = atomic_swap(shared_count, (uint)-1);
        /* if get -1 back, someone else is looking at it */
    } while (pos == -1);
    /* now increment it */
    atomic_swap(shared_count, pos - 1);
    UnmapViewOfFile(shared_view_count);
    CloseHandle(shared_map_count);
}
/***************************************************************************/

static void
event_exit(void);

static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data);

static dr_emit_flags_t
event_insert_instrumentation(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                             bool for_trace, bool translating, void *user_data);

static client_id_t my_id;

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    uint i;

    dr_set_client_name("DynamoRIO Sample Client 'stats'", "http://dynamorio.org/issues");
    my_id = id;
    /* Make it easy to tell by looking at the log which client executed. */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'stats' initializing\n");

    if (!drmgr_init())
        DR_ASSERT(false);
    drx_init();

    stats = shared_memory_init();
    memset(stats, 0, sizeof(stats));
    stats->num_stats = NUM_STATS;
    stats->pid = dr_get_process_id();
    for (i = 0; i < NUM_STATS; i++) {
        strncpy(stats->names[i], stat_names[i], CLIENTSTAT_NAME_MAX_LEN);
        stats->names[i][CLIENTSTAT_NAME_MAX_LEN - 1] = '\0';
    }
    dr_register_exit_event(event_exit);
    if (!drmgr_register_bb_instrumentation_event(event_analyze_bb,
                                                 event_insert_instrumentation, NULL))
        DR_ASSERT(false);
}

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#else
#    define IF_WINDOWS(x) /* nothing */
#endif

static void
event_exit(void)
{
    file_t f;
    /* Display the results! */
    char msg[512];
    int len;
    len = dr_snprintf(msg, sizeof(msg) / sizeof(msg[0]),
                      "Instrumentation results:\n"
                      "  saw %" STAT_FORMAT_CODE " flops\n",
                      stats->num_flops);
    DR_ASSERT(len > 0);
    msg[sizeof(msg) / sizeof(msg[0]) - 1] = '\0';
#ifdef SHOW_RESULTS
    DISPLAY_STRING(msg);
#endif /* SHOW_RESULTS */

    /* On Windows we need an absolute path so we place it in
     * the same directory as our library.
     */
    f = log_file_open(my_id, NULL, NULL /* client lib path */, "stats", 0);
    DR_ASSERT(f != INVALID_FILE);
    dr_fprintf(f, "%s\n", msg);
    dr_close_file(f);

    shared_memory_exit();

    drx_exit();
    if (!drmgr_unregister_bb_instrumentation_event(event_analyze_bb))
        DR_ASSERT(false);
    drmgr_exit();
}

/* This event is passed the instruction list for the whole bb. */
static dr_emit_flags_t
event_analyze_bb(void *drcontext, void *tag, instrlist_t *bb, bool for_trace,
                 bool translating, void **user_data)
{
    /* Count the instructions and pass the result to event_insert_instrumentation. */
    per_bb_data_t *per_bb = dr_thread_alloc(drcontext, sizeof(*per_bb));
    instr_t *instr;
    uint num_instrs = 0;
    uint num_flops = 0;
    uint num_syscalls = 0;
    dr_fp_type_t fp_type;

    for (instr = instrlist_first_app(bb); instr != NULL;
         instr = instr_get_next_app(instr)) {
        num_instrs++;
        if (instr_is_floating_ex(instr, &fp_type) &&
            /* We exclude loads and stores (and reg-reg moves) and state preservation */
            (fp_type == DR_FP_CONVERT || fp_type == DR_FP_MATH)) {
#ifdef VERBOSE
            dr_print_instr(drcontext, STDOUT, instr, "Found flop: ");
#endif
            num_flops++;
        }
        if (instr_is_syscall(instr)) {
            num_syscalls++;
        }
    }

    per_bb->num_instrs = num_instrs;
    per_bb->num_flops = num_flops;
    per_bb->num_syscalls = num_syscalls;
    *(per_bb_data_t **)user_data = per_bb;

    return DR_EMIT_DEFAULT;
}

/* This event is called separately for each individual instruction in the bb. */
static dr_emit_flags_t
event_insert_instrumentation(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                             bool for_trace, bool translating, void *user_data)
{
    per_bb_data_t *per_bb = (per_bb_data_t *)user_data;
    /* We increment the per-bb counters just once, at the top of the bb. */
    if (drmgr_is_first_instr(drcontext, instr)) {
        /* drx will analyze whether to save the flags for us. */
        uint flags = DRX_COUNTER_LOCK;
        if (per_bb->num_instrs > 0) {
            drx_insert_counter_update(drcontext, bb, instr, SPILL_SLOT_MAX + 1,
                                      &stats->num_instrs, per_bb->num_instrs, flags);
        }
        if (per_bb->num_flops > 0) {
            drx_insert_counter_update(drcontext, bb, instr, SPILL_SLOT_MAX + 1,
                                      &stats->num_flops, per_bb->num_flops, flags);
        }
        if (per_bb->num_syscalls > 0) {
            drx_insert_counter_update(drcontext, bb, instr, SPILL_SLOT_MAX + 1,
                                      &stats->num_syscalls, per_bb->num_syscalls, flags);
        }
    }
    if (drmgr_is_last_instr(drcontext, instr))
        dr_thread_free(drcontext, per_bb, sizeof(*per_bb));
    return DR_EMIT_DEFAULT;
}
