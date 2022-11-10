/* **********************************************************
 * Copyright (c) 2010-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * dynamo.c -- initialization and cleanup routines for DynamoRIO
 */

#include "globals.h"
#include "configure_defines.h"
#include "link.h"
#include "fragment.h"
#include "fcache.h"
#include "emit.h"
#include "dispatch.h"
#include "utils.h"
#include "monitor.h"
#include "vmareas.h"
#ifdef SIDELINE
#    include "sideline.h"
#endif
#ifdef PAPI
#    include "perfctr.h"
#endif
#include "instrument.h"
#include "hotpatch.h"
#include "moduledb.h"
#include "module_shared.h"
#include "synch.h"
#include "native_exec.h"
#include "jit_opt.h"

#ifdef ANNOTATIONS
#    include "annotations.h"
#endif

#ifdef WINDOWS
/* for close handle, duplicate handle, free memory and constants associated with them
 */
/* also for nt_terminate_process_for_app() */
#    include "ntdll.h"
#    include "nudge.h" /* to get generic_nudge_target() address for an assert */
#endif

#ifdef RCT_IND_BRANCH
#    include "rct.h"
#endif

#include "perscache.h"

#ifdef VMX86_SERVER
#    include "vmkuw.h"
#endif

#ifndef STANDALONE_UNIT_TEST
#    ifdef __AVX512F__
#        error "DynamoRIO core should run without AVX-512 instructions to remain \
portable and to avoid frequency scaling."
#    endif
#endif

/* global thread-shared variables */
bool dynamo_initialized = false;
static bool dynamo_options_initialized = false;
bool dynamo_heap_initialized = false;
bool dynamo_started = false;
bool automatic_startup = false;
bool control_all_threads = false;
/* On Windows we can't really tell attach apart from our default late
 * injection, and we do see early threads in place which is the point of
 * this flag: so we always set it.
 */
bool dynamo_control_via_attach = IF_WINDOWS_ELSE(true, false);
#ifdef WINDOWS
bool dr_early_injected = false;
int dr_early_injected_location = INJECT_LOCATION_Invalid;
bool dr_earliest_injected = false;
static void *dr_earliest_inject_args;

/* should be set if we are controlling the primary thread, either by
 * injecting initially (!dr_injected_secondary_thread), or by retaking
 * over (dr_late_injected_primary_thread).  Used only for debugging
 * purposes, yet can't rely on !dr_injected_secondary_thread very
 * early in the process
 */
bool dr_injected_primary_thread = false;
bool dr_injected_secondary_thread = false;

/* should be set once we retakeover the primary thread for -inject_primary */
bool dr_late_injected_primary_thread = false;
#endif /* WINDOWS */
/* flags to indicate when DR is being initialized / exited using the API */
bool dr_api_entry = false;
bool dr_api_exit = false;
#ifdef RETURN_AFTER_CALL
bool dr_preinjected = false;
#endif /* RETURN_AFTER_CALL */
#ifdef UNIX
static bool dynamo_exiting = false;
#endif
bool dynamo_exited = false;
bool dynamo_exited_all_other_threads = false;
bool dynamo_exited_and_cleaned = false;
#ifdef DEBUG
bool dynamo_exited_log_and_stats = false;
#endif
/* Only used in release build to decide whether synch is needed, justifying
 * its placement in .nspdata.  If we use it for more we should protect it.
 */
DECLARE_NEVERPROT_VAR(bool dynamo_all_threads_synched, false);
bool dynamo_resetting = false;
bool standalone_library = false;
static int standalone_init_count;
#ifdef UNIX
bool post_execve = false;
#endif
/* initial stack so we don't have to use app's */
byte *d_r_initstack;

event_t dr_app_started;
event_t dr_attach_finished;

#ifdef WINDOWS
/* PR203701: separate stack for error reporting when the dstack is exhausted */
#    define EXCEPTION_STACK_SIZE (2 * PAGE_SIZE)
DECLARE_NEVERPROT_VAR(byte *exception_stack, NULL);
#endif

/*******************************************************/
/* separate segment of Non-Self-Protected data to avoid data section
 * protection issues -- we need to write to these vars in bootstrapping
 * spots where we cannot unprotect first
 */
START_DATA_SECTION(NEVER_PROTECTED_SECTION, "w");

/* spinlock used in assembly trampolines when we can't spare registers for more */
mutex_t initstack_mutex IF_AARCH64(__attribute__((aligned(8))))
    VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = INIT_SPINLOCK_FREE(initstack_mutex);
byte *initstack_app_xsp VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = 0;
/* keeps track of how many threads are in cleanup_and_terminate */
volatile int exiting_thread_count VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = 0;
/* Tracks newly created threads not yet on the all_threads list. */
volatile int uninit_thread_count VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = 0;

/* This is unprotected to allow stats to be written while the data
 * segment is still protected (right now the only ones are selfmod stats)
 */
static dr_statistics_t nonshared_stats VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = {
    { 0 },
};

/* Each lock protects its corresponding datasec_start, datasec_end, and
 * datasec_writable variables.
 */
static mutex_t
    datasec_lock[DATASEC_NUM] VAR_IN_SECTION(NEVER_PROTECTED_SECTION) = { { 0 } };

/* back to normal section */
END_DATA_SECTION()
/*******************************************************/

/* Like a recursive lock: 0==readonly, 1+=writable.
 * This would be a simple array, but we need each in its own protected
 * section, as this could be exploited.
 */
const uint datasec_writable_neverprot = 1; /* always writable */
uint datasec_writable_rareprot = 1;
DECLARE_FREQPROT_VAR(uint datasec_writable_freqprot, 1);
DECLARE_CXTSWPROT_VAR(uint datasec_writable_cxtswprot, 1);

static app_pc datasec_start[DATASEC_NUM];
static app_pc datasec_end[DATASEC_NUM];

const uint DATASEC_SELFPROT[] = {
    0,
    SELFPROT_DATA_RARE,
    SELFPROT_DATA_FREQ,
    SELFPROT_DATA_CXTSW,
};

const char *const DATASEC_NAMES[] = {
    NEVER_PROTECTED_SECTION,
    RARELY_PROTECTED_SECTION,
    FREQ_PROTECTED_SECTION,
    CXTSW_PROTECTED_SECTION,
};

/* kept in unprotected heap to avoid issues w/ data segment being RO */
typedef struct _protect_info_t {
    /* FIXME: this needs to be a recursive lock to handle signals
     * and exceptions!
     */
    mutex_t lock;
    int num_threads_unprot; /* # threads in DR code */
    int num_threads_suspended;
} protect_info_t;
static protect_info_t *protect_info;

static void
data_section_init(void);
static void
data_section_exit(void);

#ifdef DEBUG /*************************/

#    include <time.h>

/* FIXME: not all dynamo_options references are #ifdef DEBUG
 * are we trying to hardcode the options for a release build?
 */
#    ifdef UNIX
/* linux include files for mmap stuff*/
#        include <sys/ipc.h>
#        include <sys/types.h>
#        include <unistd.h>
#    endif

static uint starttime;

file_t main_logfile = INVALID_FILE;

#endif /* DEBUG ****************************/

dr_statistics_t *d_r_stats = NULL;

DECLARE_FREQPROT_VAR(static int num_known_threads, 0);
#ifdef UNIX
/* i#237/PR 498284: vfork threads that execve need to be separately delay-freed */
DECLARE_FREQPROT_VAR(int num_execve_threads, 0);
#endif
DECLARE_FREQPROT_VAR(static uint threads_ever_count, 0);

/* FIXME : not static so os.c can hand walk it for dump core */
/* FIXME: use new generic_table_t and generic_hash_* routines */
thread_record_t **all_threads; /* ALL_THREADS_HASH_BITS-bit addressed hash table */

/* these locks are used often enough that we put them in .cspdata: */

/* not static so can be referenced in win32/os.c for SuspendThread handling,
 * FIXME : is almost completely redundant in usage with thread_initexit_lock
 * maybe replace this lock with thread_initexit_lock? */
DECLARE_CXTSWPROT_VAR(mutex_t all_threads_lock, INIT_LOCK_FREE(all_threads_lock));
/* used for synch to prevent thread creation/deletion in critical periods
 * due to its use for flushing, this lock cannot be held while couldbelinking!
 */
DECLARE_CXTSWPROT_VAR(mutex_t thread_initexit_lock, INIT_LOCK_FREE(thread_initexit_lock));

/* recursive to handle signals/exceptions while in DR code */
DECLARE_CXTSWPROT_VAR(static recursive_lock_t thread_in_DR_exclusion,
                      INIT_RECURSIVE_LOCK(thread_in_DR_exclusion));

static thread_synch_state_t
exit_synch_state(void);

static void
synch_with_threads_at_exit(thread_synch_state_t synch_res, bool pre_exit);

static void
delete_dynamo_context(dcontext_t *dcontext, bool free_stack);

/****************************************************************************/
#ifdef DEBUG

static const char *
main_logfile_name(void)
{
    return get_app_name_for_path();
}

static const char *
thread_logfile_name(void)
{
    return "log";
}

#endif /* DEBUG */
/****************************************************************************/

static void
statistics_pre_init(void)
{
    /* until it's set up for real, point at static var
     * really only logmask and loglevel are meaningful, so be careful!
     * statistics_init and create_log_directory are the only routines that
     * use stats before it's set up for real, currently
     */
    /* The indirection here is left over from when we used to allow alternative
     * locations for stats (namely shared memory for the old MIT gui). */
    d_r_stats = &nonshared_stats;
    d_r_stats->process_id = get_process_id();
    strncpy(d_r_stats->process_name, get_application_name(), MAXIMUM_PATH);
    d_r_stats->process_name[MAXIMUM_PATH - 1] = '\0';
    ASSERT(strlen(d_r_stats->process_name) > 0);
    d_r_stats->num_stats = 0;
}

static void
statistics_init(void)
{
    /* should have called statistics_pre_init() first */
    ASSERT(d_r_stats == &nonshared_stats);
    ASSERT(d_r_stats->num_stats == 0);
#ifndef DEBUG
    if (!DYNAMO_OPTION(global_rstats)) {
        /* references to stat values should return 0 (static var) */
        return;
    }
#endif
    d_r_stats->num_stats = 0
#ifdef DEBUG
#    define STATS_DEF(desc, name) +1
#else
#    define RSTATS_DEF(desc, name) +1
#endif
#include "statsx.h"
#undef STATS_DEF
#undef RSTATS_DEF
        ;
    /* We inline the stat description to make it easy for external processes
     * to view our stats: they don't have to chase pointers, and we could put
     * this in shared memory easily.  However, we do waste some memory, but
     * not much in release build.
     */
#ifdef DEBUG
#    define STATS_DEF(desc, statname)                                   \
        strncpy(d_r_stats->statname##_pair.name, desc,                  \
                BUFFER_SIZE_ELEMENTS(d_r_stats->statname##_pair.name)); \
        NULL_TERMINATE_BUFFER(d_r_stats->statname##_pair.name);
#else
#    define RSTATS_DEF(desc, statname)                                  \
        strncpy(d_r_stats->statname##_pair.name, desc,                  \
                BUFFER_SIZE_ELEMENTS(d_r_stats->statname##_pair.name)); \
        NULL_TERMINATE_BUFFER(d_r_stats->statname##_pair.name);
#endif
#include "statsx.h"
#undef STATS_DEF
#undef RSTATS_DEF
}

static void
statistics_exit(void)
{
    if (doing_detach)
        memset(d_r_stats, 0, sizeof(*d_r_stats)); /* for possible re-attach */
    d_r_stats = NULL;
}

dr_statistics_t *
get_dr_stats(void)
{
    return d_r_stats;
}

/* initialize per-process dynamo state; this must be called before any
 * threads are created and before any other API calls are made;
 * returns zero on success, non-zero on failure
 */
DYNAMORIO_EXPORT int
dynamorio_app_init(void)
{
    dynamorio_app_init_part_one_options();
    return dynamorio_app_init_part_two_finalize();
}

void
dynamorio_app_init_part_one_options(void)
{
    if (dynamo_initialized || dynamo_options_initialized) {
        if (standalone_library) {
            REPORT_FATAL_ERROR_AND_EXIT(STANDALONE_ALREADY, 2, get_application_name(),
                                        get_application_pid());
        }
    } else /* we do enter if nullcalls is on */ {

#ifdef UNIX
        os_page_size_init((const char **)our_environ, is_our_environ_followed_by_auxv());
#endif
#ifdef WINDOWS
        /* MUST do this before making any system calls */
        syscalls_init();
#endif
        /* avoid time() for libc independence */
        DODEBUG(starttime = query_time_seconds(););

#ifdef UNIX
        if (getenv(DYNAMORIO_VAR_EXECVE) != NULL) {
            post_execve = true;
#    ifdef VMX86_SERVER
            /* PR 458917: our gdt slot was not cleared on exec so we need to
             * clear it now to ensure we don't leak it and eventually run out of
             * slots.  We could alternatively call os_tls_exit() prior to
             * execve, since syscalls use thread-private fcache_enter, but
             * complex to recover from execve failure, so instead we pass which
             * TLS index we had.
             */
            os_tls_pre_init(atoi(getenv(DYNAMORIO_VAR_EXECVE)));
#    endif
            /* important to remove it, don't want to propagate to forked children, etc. */
            /* i#909: unsetenv is unsafe as it messes up auxv access, so we disable */
            disable_env(DYNAMORIO_VAR_EXECVE);
            /* check that it's gone: we've had problems with unsetenv */
            ASSERT(getenv(DYNAMORIO_VAR_EXECVE) == NULL);
        } else
            post_execve = false;
#endif

            /* default non-zero dynamo settings (options structure is
             * initialized to 0 automatically)
             */
#ifdef DEBUG
#    ifndef INTERNAL
        nonshared_stats.logmask = LOG_ALL_RELEASE;
#    else
        nonshared_stats.logmask = LOG_ALL;
#    endif
        statistics_pre_init();
#endif

        d_r_config_init();
        options_init();
#ifdef WINDOWS
        syscalls_init_options_read(); /* must be called after options_init
                                       * but before init_syscall_trampolines */
#endif
        utils_init();
        data_section_init();

#ifdef DEBUG
        /* decision: nullcalls WILL create a dynamorio.log file and
         * fill it with perfctr stats!
         */
        if (d_r_stats->loglevel > 0) {
            main_logfile = open_log_file(main_logfile_name(), NULL, 0);
            LOG(GLOBAL, LOG_TOP, 1, "global log file fd=%d\n", main_logfile);
        } else {
            /* loglevel 0 means we don't create a log file!
             * if the loglevel is later raised, too bad!  it all goes to stderr!
             * N.B.: when checking for no logdir, we check for empty string or
             * first char '<'!
             */
            strncpy(d_r_stats->logdir, "<none (loglevel was 0 on startup)>",
                    MAXIMUM_PATH - 1);
            d_r_stats->logdir[MAXIMUM_PATH - 1] = '\0'; /* if max no null */
            main_logfile = INVALID_FILE;
        }

#    ifdef PAPI
        /* setup hardware performance counting */
        hardware_perfctr_init();
#    endif

        DOLOG(1, LOG_TOP, { print_version_and_app_info(GLOBAL); });

        /* now exit if nullcalls, now that perfctrs are set up */
        if (INTERNAL_OPTION(nullcalls)) {
            return;
        }

        LOG(GLOBAL, LOG_TOP, 1, PRODUCT_NAME "'s stack size: %d Kb\n",
            DYNAMORIO_STACK_SIZE / 1024);
#endif /* !DEBUG */

        /* set up exported statistics struct */
#ifndef DEBUG
        statistics_pre_init();
#endif
        statistics_init();

        dynamo_options_initialized = true;
    }
}

int
dynamorio_app_init_part_two_finalize(void)
{
    if (!dynamo_options_initialized) {
        /* Part one was never called. */
        return FAILURE;
    } else if (dynamo_initialized) {
        if (standalone_library) {
            REPORT_FATAL_ERROR_AND_EXIT(STANDALONE_ALREADY, 2, get_application_name(),
                                        get_application_pid());
        }
        /* Nop. */
    } else if (INTERNAL_OPTION(nullcalls)) {
        print_file(main_logfile, "** nullcalls is set, NOT taking over execution **\n\n");
        return SUCCESS;
    } else {
#ifdef VMX86_SERVER
        /* Must be before {vmm,d_r}_heap_init() */
        vmk_init_lib();
#endif

        /* initialize components (CAUTION: order is important here) */
        vmm_heap_init(); /* must be called even if not using vmm heap */
        /* PR 200207: load the client lib before callback_interception_init
         * since the client library load would hit our own hooks (xref hotpatch
         * cases about that) -- though -private_loader removes that issue.
         */
        instrument_load_client_libs();
        d_r_heap_init();
        dynamo_heap_initialized = true;

        /* The process start event should be done after d_r_os_init() but before
         * process_control_int() because the former initializes event logging
         * and the latter can kill the process if a violation occurs.
         */
        SYSLOG(SYSLOG_INFORMATION, INFO_PROCESS_START_CLIENT, 2, get_application_name(),
               get_application_pid());

#ifdef PROCESS_CONTROL
        if (IS_PROCESS_CONTROL_ON()) /* Case 8594. */
            process_control_init();
#endif

#ifdef WINDOWS
        /* Now that DR is set up, perform any final clean-up, before
         * we do our address space scans.
         */
        if (dr_earliest_injected)
            earliest_inject_cleanup(dr_earliest_inject_args);
#endif

        dynamo_vm_areas_init();
        d_r_decode_init();
        proc_init();
        modules_init(); /* before vm_areas_init() */
        d_r_os_init();
        config_heap_init(); /* after heap_init */

        /* Setup for handling faults in loader_init() */
        /* initial stack so we don't have to use app's
         * N.B.: we never de-allocate d_r_initstack (see comments in app_exit)
         */
        d_r_initstack = (byte *)stack_alloc(DYNAMORIO_STACK_SIZE, NULL);
        LOG(GLOBAL, LOG_SYNCH, 2, "d_r_initstack is " PFX "-" PFX "\n",
            d_r_initstack - DYNAMORIO_STACK_SIZE, d_r_initstack);

#ifdef WINDOWS
        /* PR203701: separate stack for error reporting when the
         * dstack is exhausted
         */
        exception_stack = (byte *)stack_alloc(EXCEPTION_STACK_SIZE, NULL);
#endif
#ifdef WINDOWS
        if (!INTERNAL_OPTION(noasynch)) {
            /* We split the hooks up: first we put in just Ki* to catch
             * exceptions in client init routines (PR 200207), but we don't want
             * syscall hooks so client init can scan syscalls.
             * Xref PR 216934 where this was originally down below 1st thread init,
             * before we had GLOBAL_DCONTEXT.
             */
            callback_interception_init_start();
        }
#endif /* WINDOWS */

        /* Set up any private-loader-related data we need before generating any
         * code, such as the private PEB on Windows.
         */
        loader_init_prologue();

        d_r_arch_init();
        synch_init();

#ifdef KSTATS
        kstat_init();
#endif
        d_r_monitor_init();
        fcache_init();
        d_r_link_init();
        fragment_init();
        moduledb_init();    /* before vm_areas_init, after heap_init */
        perscache_init();   /* before vm_areas_init */
        native_exec_init(); /* before vm_areas_init, after arch_init */

        if (!DYNAMO_OPTION(thin_client)) {
#ifdef HOT_PATCHING_INTERFACE
            /* must init hotp before vm_areas_init() calls find_executable_vm_areas() */
            if (DYNAMO_OPTION(hot_patching))
                hotp_init();
#endif
        }

#ifdef INTERNAL
        {
            char initial_options[MAX_OPTIONS_STRING];
            get_dynamo_options_string(&dynamo_options, initial_options,
                                      sizeof(initial_options), true);
            SYSLOG_INTERNAL_INFO("Initial options = %s", initial_options);
            DOLOG(1, LOG_TOP, {
                get_pcache_dynamo_options_string(&dynamo_options, initial_options,
                                                 sizeof(initial_options),
                                                 OP_PCACHE_LOCAL);
                LOG(GLOBAL, LOG_TOP, 1, "Initial pcache-affecting options = %s\n",
                    initial_options);
            });
        }
#endif /* INTERNAL */

        LOG(GLOBAL, LOG_TOP, 1, "\n");

        /* initialize thread hashtable */
        /* Note: for thin_client, this isn't needed if it is only going to
         * look for spawned processes; however, if we plan to promote from
         * thin_client to hotp_only mode (highly likely), this would be needed.
         * For now, leave it in there unless thin_client footprint becomes an
         * issue.
         */
        int size = HASHTABLE_SIZE(ALL_THREADS_HASH_BITS) * sizeof(thread_record_t *);
        all_threads =
            (thread_record_t **)global_heap_alloc(size HEAPACCT(ACCT_THREAD_MGT));
        memset(all_threads, 0, size);
        if (!INTERNAL_OPTION(nop_initial_bblock) IF_WINDOWS(
                || !check_sole_thread())) /* some other thread is already here! */
            bb_lock_start = true;

#ifdef SIDELINE
        /* initialize sideline thread after thread table is set up */
        if (dynamo_options.sideline)
            sideline_init();
#endif

        /* We can't clear this on detach like other vars b/c we need native threads
         * to continue to avoid safe_read_tls_magic() in is_thread_tls_initialized().
         * So we clear it on (re-)init in dynamorio_take_over_threads().
         * From now until then, we avoid races where another thread invokes a
         * safe_read during native signal delivery but we remove DR's handler before
         * it reaches there and it is delivered to the app's handler instead, kind
         * of like i#3535, by re-using the i#3535 mechanism of pointing at the only
         * thread who could possibly have a dcontext.
         * XXX: Should we rename this s/detacher_/singleton_/ or something?
         */
        detacher_tid = IF_UNIX_ELSE(get_sys_thread_id(), INVALID_THREAD_ID);
        /* thread-specific initialization for the first thread we inject in
         * (in a race with injected threads, sometimes it is not the primary thread)
         */
        /* i#117/PR 395156: it'd be nice to have mc here but would
         * require changing start/stop API
         */
        dynamo_thread_init(NULL, NULL, NULL, false);
        /* i#2751: we need TLS to be set up to relocate and call init funcs. */
        loader_init_epilogue(get_thread_private_dcontext());

        /* We move vm_areas_init() below dynamo_thread_init() so we can have
         * two things: 1) a dcontext and 2) a SIGSEGV handler, for TRY/EXCEPT
         * inside vm_areas_init() for PR 361594's probes and for d_r_safe_read().
         * This means vm_areas_thread_init() runs before vm_areas_init().
         */
        if (!DYNAMO_OPTION(thin_client)) {
            vm_areas_init();
#ifdef RCT_IND_BRANCH
            /* relies on is_in_dynamo_dll() which needs vm_areas_init */
            rct_init();
#endif
        } else {
            /* This is needed to handle exceptions in thin_client mode, mostly
             * internal ones, but can be app ones too. */
            dynamo_vm_areas_lock();
            find_dynamo_library_vm_areas();
            dynamo_vm_areas_unlock();
        }

#ifdef ANNOTATIONS
        annotation_init();
#endif
        jitopt_init();

        dr_attach_finished = create_broadcast_event();

        /* New client threads rely on dr_app_started being initialized, so do
         * that before initializing clients.
         */
        dr_app_started = create_broadcast_event();
        /* client last, in case it depends on other inits: must be after
         * dynamo_thread_init so the client can use a dcontext (PR 216936).
         * Note that we *load* the client library before installing our hooks,
         * but call the client's init routine afterward so that we correctly
         * report crashes (PR 200207).
         * Note: DllMain in client libraries can crash and we still won't
         *       report; better document that client libraries shouldn't have
         *       DllMain.
         */
        instrument_init();
        /* To give clients a chance to process pcaches as we load them, we
         * delay the loading until we've initialized the clients.
         */
        vm_area_delay_load_coarse_units();

#ifdef WINDOWS
        if (!INTERNAL_OPTION(noasynch))
            callback_interception_init_finish(); /* split for PR 200207: see above */
#endif

        if (SELF_PROTECT_ON_CXT_SWITCH) {
            protect_info = (protect_info_t *)global_unprotected_heap_alloc(
                sizeof(protect_info_t) HEAPACCT(ACCT_OTHER));
            ASSIGN_INIT_LOCK_FREE(protect_info->lock, protect_info);
            protect_info->num_threads_unprot = 0; /* ENTERING_DR() below will inc to 1 */
            protect_info->num_threads_suspended = 0;
            if (INTERNAL_OPTION(single_privileged_thread)) {
                /* FIXME: thread_initexit_lock must be a recursive lock! */
                ASSERT_NOT_IMPLEMENTED(false);
                /* grab the lock now -- the thread that is in dynamo must be holding
                 * the lock, and we are the initial thread in dynamo!
                 */
                d_r_mutex_lock(&thread_initexit_lock);
            }
            /* ENTERING_DR will increment, so decrement first
             * FIXME: waste of protection change since will nop-unprotect!
             */
            if (TEST(SELFPROT_DATA_CXTSW, DYNAMO_OPTION(protect_mask)))
                datasec_writable_cxtswprot = 0;
            /* FIXME case 8073: remove once freqprot not every cxt sw */
            if (TEST(SELFPROT_DATA_FREQ, DYNAMO_OPTION(protect_mask)))
                datasec_writable_freqprot = 0;
        }
        /* this thread is now entering DR */
        ENTERING_DR();

#ifdef WINDOWS
        if (DYNAMO_OPTION(early_inject)) {
            /* AFTER callback_interception_init and self protect init and
             * ENTERING_DR() */
            early_inject_init();
        }
#endif
    }

    dynamo_initialized = true;

    /* Protect .data, assuming all vars there have been initialized. */
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    /* internal-only options for testing run-once (case 3990) */
    if (INTERNAL_OPTION(unsafe_crash_process)) {
        SYSLOG_INTERNAL_ERROR("Crashing the process deliberately!");
        *((int *)PTR_UINT_MINUS_1) = 0;
    }
    if (INTERNAL_OPTION(unsafe_hang_process)) {
        event_t never_signaled = create_event();
        SYSLOG_INTERNAL_ERROR("Hanging the process deliberately!");
        wait_for_event(never_signaled, 0);
        destroy_event(never_signaled);
    }

    return SUCCESS;
}

#ifdef UNIX
void
dynamorio_fork_init(dcontext_t *dcontext)
{
    /* on a fork we want to re-initialize some data structures, especially
     * log files, which we want a separate directory for
     */
    thread_record_t **threads;
    int i, num_threads;
#    ifdef DEBUG
    char parent_logdir[MAXIMUM_PATH];
#    endif

    /* re-cache app name, etc. that are using parent pid before we
     * create log dirs (xref i#189/PR 452168)
     */
    os_fork_init(dcontext);

    /* sanity check, plus need to set this for statistics_init:
     * even if parent did an execve, env var should be reset by now
     */
    post_execve = (getenv(DYNAMORIO_VAR_EXECVE) != NULL);
    ASSERT(!post_execve);

#    ifdef DEBUG
    /* copy d_r_stats->logdir
     * d_r_stats->logdir is static, so current copy is fine, don't need
     * frozen copy
     */
    strncpy(parent_logdir, d_r_stats->logdir, MAXIMUM_PATH - 1);
    d_r_stats->logdir[MAXIMUM_PATH - 1] = '\0'; /* if max no null */
#    endif

    if (get_log_dir(PROCESS_DIR, NULL, NULL)) {
        /* we want brand new log dir  */
        enable_new_log_dir();
        create_log_dir(PROCESS_DIR);
    }

#    ifdef DEBUG
    /* just like dynamorio_app_init, create main_logfile before stats */
    if (d_r_stats->loglevel > 0) {
        /* we want brand new log files.  os_fork_init() closed inherited files. */
        main_logfile = open_log_file(main_logfile_name(), NULL, 0);
        print_file(main_logfile, "%s\n", dynamorio_version_string);
        print_file(main_logfile, "New log file for child %d forked by parent %d\n",
                   d_r_get_thread_id(), get_parent_id());
        print_file(main_logfile, "Parent's log dir: %s\n", parent_logdir);
    }

    d_r_stats->process_id = get_process_id();

    if (d_r_stats->loglevel > 0) {
        /* FIXME: share these few lines of code w/ dynamorio_app_init? */
        LOG(GLOBAL, LOG_TOP, 1, "Running: %s\n", d_r_stats->process_name);
#        ifndef _WIN32_WCE
        LOG(GLOBAL, LOG_TOP, 1, "DYNAMORIO_OPTIONS: %s\n", d_r_option_string);
#        endif
    }
#    endif /* DEBUG */

    vmm_heap_fork_init(dcontext);

    /* must re-hash parent entry in threads table, plus no longer have any
     * other threads (fork -> we're alone in address space), so clear
     * out entire thread table, then add child
     */
    d_r_mutex_lock(&thread_initexit_lock);
    get_list_of_threads_ex(&threads, &num_threads, true /*include execve*/);
    for (i = 0; i < num_threads; i++) {
        if (threads[i] == dcontext->thread_record)
            remove_thread(threads[i]->id);
        else
            dynamo_other_thread_exit(threads[i]);
    }
    d_r_mutex_unlock(&thread_initexit_lock);
    global_heap_free(threads,
                     num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));

    add_thread(get_process_id(), d_r_get_thread_id(), true /*under dynamo control*/,
               dcontext);

    GLOBAL_STAT(num_threads) = 1;
#    ifdef DEBUG
    if (d_r_stats->loglevel > 0) {
        /* need a new thread-local logfile */
        dcontext->logfile = open_log_file(thread_logfile_name(), NULL, 0);
        print_file(dcontext->logfile, "%s\n", dynamorio_version_string);
        print_file(dcontext->logfile, "New log file for child %d forked by parent %d\n",
                   d_r_get_thread_id(), get_parent_id());
        LOG(THREAD, LOG_TOP | LOG_THREADS, 1, "THREAD %d (dcontext " PFX ")\n\n",
            d_r_get_thread_id(), dcontext);
    }
#    endif
    num_threads = 1;

    /* FIXME: maybe should have a callback list for who wants to be notified
     * on a fork -- probably everyone who makes a log file on init.
     */
    fragment_fork_init(dcontext);
    /* this must be called after dynamo_other_thread_exit() above */
    signal_fork_init(dcontext);

    if (CLIENTS_EXIST()) {
        instrument_fork_init(dcontext);
    }
}
#endif /* UNIX */

/* To make DynamoRIO useful as a library for a standalone client
 * application (as opposed to a client library that works with
 * DynamoRIO in executing a target application).  This makes DynamoRIO
 * useful as an IA-32 disassembly library, etc.
 */
dcontext_t *
standalone_init(void)
{
    dcontext_t *dcontext;
    int count = atomic_add_exchange_int(&standalone_init_count, 1);
    if (count > 1 || dynamo_initialized)
        return GLOBAL_DCONTEXT;
    standalone_library = true;
    /* We have release-build stats now so this is not just DEBUG */
    d_r_stats = &nonshared_stats;
    /* No reason to limit heap size when there's no code cache. */
    IF_X64(dynamo_options.reachable_heap = false;)
    dynamo_options.vm_base_near_app = false;
#if defined(INTERNAL) && defined(DEADLOCK_AVOIDANCE)
    /* avoid issues w/ GLOBAL_DCONTEXT instead of thread dcontext */
    dynamo_options.deadlock_avoidance = false;
#endif
#ifdef UNIX
    os_page_size_init((const char **)our_environ, is_our_environ_followed_by_auxv());
#endif
#ifdef WINDOWS
    /* MUST do this before making any system calls */
    if (!syscalls_init())
        return NULL; /* typically b/c of unsupported OS version */
#endif
    d_r_config_init();
    options_init();
    vmm_heap_init();
    d_r_heap_init();
    dynamo_heap_initialized = true;
    dynamo_vm_areas_init();
    d_r_decode_init();
    proc_init();
    d_r_os_init();
    config_heap_init();

#ifdef STANDALONE_UNIT_TEST
    os_tls_init();
    dcontext = create_new_dynamo_context(true /*initial*/, NULL, NULL);
    set_thread_private_dcontext(dcontext);
    /* sanity check */
    ASSERT(get_thread_private_dcontext() == dcontext);

    heap_thread_init(dcontext);

#    ifdef DEBUG
    /* XXX: share code w/ main init routine? */
    nonshared_stats.logmask = LOG_ALL;
    options_init();
    if (d_r_stats->loglevel > 0) {
        char initial_options[MAX_OPTIONS_STRING];
        main_logfile = open_log_file(main_logfile_name(), NULL, 0);
        print_file(main_logfile, "%s\n", dynamorio_version_string);
        print_file(main_logfile, "Log file for standalone unit test\n");
        get_dynamo_options_string(&dynamo_options, initial_options,
                                  sizeof(initial_options), true);
        SYSLOG_INTERNAL_INFO("Initial options = %s", initial_options);
        print_file(main_logfile, "\n");
    }
#    endif /* DEBUG */
#else
    /* rather than ask the user to call some thread-init routine in
     * every thread, we just use global dcontext everywhere (i#548)
     */
    dcontext = GLOBAL_DCONTEXT;
#endif

    /* In case standalone_exit() is omitted or there's a crash, we clean up any .1config
     * file right now.  the only loss if that we can't synch options: but that
     * should be less important for standalone.  We disabling synching.
     */
    /* options are never made read-only for standalone */
    dynamo_options.dynamic_options = false;

    dynamo_initialized = true;

    return dcontext;
}

void
standalone_exit(void)
{
    int count = atomic_add_exchange_int(&standalone_init_count, -1);
    if (count != 0)
        return;
    /* We support re-attach by setting doing_detach. */
    doing_detach = true;
#ifdef STANDALONE_UNIT_TEST
    dcontext_t *dcontext = get_thread_private_dcontext();
    set_thread_private_dcontext(NULL);
    heap_thread_exit(dcontext);
    delete_dynamo_context(dcontext, true);
    /* We can't call os_tls_exit() b/c we don't have safe_read support for
     * the TLS magic read on Linux.
     */
#endif
    config_heap_exit();
    os_fast_exit();
    os_slow_exit();
#if !defined(STANDALONE_UNIT_TEST) || !defined(AARCH64)
    /* XXX: The lock setup is somehow messed up on AArch64.  Disabling cleanup. */
    dynamo_vm_areas_exit();
#endif
#ifndef STANDALONE_UNIT_TEST
    /* We have a leak b/c we can't call os_tls_exit().  For now we simplify
     * and leave it alone.
     */
    d_r_heap_exit();
    vmm_heap_exit();
#endif
    options_exit();
    d_r_config_exit();
    doing_detach = false;
    standalone_library = false;
    dynamo_initialized = false;
    dynamo_options_initialized = false;
    dynamo_heap_initialized = false;
    options_detach();
}

/* Perform exit tasks that require full thread data structs, which we have
 * already cleaned up by the time we reach dynamo_shared_exit() for both
 * debug and detach paths.
 */
void
dynamo_process_exit_with_thread_info(void)
{
    perscache_fast_exit(); /* "fast" b/c called in release as well */
}

/* shared between app_exit and detach */
int
dynamo_shared_exit(thread_record_t *toexit /* must ==cur thread for Linux */
                       _IF_WINDOWS(bool detach_stacked_callbacks))
{
    DEBUG_DECLARE(uint endtime);
    /* set this now, could already be set */
    dynamo_exited = true;

    /* avoid time() for libc independence */
    DODEBUG(endtime = query_time_seconds(););
    LOG(GLOBAL, LOG_STATS, 1, "\n#### Statistics for entire process:\n");
    LOG(GLOBAL, LOG_STATS, 1, "Total running time: %d seconds\n", endtime - starttime);

#ifdef PAPI
    hardware_perfctr_exit();
#endif
#ifdef DEBUG
#    if defined(INTERNAL) && defined(X86)
    print_optimization_stats();
#    endif /* INTERNAL && X86 */
    DOLOG(1, LOG_STATS, { dump_global_stats(false); });
#endif /* DEBUG */

    if (SELF_PROTECT_ON_CXT_SWITCH) {
        DELETE_LOCK(protect_info->lock);
        global_unprotected_heap_free(protect_info,
                                     sizeof(protect_info_t) HEAPACCT(ACCT_OTHER));
    }

    /* call all component exit routines (CAUTION: order is important here) */

    DELETE_RECURSIVE_LOCK(thread_in_DR_exclusion);
    DOSTATS({
        LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1,
            "fcache_stats_exit: before fragment cleanup\n");
        DOLOG(1, LOG_CACHE, fcache_stats_exit(););
    });
#ifdef RCT_IND_BRANCH
    if (!DYNAMO_OPTION(thin_client))
        rct_exit();
#endif
    fragment_exit();
#ifdef ANNOTATIONS
    annotation_exit();
#endif
    jitopt_exit();
    /* We tell the client as soon as possible in case it wants to use services from other
     * components.  Must be after fragment_exit() so that the client gets all the
     * fragment_deleted() callbacks (xref PR 228156). FIXME - might be issues with the
     * client trying to use api routines that depend on fragment state.
     */
    instrument_exit_event();
    /* We only need do a second synch-all if there are sideline client threads. */
    if (d_r_get_num_threads() > 1)
        synch_with_threads_at_exit(exit_synch_state(), false /*post-exit*/);
    /* only current thread is alive */
    dynamo_exited_all_other_threads = true;
    fragment_exit_post_sideline();

    /* The dynamo_exited_and_cleaned should be set after the second synch-all.
     * If it is set earlier after the first synch-all, some client thread may
     * have memory leak due to dynamo_thread_exit_pre_client being skipped in
     * dynamo_thread_exit_common called from exiting client threads.
     */
    dynamo_exited_and_cleaned = true;

    destroy_event(dr_app_started);
    destroy_event(dr_attach_finished);

    /* Make thread and process exit calls before we clean up thread data. */
    loader_make_exit_calls(get_thread_private_dcontext());
    /* we want dcontext around for loader_exit() */
    if (get_thread_private_dcontext() != NULL)
        loader_thread_exit(get_thread_private_dcontext());
    /* This will unload client libs, which we delay until after they receive their
     * thread exit calls in loader_thread_exit().
     */
    instrument_exit();
    loader_exit();

    if (toexit != NULL) {
        /* Free detaching thread's dcontext.
         * Restoring the teb fields or segment registers can only be done
         * on the current thread, which must be toexit.
         */
#ifdef WINDOWS
        /* XXX i#5340: We used to go through dynamo_other_thread_exit() which rewinds
         * the kstats stack as below.  To avoid a kstats assert on this new path we
         * repeat it here but it seems like we shouldn't need it.
         */
        KSTOP_REWIND_DC(get_thread_private_dcontext(), thread_measured);
        KSTART_DC(get_thread_private_dcontext(), thread_measured);
#endif
        ASSERT(toexit->id == d_r_get_thread_id());
        dynamo_thread_exit();
    }

    if (IF_WINDOWS_ELSE(!detach_stacked_callbacks, true)) {
        /* We don't fully free cur thread until after client exit event (PR 536058) */
        if (thread_lookup(d_r_get_thread_id()) == NULL) {
            LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1,
                "Current thread never under DynamoRIO control, not exiting it\n");
        } else {
            /* call thread_exit even if !under_dynamo_control, could have
             * been at one time
             */
            /* exit this thread now */
            dynamo_thread_exit();
        }
    }
    /* now that the final thread is exited, free the all_threads memory */
    d_r_mutex_lock(&all_threads_lock);
    global_heap_free(all_threads,
                     HASHTABLE_SIZE(ALL_THREADS_HASH_BITS) *
                         sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    all_threads = NULL;
    d_r_mutex_unlock(&all_threads_lock);

#ifdef WINDOWS
    /* for -private_loader we do this here to catch more exit-time crashes */
    if (!INTERNAL_OPTION(noasynch) && INTERNAL_OPTION(private_loader) && !doing_detach)
        callback_interception_unintercept();
    /* callback_interception_exit must be after fragment exit for clients so
     * that fragment_exit->frees fragments->instrument_fragment_deleted->
     * hide_tag_from_fragment->is_intercepted_app_pc won't crash. Xref PR 228156.
     */
    if (!INTERNAL_OPTION(noasynch)) {
        callback_interception_exit();
    }
#endif
    d_r_link_exit();
    fcache_exit();
    d_r_monitor_exit();
    synch_exit();
    d_r_arch_exit(IF_WINDOWS(detach_stacked_callbacks));
#ifdef CALL_PROFILE
    /* above os_exit to avoid eventlog_mutex trigger if we're the first to
     * create a log file
     */
    profile_callers_exit();
#endif
    os_fast_exit();
    os_slow_exit();
    native_exec_exit(); /* before vm_areas_exit for using dynamo_areas */
    vm_areas_exit();
    perscache_slow_exit(); /* fast called in dynamo_process_exit_with_thread_info() */
    modules_exit();        /* after aslr_exit() from os_slow_exit(),
                            * after vm_areas & perscache exits */
    moduledb_exit();       /* before heap_exit */
#ifdef HOT_PATCHING_INTERFACE
    if (DYNAMO_OPTION(hot_patching))
        hotp_exit();
#endif
#ifdef WINDOWS
    /* Free exception stack before calling heap_exit */
    stack_free(exception_stack, EXCEPTION_STACK_SIZE);
    exception_stack = NULL;
#endif
    config_heap_exit();
    d_r_heap_exit();
    vmm_heap_exit();
    diagnost_exit();
    data_section_exit();
    /* Funny dependences: options exit just frees lock, not destroying
     * any options that are needed for other exits, so do it prior to
     * checking locks in debug build.  We have a separate options_detach()
     * which resets options for re-attach.
     */
    options_exit();
    utils_exit();
    d_r_config_exit();

#ifdef KSTATS
    kstat_exit();
#endif

    DELETE_LOCK(all_threads_lock);
    DELETE_LOCK(thread_initexit_lock);

    DOLOG(1, LOG_STATS, {
        /* dump after cleaning up to make it easy to check if stats that
         * are inc-ed and dec-ed actually come down to 0
         */
        dump_global_stats(false);
    });
    if (INTERNAL_OPTION(rstats_to_stderr))
        dump_global_rstats_to_stderr();

    statistics_exit();
#ifdef DEBUG
#    ifdef DEADLOCK_AVOIDANCE
    ASSERT(locks_not_closed() == 0);
#    endif
    dynamo_exited_log_and_stats = true;
    if (main_logfile != STDERR) {
        /* do it this way just in case someone tries to log to the global file
         * right now */
        file_t file_temp = main_logfile;
        main_logfile = INVALID_FILE;
        close_log_file(file_temp);
    }
#else
#    ifdef DEADLOCK_AVOIDANCE
    ASSERT(locks_not_closed() == 0);
#    endif
#endif /* DEBUG */

    dynamo_initialized = false;
    dynamo_started = false;
    return SUCCESS;
}

/* NOINLINE because dynamorio_app_exit is a stopping point. */
NOINLINE int
dynamorio_app_exit(void)
{
    return dynamo_process_exit();
}

/* synchs with all threads using synch type synch_res.
 * also sets dynamo_exited to true.
 * does not resume the threads but does release the thread_initexit_lock.
 */
static void
synch_with_threads_at_exit(thread_synch_state_t synch_res, bool pre_exit)
{
    int num_threads;
    thread_record_t **threads;
    DEBUG_DECLARE(bool ok;)
    /* If we fail to suspend a thread (e.g., privilege
     * problems) ignore it. XXX: retry instead?
     */
    uint flags = THREAD_SYNCH_SUSPEND_FAILURE_IGNORE;
    if (pre_exit) {
        /* i#297: we only synch client threads after process exit event. */
        flags |= THREAD_SYNCH_SKIP_CLIENT_THREAD;
    }
    LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1,
        "\nsynch_with_threads_at_exit: cleaning up %d un-terminated threads\n",
        d_r_get_num_threads());

#ifdef WINDOWS
    /* make sure client nudges are finished */
    wait_for_outstanding_nudges();
#endif

    /* xref case 8747, requesting suspended is preferable to terminated and it
     * doesn't make a difference here which we use (since the process is about
     * to die).
     * On Linux, however, we do not have dependencies on OS thread
     * properties like we do on Windows (TEB, etc.), and our suspended
     * threads use their sigstacks and ostd data structs, making cleanup
     * while still catching other leaks more difficult: thus it's
     * simpler to terminate and then clean up.  FIXME: by terminating
     * we'll raise SIGCHLD that may not have been raised natively if the
     * whole group went down in a single SYS_exit_group.  Instead we
     * could have the suspended thread move from the sigstack-reliant
     * loop to a stack-free loop (xref i#95).
     */
    IF_UNIX(dynamo_exiting = true;) /* include execve-exited vfork threads */
    DEBUG_DECLARE(ok =)
    synch_with_all_threads(synch_res, &threads, &num_threads,
                           /* Case 6821: other synch-all-thread uses that
                            * only care about threads carrying fcache
                            * state can ignore us
                            */
                           THREAD_SYNCH_NO_LOCKS_NO_XFER, flags);
    ASSERT(ok);
    ASSERT(threads == NULL && num_threads == 0); /* We asked for CLEANED */
    /* the synch_with_all_threads function grabbed the
     * thread_initexit_lock for us! */
    /* do this now after all threads we know about are killed and
     * while we hold the thread_initexit_lock so any new threads that
     * are waiting on it won't get in our way (see thread_init()) */
    dynamo_exited = true;
    end_synch_with_all_threads(threads, num_threads, false /*don't resume*/);
}

static thread_synch_state_t
exit_synch_state(void)
{
    thread_synch_state_t synch_res = IF_WINDOWS_ELSE(THREAD_SYNCH_SUSPENDED_AND_CLEANED,
                                                     THREAD_SYNCH_TERMINATED_AND_CLEANED);
#if defined(DR_APP_EXPORTS) && defined(UNIX)
    if (dr_api_exit) {
        /* Don't terminate the app's threads in case the app plans to continue
         * after dr_app_cleanup().  Note that today we don't fully support that
         * anyway: the app should use dr_app_stop_and_cleanup() whose detach
         * code won't come here.
         */
        synch_res = THREAD_SYNCH_SUSPENDED_AND_CLEANED;
    }
#endif
    return synch_res;
}

#ifdef DEBUG
/* cleanup after the application has exited */
static int
dynamo_process_exit_cleanup(void)
{
    /* CAUTION: this should only be invoked after all app threads have stopped */
    if (!dynamo_exited && !INTERNAL_OPTION(nullcalls)) {
        APP_EXPORT_ASSERT(dynamo_initialized, "Improper DynamoRIO initialization");

        /* we deliberately do NOT clean up d_r_initstack (which was
         * allocated using a separate mmap and so is not part of some
         * large unit that is de-allocated), as it is used in special
         * circumstances to call us...FIXME: is this memory leak ok?
         * is there a better solution besides assuming the app stack?
         */

#    ifdef SIDELINE
        if (dynamo_options.sideline) {
            /* exit now to make thread cleanup simpler */
            sideline_exit();
        }
#    endif

        /* perform exit tasks that require full thread data structs */
        dynamo_process_exit_with_thread_info();

        if (INTERNAL_OPTION(single_privileged_thread)) {
            d_r_mutex_unlock(&thread_initexit_lock);
        }

        /* if ExitProcess called before all threads terminated, they won't
         * all have gone through dynamo_thread_exit, so clean them up now
         * so we can get stats about them
         *
         * we don't check control_all_threads b/c we're just killing
         * the threads we know about here
         */
        synch_with_threads_at_exit(exit_synch_state(), true /*pre-exit*/);
        /* now that APC interception point is unpatched and
         * dynamorio_exited is set and we've killed all the theads we know
         * about, assumption is that no other threads will be running in
         * dynamorio code from here on out (esp. when we get into shared exit)
         * that will do anything that could be dangerous (could possibly be
         * a thread in the APC interception code prior to reaching thread_init
         * but it will only global log and do thread_lookup which should be
         * safe throughout) */

        /* In order to pass the client a dcontext in the process exit event
         * we do some thread cleanup early for the final thread so we can delay
         * the rest (PR 536058).  This is a little risky in that we
         * clean up dcontext->fragment_field, which is used for lots of
         * things like couldbelinking (and thus we have to disable some API
         * routines in the thread exit event: i#1989).
         */
        dynamo_thread_exit_pre_client(get_thread_private_dcontext(), d_r_get_thread_id());

#    ifdef WINDOWS
        /* FIXME : our call un-interception isn't atomic so (miniscule) chance
         * of something going wrong if new thread is just hitting its init APC
         */
        /* w/ the app's loader we must remove our LdrUnloadDll hook
         * before we unload the client lib (and thus we miss client
         * exit crashes): xref PR 200207.
         */
        if (!INTERNAL_OPTION(noasynch) && !INTERNAL_OPTION(private_loader)) {
            callback_interception_unintercept();
        }
#    else  /* UNIX */
        unhook_vsyscall();
#    endif /* UNIX */

        return dynamo_shared_exit(NULL /* not detaching */
                                      _IF_WINDOWS(false /* not detaching */));
    }
    return SUCCESS;
}
#endif /* DEBUG */

int
dynamo_nullcalls_exit(void)
{
    /* this routine is used when nullcalls is turned on
     * simply to get perfctr numbers in a log file
     */
    ASSERT(INTERNAL_OPTION(nullcalls));
#ifdef PAPI
    hardware_perfctr_exit();
#endif

#ifdef DEBUG
    if (main_logfile != STDERR) {
        close_log_file(main_logfile);
        main_logfile = INVALID_FILE;
    }
#endif /* DEBUG */

    dynamo_exited = true;
    return SUCCESS;
}

/* called when we see that the process is about to exit */
int
dynamo_process_exit(void)
{
#ifndef DEBUG
    bool each_thread;
#endif
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    synchronize_dynamic_options();
    SYSLOG(SYSLOG_INFORMATION, INFO_PROCESS_STOP, 2, get_application_name(),
           get_application_pid());
#ifdef DEBUG
    if (!dynamo_exited) {
        if (INTERNAL_OPTION(nullcalls)) {
            /* if nullcalls is on we still do perfctr stats, and this is
             * the only place we can print them out and exit
             */
            dynamo_nullcalls_exit();
        } else {
            /* we don't check automatic_startup -- even if the app_
             * interface is used, we are about to be gone from the process
             * address space, so we clean up now
             */
            LOG(GLOBAL, LOG_TOP, 1,
                "\ndynamo_process_exit from thread " TIDFMT " -- cleaning up dynamo\n",
                d_r_get_thread_id());
            dynamo_process_exit_cleanup();
        }
    }

    return SUCCESS;

#else
    if (dynamo_exited)
        return SUCCESS;

    /* don't need to do much!
     * we didn't create any IPC objects or anything that might be persistent
     * beyond our death, we're not holding any systemwide locks, etc.
     */

    /* It is not clear whether the Event Log service handles unterminated connections */

    /* Do we need profile data for each thread?
     * Note that windows prof_pcs duplicates the thread walk in d_r_os_exit()
     * FIXME: should combine that thread walk with this one
     */
    each_thread = TRACEDUMP_ENABLED();
#    ifdef UNIX
    each_thread = each_thread || INTERNAL_OPTION(profile_pcs);
#    endif
#    ifdef KSTATS
    each_thread = each_thread || DYNAMO_OPTION(kstats);
#    endif
    each_thread = each_thread ||
        /* If we don't need a thread exit event, avoid the possibility of
         * racy crashes (PR 470957) by not calling instrument_thread_exit()
         */
        (!INTERNAL_OPTION(nullcalls) && dr_thread_exit_hook_exists() &&
         !DYNAMO_OPTION(skip_thread_exit_at_exit));

    if (DYNAMO_OPTION(synch_at_exit)
        /* by default we synch if any exit event exists */
        || (!DYNAMO_OPTION(multi_thread_exit) && dr_exit_hook_exists()) ||
        (!DYNAMO_OPTION(skip_thread_exit_at_exit) && dr_thread_exit_hook_exists())) {
        /* Needed primarily for clients but technically all configurations
         * can have racy crashes at exit time (xref PR 470957).
         */
        synch_with_threads_at_exit(exit_synch_state(), true /*pre-exit*/);
    } else
        dynamo_exited = true;

    if (each_thread) {
        thread_record_t **threads;
        int num, i;
        d_r_mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num);

        for (i = 0; i < num; i++) {
            if (IS_CLIENT_THREAD(threads[i]->dcontext))
                continue;
            /* FIXME: separate trace dump from rest of fragment cleanup code */
            if (TRACEDUMP_ENABLED() || true) {
                /* We always want to call this for CI builds so we can get the
                 * dr_fragment_deleted() callbacks.
                 */
                fragment_thread_exit(threads[i]->dcontext);
            }
#    ifdef UNIX
            if (INTERNAL_OPTION(profile_pcs))
                pcprofile_thread_exit(threads[i]->dcontext);
#    endif
#    ifdef KSTATS
            if (DYNAMO_OPTION(kstats))
                kstat_thread_exit(threads[i]->dcontext);
#    endif
            /* Inform client of all thread exits */
            if (!INTERNAL_OPTION(nullcalls) && !DYNAMO_OPTION(skip_thread_exit_at_exit)) {
                instrument_thread_exit_event(threads[i]->dcontext);
                /* i#1617: ensure we do all cleanup of priv libs */
                if (threads[i]->id != d_r_get_thread_id()) /* i#1617: must delay this */
                    loader_thread_exit(threads[i]->dcontext);
            }
        }
        global_heap_free(threads,
                         num * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
        d_r_mutex_unlock(&thread_initexit_lock);
    }

    /* PR 522783: must be before we clear dcontext (for clients)! */
    /* must also be prior to fragment_exit so we actually freeze pcaches (i#703) */
    dynamo_process_exit_with_thread_info();

    /* FIXME: separate trace dump from rest of fragment cleanup code.  For client
     * interface we need to call fragment_exit to get all the fragment deleted events. */
    if (TRACEDUMP_ENABLED() || dr_fragment_deleted_hook_exists())
        fragment_exit();

    /* Inform client of process exit */
    if (!INTERNAL_OPTION(nullcalls)) {
#    ifdef WINDOWS
        /* instrument_exit() unloads the client library, so make sure
         * LdrUnloadDll isn't hooked if using the app loader.
         */
        if (!INTERNAL_OPTION(noasynch) && !INTERNAL_OPTION(private_loader)) {
            callback_interception_unintercept();
        }
#    endif
#    ifdef UNIX
        /* i#2976: unhook prior to client exit if modules are being watched */
        if (dr_modload_hook_exists())
            unhook_vsyscall();
#    endif
        /* Must be after fragment_exit() so that the client gets all the
         * fragment_deleted() callbacks (xref PR 228156).  FIXME - might be issues
         * with the client trying to use api routines that depend on fragment state.
         */
        instrument_exit_event();

        /* We only need do a second synch-all if there are sideline client threads. */
        if (d_r_get_num_threads() > 1)
            synch_with_threads_at_exit(exit_synch_state(), false /*post-exit*/);
        dynamo_exited_all_other_threads = true;

        /* i#1617: We need to call client library fini routines for global
         * destructors, etc.
         */
        if (!INTERNAL_OPTION(nullcalls) && !DYNAMO_OPTION(skip_thread_exit_at_exit))
            loader_thread_exit(get_thread_private_dcontext());
        /* This will unload client libs, which we delay until after they receive their
         * thread exit calls in loader_thread_exit().
         */
        instrument_exit();
        loader_exit();

        /* for -private_loader we do this here to catch more exit-time crashes */
#    ifdef WINDOWS
        if (!INTERNAL_OPTION(noasynch) && INTERNAL_OPTION(private_loader))
            callback_interception_unintercept();
#    endif
    }
    fragment_exit_post_sideline();

#    ifdef CALL_PROFILE
    profile_callers_exit();
#    endif
#    ifdef KSTATS
    if (DYNAMO_OPTION(kstats))
        kstat_exit();
#    endif
    /* so make sure eventlog connection is terminated (if present)  */
    os_fast_exit();

    if (INTERNAL_OPTION(rstats_to_stderr))
        dump_global_rstats_to_stderr();

    return SUCCESS;
#endif /* !DEBUG */
}

void
dynamo_exit_post_detach(void)
{
    /* i#2157: best-effort re-init in case of re-attach */

    do_once_generation++; /* Increment the generation in case we re-attach */

    dynamo_initialized = false;
    dynamo_options_initialized = false;
    dynamo_heap_initialized = false;
    automatic_startup = false;
    control_all_threads = false;
    dr_api_entry = false;
    dr_api_exit = false;
#ifdef UNIX
    dynamo_exiting = false;
#endif
    dynamo_exited = false;
    dynamo_exited_all_other_threads = false;
    dynamo_exited_and_cleaned = false;
#ifdef DEBUG
    dynamo_exited_log_and_stats = false;
#endif
    dynamo_resetting = false;
#ifdef UNIX
    post_execve = false;
#endif
    vm_areas_post_exit();
    heap_post_exit();
}

dcontext_t *
create_new_dynamo_context(bool initial, byte *dstack_in, priv_mcontext_t *mc)
{
    dcontext_t *dcontext;
    size_t alloc = sizeof(dcontext_t) + proc_get_cache_line_size();
    void *alloc_start =
        (void *)((TEST(SELFPROT_GLOBAL, dynamo_options.protect_mask) &&
                  !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
                     ?
                     /* if protecting global but not dcontext, put whole thing in unprot
                        mem */
                     global_unprotected_heap_alloc(alloc HEAPACCT(ACCT_OTHER))
                     : global_heap_alloc(alloc HEAPACCT(ACCT_OTHER)));
    dcontext = (dcontext_t *)proc_bump_to_end_of_cache_line((ptr_uint_t)alloc_start);
    ASSERT(proc_is_cache_aligned(dcontext));
#ifdef X86
    /* 264138: ensure xmm/ymm slots are aligned so we can use vmovdqa */
    ASSERT(ALIGNED(get_mcontext(dcontext)->simd, ZMM_REG_SIZE));
    /* also ensure we don't have extra padding beyond x86.asm defines */
    ASSERT(sizeof(priv_mcontext_t) ==
           IF_X64_ELSE(18, 10) * sizeof(reg_t) + PRE_XMM_PADDING +
               MCXT_TOTAL_SIMD_SLOTS_SIZE + MCXT_TOTAL_OPMASK_SLOTS_SIZE);
#elif defined(ARM)
    /* FIXME i#1551: add arm alignment check if any */
#endif /* X86/ARM */

    /* Put here all one-time dcontext field initialization
     * Make sure to update create_callback_dcontext to shared
     * fields across callback dcontexts for the same thread.
     */
    /* must set to 0 so can tell if initialized for callbacks! */
    memset(dcontext, 0x0, sizeof(dcontext_t));
    dcontext->allocated_start = alloc_start;

    /* we share a single dstack across all callbacks */
    if (initial) {
        /* DrMi#1723: our dstack needs to be at a higher address than the app
         * stack.  If mc passed, use its xsp; else use cur xsp (initial thread
         * is on the app stack here: xref i#1105), for lower bound for dstack.
         */
        byte *app_xsp;
        if (mc == NULL)
            GET_STACK_PTR(app_xsp);
        else
            app_xsp = (byte *)mc->xsp;
        if (dstack_in == NULL) {
            dcontext->dstack = (byte *)stack_alloc(DYNAMORIO_STACK_SIZE, app_xsp);
        } else
            dcontext->dstack = dstack_in; /* xref i#149/PR 403015 */
#ifdef WINDOWS
        DOCHECK(1, {
            if (dcontext->dstack < app_xsp)
                SYSLOG_INTERNAL_WARNING_ONCE("dstack is below app xsp");
        });
#endif
    } else {
        /* dstack may be pre-allocated only at thread init, not at callback */
        ASSERT(dstack_in == NULL);
    }
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        dcontext->upcontext.separate_upcontext = global_unprotected_heap_alloc(
            sizeof(unprotected_context_t) HEAPACCT(ACCT_OTHER));
        /* don't need to initialize upcontext */
        LOG(GLOBAL, LOG_TOP, 2, "new dcontext=" PFX ", dcontext->upcontext=" PFX "\n",
            dcontext, dcontext->upcontext.separate_upcontext);
        dcontext->upcontext_ptr = dcontext->upcontext.separate_upcontext;
    } else
        dcontext->upcontext_ptr = &(dcontext->upcontext.upcontext);
#ifdef HOT_PATCHING_INTERFACE
    /* Set the hot patch exception state to be empty/unused. */
    DODEBUG(memset(&dcontext->hotp_excpt_state, -1, sizeof(dr_jmp_buf_t)););
#endif
    ASSERT(dcontext->try_except.try_except_state == NULL);

    DODEBUG({ dcontext->logfile = INVALID_FILE; });
    dcontext->owning_thread = d_r_get_thread_id();
#ifdef UNIX
    dcontext->owning_process = get_process_id();
#endif
    /* thread_record is set in add_thread */
    /* all of the thread-private fcache and hashtable fields are shared
     * among all dcontext instances of a thread, so the caller must
     * set those fields
     */
    /* rest of dcontext initialization happens in initialize_dynamo_context(),
     * which is executed for each dr_app_start() and each
     * callback start
     */
    return dcontext;
}

static void
delete_dynamo_context(dcontext_t *dcontext, bool free_stack)
{
    if (free_stack) {
        ASSERT(dcontext->dstack != NULL);
        ASSERT(!is_currently_on_dstack(dcontext));
        LOG(GLOBAL, LOG_THREADS, 1, "Freeing DR stack " PFX "\n", dcontext->dstack);
        stack_free(dcontext->dstack, DYNAMORIO_STACK_SIZE);
    } /* else will be cleaned up by caller */

    ASSERT(dcontext->try_except.try_except_state == NULL);

    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        global_unprotected_heap_free(dcontext->upcontext.separate_upcontext,
                                     sizeof(unprotected_context_t) HEAPACCT(ACCT_OTHER));
    }
    if (TEST(SELFPROT_GLOBAL, dynamo_options.protect_mask) &&
        !TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask)) {
        /* if protecting global but not dcontext, we put whole thing in unprot mem */
        global_unprotected_heap_free(dcontext->allocated_start,
                                     sizeof(dcontext_t) +
                                         proc_get_cache_line_size() HEAPACCT(ACCT_OTHER));
    } else {
        global_heap_free(dcontext->allocated_start,
                         sizeof(dcontext_t) +
                             proc_get_cache_line_size() HEAPACCT(ACCT_OTHER));
    }
}

/* This routine is called not only at thread initialization,
 * but for every callback, etc. that gets a fresh execution
 * environment!
 */
void
initialize_dynamo_context(dcontext_t *dcontext)
{
    /* we can't just zero out the whole thing b/c we have persistent state
     * (fields kept across callbacks, like dstack, module-private fields, next &
     * prev, etc.)
     */
    memset(dcontext->upcontext_ptr, 0, sizeof(unprotected_context_t));
    dcontext->initialized = true;
    dcontext->whereami = DR_WHERE_APP;
    dcontext->next_tag = NULL;
    dcontext->native_exec_postsyscall = NULL;
    memset(dcontext->native_retstack, 0, sizeof(dcontext->native_retstack));
    dcontext->native_retstack_cur = 0;
    dcontext->isa_mode = DEFAULT_ISA_MODE;
#ifdef ARM
    dcontext->encode_state[0] = 0;
    dcontext->encode_state[1] = 0;
    dcontext->decode_state[0] = 0;
    dcontext->decode_state[1] = 0;
#endif
    dcontext->sys_num = 0;
#ifdef WINDOWS
    dcontext->app_errno = 0;
#    ifdef DEBUG
    dcontext->is_client_thread_exiting = false;
#    endif
    dcontext->sys_param_base = NULL;
    /* always initialize aslr_context */
    dcontext->aslr_context.sys_aslr_clobbered = 0;
    dcontext->aslr_context.randomized_section_handle = INVALID_HANDLE_VALUE;
    dcontext->aslr_context.original_image_section_handle = INVALID_HANDLE_VALUE;
    dcontext->aslr_context.original_section_base = ASLR_INVALID_SECTION_BASE;
#    ifdef DEBUG
    dcontext->aslr_context.last_app_section_handle = INVALID_HANDLE_VALUE;
#    endif
    /* note that aslr_context.last_child_padded is preserved across callbacks */
    dcontext->ignore_enterexit = false;
#else
    dcontext->sys_param0 = 0;
    dcontext->sys_param1 = 0;
    dcontext->sys_param2 = 0;
#endif

#ifdef UNIX
    dcontext->signals_pending = 0;
#endif

    /* all thread-private fields are initialized in dynamo_thread_init
     * or in create_callback_dcontext because they must be initialized differently
     * in those two cases
     */

    set_last_exit(dcontext, (linkstub_t *)get_starting_linkstub());

#ifdef PROFILE_RDTSC
    dcontext->start_time = (uint64)0;
    dcontext->prev_fragment = NULL;
    dcontext->cache_frag_count = (uint64)0;
    {
        int i;
        for (i = 0; i < 10; i++) {
            dcontext->cache_time[i] = (uint64)0;
            dcontext->cache_count[i] = (uint64)0;
        }
    }
#endif
#ifdef DEBUG
    dcontext->in_opnd_disassemble = false;
#endif
#ifdef WINDOWS
    /* Other pieces of DR -- callback & APC handling, detach -- test
     * asynch_target to determine where the next app pc to execute is
     * stored. Init it to 0 to indicate that this context's most recent
     * syscall was not executed from handle_system_call().
     */
    dcontext->asynch_target = NULL;
    /* next_saved and prev_unused are zeroed out when dcontext is
     * created; we shouldn't zero them here, they may have valid data
     */
    dcontext->valid = true;
#endif
#ifdef HOT_PATCHING_INTERFACE
    dcontext->nudge_thread = false; /* Fix for case 5367. */
#endif
#ifdef CHECK_RETURNS_SSE2
    /* initialize sse2 index with 0
     * go ahead and use eax, it's dead (about to return)
     */
#    ifdef UNIX
    asm("movl $0, %eax");
    asm("pinsrw $7,%eax,%xmm7");
#    else
#        error NYI
#    endif
#endif
    /* We don't need to initialize dcontext->coarse_exit as it is only
     * read when last_exit indicates a coarse exit, which sets the fields.
     */
    dcontext->go_native = false;
}

#ifdef WINDOWS
/* on windows we use a new dcontext for each callback context */
dcontext_t *
create_callback_dcontext(dcontext_t *old_dcontext)
{
    dcontext_t *new_dcontext = create_new_dynamo_context(false, NULL, NULL);
    new_dcontext->valid = false;
    /* all of these fields are shared among all dcontexts of a thread: */
    new_dcontext->owning_thread = old_dcontext->owning_thread;
#    ifdef UNIX
    new_dcontext->owning_process = old_dcontext->owning_process;
#    endif
    new_dcontext->thread_record = old_dcontext->thread_record;
    /* now that we have clean stack usage we can share a single stack */
    ASSERT(old_dcontext->dstack != NULL);
    new_dcontext->dstack = old_dcontext->dstack;
    new_dcontext->isa_mode = old_dcontext->isa_mode;
    new_dcontext->link_field = old_dcontext->link_field;
    new_dcontext->monitor_field = old_dcontext->monitor_field;
    new_dcontext->fcache_field = old_dcontext->fcache_field;
    new_dcontext->fragment_field = old_dcontext->fragment_field;
    new_dcontext->heap_field = old_dcontext->heap_field;
    new_dcontext->vm_areas_field = old_dcontext->vm_areas_field;
    new_dcontext->os_field = old_dcontext->os_field;
    new_dcontext->synch_field = old_dcontext->synch_field;
    /* case 8958: copy win32_start_addr in case we produce a forensics file
     * from within a callback.
     */
    new_dcontext->win32_start_addr = old_dcontext->win32_start_addr;
    /* FlsData is persistent across callbacks */
    new_dcontext->app_fls_data = old_dcontext->app_fls_data;
    new_dcontext->priv_fls_data = old_dcontext->priv_fls_data;
    new_dcontext->app_nt_rpc = old_dcontext->app_nt_rpc;
    new_dcontext->priv_nt_rpc = old_dcontext->priv_nt_rpc;
    new_dcontext->app_nls_cache = old_dcontext->app_nls_cache;
    new_dcontext->priv_nls_cache = old_dcontext->priv_nls_cache;
    new_dcontext->app_static_tls = old_dcontext->app_static_tls;
    new_dcontext->priv_static_tls = old_dcontext->priv_static_tls;
    new_dcontext->app_stack_limit = old_dcontext->app_stack_limit;
    new_dcontext->app_stack_base = old_dcontext->app_stack_base;
    new_dcontext->teb_base = old_dcontext->teb_base;
#    ifdef UNIX
    new_dcontext->signal_field = old_dcontext->signal_field;
    new_dcontext->pcprofile_field = old_dcontext->pcprofile_field;
#    endif
    new_dcontext->private_code = old_dcontext->private_code;
    new_dcontext->client_data = old_dcontext->client_data;
#    ifdef DEBUG
    new_dcontext->logfile = old_dcontext->logfile;
    new_dcontext->thread_stats = old_dcontext->thread_stats;
#    endif
#    ifdef DEADLOCK_AVOIDANCE
    new_dcontext->thread_owned_locks = old_dcontext->thread_owned_locks;
#    endif
#    ifdef KSTATS
    new_dcontext->thread_kstats = old_dcontext->thread_kstats;
#    endif
    /* at_syscall is real time based, not app context based, so shared
     *
     * FIXME: Yes need to share when swapping at NtCallbackReturn, but
     * want to keep old so when return from cb will do post-syscall for
     * syscall that triggered cb in the first place!
     * Plus, new cb calls initialize_dynamo_context(), which clears this field
     * anyway!  This all works now b/c we don't have alertable syscalls
     * that we do post-syscall processing on.
     */
    new_dcontext->upcontext_ptr->at_syscall = old_dcontext->upcontext_ptr->at_syscall;
#    ifdef HOT_PATCHING_INTERFACE /* Fix for case 5367. */
    /* hotp_excpt_state should be unused at this point.  If it is used, it can
     * be only because a hot patch made a system call with a callback.  This is
     * a bug because hot patches can't do system calls, let alone one with
     * callbacks.
     */
    DOCHECK(1, {
        dr_jmp_buf_t empty;
        memset(&empty, -1, sizeof(dr_jmp_buf_t));
        ASSERT(memcmp(&old_dcontext->hotp_excpt_state, &empty, sizeof(dr_jmp_buf_t)) ==
               0);
    });
    new_dcontext->nudge_thread = old_dcontext->nudge_thread;
#    endif
    /* our exceptions should be handled within one DR context switch */
    ASSERT(old_dcontext->try_except.try_except_state == NULL);
    new_dcontext->local_state = old_dcontext->local_state;
#    ifdef WINDOWS
    new_dcontext->aslr_context.last_child_padded =
        old_dcontext->aslr_context.last_child_padded;
#    endif

    LOG(new_dcontext->logfile, LOG_TOP, 2, "made new dcontext " PFX " (old=" PFX ")\n",
        new_dcontext, old_dcontext);
    return new_dcontext;
}
#endif

bool
is_thread_initialized(void)
{
#if defined(UNIX) && defined(HAVE_TLS)
    /* We don't want to pay the d_r_get_thread_id() cost on every
     * get_thread_private_dcontext() when we only really need the
     * check for this call here, so we explicitly check.
     */
    if (get_tls_thread_id() != get_sys_thread_id())
        return false;
#endif
    return (get_thread_private_dcontext() != NULL);
}

bool
is_thread_known(thread_id_t tid)
{
    return (thread_lookup(tid) != NULL);
}

#ifdef UNIX
/* i#237/PR 498284: a thread about to execute SYS_execve should be considered
 * exited, but we can't easily clean up it for real immediately
 */
void
mark_thread_execve(thread_record_t *tr, bool execve)
{
    ASSERT((execve && !tr->execve) || (!execve && tr->execve));
    tr->execve = execve;
    d_r_mutex_lock(&all_threads_lock);
    if (execve) {
        /* since we free on a second vfork we should never accumulate
         * more than one
         */
        ASSERT(num_execve_threads == 0);
        num_execve_threads++;
    } else {
        ASSERT(num_execve_threads > 0);
        num_execve_threads--;
    }
    d_r_mutex_unlock(&all_threads_lock);
}
#endif /* UNIX */

int
d_r_get_num_threads(void)
{
    return num_known_threads IF_UNIX(-num_execve_threads);
}

bool
is_last_app_thread(void)
{
    return (d_r_get_num_threads() == get_num_client_threads() + 1);
}

/* This routine takes a snapshot of all the threads known to DR,
 * NOT LIMITED to those currently under DR control!
 * It returns an array of thread_record_t* and the length of the array
 * The caller must free the array using global_heap_free
 * The caller must hold the thread_initexit_lock to ensure that threads
 * are not created or destroyed before the caller is done with the list
 * The caller CANNOT be could_be_linking, else a deadlock with flushing
 * can occur (unless the caller is the one flushing)
 */
static void
get_list_of_threads_common(thread_record_t ***list,
                           int *num _IF_UNIX(bool include_execve))
{
    int i, cur = 0, max_num;
    thread_record_t *tr;
    thread_record_t **mylist;

    /* Only a flushing thread can get the thread snapshot while being
     * couldbelinking -- else a deadlock w/ flush!
     * FIXME: this assert should be on any acquisition of thread_initexit_lock!
     */
    ASSERT(is_self_flushing() || !is_self_couldbelinking());
    ASSERT(all_threads != NULL);
    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);

    d_r_mutex_lock(&all_threads_lock);
    /* Do not include vfork threads that exited via execve, unless we're exiting */
    max_num = IF_UNIX_ELSE((include_execve || dynamo_exiting) ? num_known_threads
                                                              : d_r_get_num_threads(),
                           d_r_get_num_threads());
    mylist = (thread_record_t **)global_heap_alloc(
        max_num * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    for (i = 0; i < HASHTABLE_SIZE(ALL_THREADS_HASH_BITS); i++) {
        for (tr = all_threads[i]; tr != NULL; tr = tr->next) {
            /* include those for which !tr->under_dynamo_control */
            /* don't include those that exited for execve.  there should be
             * no race b/c vfork suspends the parent.  xref i#237/PR 498284.
             */
            if (IF_UNIX_ELSE(!tr->execve || include_execve || dynamo_exiting, true)) {
                mylist[cur] = tr;
                cur++;
            }
        }
    }

    ASSERT(cur > 0);
    IF_WINDOWS(ASSERT(cur == max_num));
    if (cur < max_num) {
        mylist = (thread_record_t **)global_heap_realloc(
            mylist, max_num, cur, sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    }

    *num = cur;
    *list = mylist;
    d_r_mutex_unlock(&all_threads_lock);
}

void
get_list_of_threads(thread_record_t ***list, int *num)
{
    get_list_of_threads_common(list, num _IF_UNIX(false));
}

#ifdef UNIX
void
get_list_of_threads_ex(thread_record_t ***list, int *num, bool include_execve)
{
    get_list_of_threads_common(list, num, include_execve);
}
#endif

/* assumes caller can ensure that thread is either suspended or self to
 * avoid races
 */
thread_record_t *
thread_lookup(thread_id_t tid)
{
    thread_record_t *tr;
    uint hindex;

    /* check that caller is self or has initexit_lock
     * FIXME: no way to tell who has initexit_lock
     */
    ASSERT(mutex_testlock(&thread_initexit_lock) || tid == d_r_get_thread_id());

    hindex = HASH_FUNC_BITS(tid, ALL_THREADS_HASH_BITS);
    d_r_mutex_lock(&all_threads_lock);
    if (all_threads == NULL) {
        tr = NULL;
    } else {
        tr = all_threads[hindex];
    }
    while (tr != NULL) {
        if (tr->id == tid) {
            d_r_mutex_unlock(&all_threads_lock);
            return tr;
        }
        tr = tr->next;
    }
    d_r_mutex_unlock(&all_threads_lock);
    return NULL;
}

/* assumes caller can ensure that thread is either suspended or self to
 * avoid races
 */
uint
get_thread_num(thread_id_t tid)
{
    thread_record_t *tr = thread_lookup(tid);
    if (tr != NULL)
        return tr->num;
    else
        return 0; /* yes can't distinguish from 1st thread, who cares */
}

void
add_thread(IF_WINDOWS_ELSE_NP(HANDLE hthread, process_id_t pid), thread_id_t tid,
           bool under_dynamo_control, dcontext_t *dcontext)
{
    thread_record_t *tr;
    uint hindex;

    ASSERT(all_threads != NULL);

    /* add entry to thread hashtable */
    tr = (thread_record_t *)global_heap_alloc(sizeof(thread_record_t)
                                                  HEAPACCT(ACCT_THREAD_MGT));
#ifdef WINDOWS
    /* we duplicate the thread pseudo-handle, this should give us full rights
     * Note that instead asking explicitly for THREAD_ALL_ACCESS or just for
     * THREAD_TERMINATE|THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT
     * does not seem able to acquire more rights than simply duplicating the
     * app handle gives.
     */
    LOG(GLOBAL, LOG_THREADS, 1, "Thread %d app handle rights: " PFX "\n", tid,
        nt_get_handle_access_rights(hthread));
    duplicate_handle(NT_CURRENT_PROCESS, hthread, NT_CURRENT_PROCESS, &tr->handle, 0, 0,
                     DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES);
    /* We prob. only need TERMINATE (for kill thread), SUSPEND/RESUME/GET_CONTEXT
     * (for synchronizing), and SET_CONTEXT (+ synchronizing requirements, for
     * detach).  All access includes this and quite a bit more. */
#    if 0
    /* eventually should be a real assert, but until we have a story for the
     * injected detach threads, have to ifdef out even the ASSERT_CURIOSITY
     * (even a syslog internal warning is prob. to noisy for QA) */
    ASSERT_CURIOSITY(TESTALL(THREAD_ALL_ACCESS, nt_get_handle_access_rights(tr->handle)));
#    endif
    LOG(GLOBAL, LOG_THREADS, 1, "Thread %d our handle rights: " PFX "\n", tid,
        nt_get_handle_access_rights(tr->handle));
    tr->retakeover = false;
#else
    tr->pid = pid;
    tr->execve = false;
#endif
    tr->id = tid;
    ASSERT(tid != INVALID_THREAD_ID); /* ensure os never assigns invalid id to a thread */
    tr->under_dynamo_control = under_dynamo_control;
    tr->dcontext = dcontext;
    if (dcontext != NULL) /* we allow NULL for dr_create_client_thread() */
        dcontext->thread_record = tr;

    d_r_mutex_lock(&all_threads_lock);
    tr->num = threads_ever_count++;
    hindex = HASH_FUNC_BITS(tr->id, ALL_THREADS_HASH_BITS);
    tr->next = all_threads[hindex];
    all_threads[hindex] = tr;
    /* must be inside all_threads_lock to avoid race w/ get_list_of_threads */
    RSTATS_ADD_PEAK(num_threads, 1);
    RSTATS_INC(num_threads_created);
    num_known_threads++;
    d_r_mutex_unlock(&all_threads_lock);
}

/* return false if couldn't find the thread */
bool
remove_thread(IF_WINDOWS_(HANDLE hthread) thread_id_t tid)
{
    thread_record_t *tr = NULL, *prevtr;
    uint hindex = HASH_FUNC_BITS(tid, ALL_THREADS_HASH_BITS);

    ASSERT(all_threads != NULL);

    d_r_mutex_lock(&all_threads_lock);
    for (tr = all_threads[hindex], prevtr = NULL; tr; prevtr = tr, tr = tr->next) {
        if (tr->id == tid) {
            if (prevtr)
                prevtr->next = tr->next;
            else
                all_threads[hindex] = tr->next;
            /* must be inside all_threads_lock to avoid race w/ get_list_of_threads */
            RSTATS_DEC(num_threads);
#ifdef UNIX
            if (tr->execve) {
                ASSERT(num_execve_threads > 0);
                num_execve_threads--;
            }
#endif
            num_known_threads--;
#ifdef WINDOWS
            close_handle(tr->handle);
#endif
            global_heap_free(tr, sizeof(thread_record_t) HEAPACCT(ACCT_THREAD_MGT));
            break;
        }
    }
    d_r_mutex_unlock(&all_threads_lock);
    return (tr != NULL);
}

/* this bool is protected by reset_pending_lock */
DECLARE_FREQPROT_VAR(static bool reset_at_nth_thread_triggered, false);

/* thread-specific initialization
 * if dstack_in is NULL, then a dstack is allocated; else dstack_in is used
 * as the thread's dstack
 * mc can be NULL for the initial thread
 * returns -1 if current thread has already been initialized
 */
/* On UNIX, if dstack_in != NULL, the parent of this new thread must have
 * increased uninit_thread_count.
 */
int
dynamo_thread_init(byte *dstack_in, priv_mcontext_t *mc, void *os_data,
                   bool client_thread)
{
    dcontext_t *dcontext;
    /* due to lock issues (see below) we need another var */
    bool reset_at_nth_thread_pending = false;
    bool under_dynamo_control = true;
    APP_EXPORT_ASSERT(dynamo_initialized || dynamo_exited || d_r_get_num_threads() == 0 ||
                          client_thread,
                      PRODUCT_NAME " not initialized");
    if (INTERNAL_OPTION(nullcalls)) {
        ASSERT(uninit_thread_count == 0);
        return SUCCESS;
    }

    /* note that ENTERING_DR is assumed to have already happened: in apc handler
     * for win32, in new_thread_setup for linux, in main init for 1st thread
     */
#if defined(WINDOWS) && defined(DR_APP_EXPORTS)
    /* We need to identify a thread we intercepted in its APC when we
     * take over all threads on dr_app_start().  Stack and pc checks aren't
     * simple b/c it can be in ntdll waiting on a lock.
     */
    if (dr_api_entry)
        os_take_over_mark_thread(d_r_get_thread_id());
#endif

    /* Try to handle externally injected threads */
    if (dynamo_initialized && !bb_lock_start)
        pre_second_thread();

    /* synch point so thread creation can be prevented for critical periods */
    d_r_mutex_lock(&thread_initexit_lock);

    /* XXX i#2611: during detach, there is a race where a thread can
     * reach here on Windows despite init_apc_go_native (i#2600).
     */
    ASSERT_BUG_NUM(2611, !doing_detach);

    /* The assumption is that if dynamo_exited, then we are about to exit and
     * clean up, initializing this thread then would be dangerous, better to
     * wait here for the app to die.
     */
    /* under current implementation of process exit, can happen only under
     * debug build, or app_start app_exit interface */
    while (dynamo_exited) {
        /* logging should be safe, though might not actually result in log
         * message */
        DODEBUG_ONCE(LOG(GLOBAL, LOG_THREADS, 1,
                         "Thread %d reached initialization point while dynamo exiting, "
                         "waiting for app to exit\n",
                         d_r_get_thread_id()););
        d_r_mutex_unlock(&thread_initexit_lock);
        os_thread_yield();
        /* just in case we want to support exited and then restarted at some
         * point */
        d_r_mutex_lock(&thread_initexit_lock);
    }

    if (is_thread_initialized()) {
        d_r_mutex_unlock(&thread_initexit_lock);
#if defined(WINDOWS) && defined(DR_APP_EXPORTS)
        if (dr_api_entry)
            os_take_over_unmark_thread(d_r_get_thread_id());
#endif
        return -1;
    }

    os_tls_init();
    dcontext = create_new_dynamo_context(true /*initial*/, dstack_in, mc);
    initialize_dynamo_context(dcontext);
    set_thread_private_dcontext(dcontext);
    /* sanity check */
    ASSERT(get_thread_private_dcontext() == dcontext);

    /* set local state pointer for access from other threads */
    dcontext->local_state = get_local_state();

    /* set initial mcontext, if known */
    if (mc != NULL)
        *get_mcontext(dcontext) = *mc;

    /* For hotp_only, the thread should run native, not under dr.  However,
     * the core should still get control of the thread at hook points to track
     * what the application is doing & at patched points to execute hot patches.
     * It is the same for thin_client except that there are fewer hooks, only to
     * follow children.
     */
    if (RUNNING_WITHOUT_CODE_CACHE())
        under_dynamo_control = false;

    /* add entry to thread hashtable before creating logdir so have thread num.
     * otherwise we'd like to do this only after we'd fully initialized the thread, but we
     * hold the thread_initexit_lock, so nobody should be listing us -- thread_lookup
     * on other than self, or a thread list, should only be done while the initexit_lock
     * is held.  CHECK: is this always correct?  thread_lookup does have an assert
     * to try and enforce but cannot tell who has the lock.
     */
    add_thread(IF_WINDOWS_ELSE(NT_CURRENT_THREAD, get_process_id()), d_r_get_thread_id(),
               under_dynamo_control, dcontext);
#ifdef UNIX /* i#2600: Not easy on Windows: we rely on init_apc_go_native there. */
    if (dstack_in != NULL) { /* Else not a thread creation we observed */
        ASSERT(uninit_thread_count > 0);
        ATOMIC_DEC(int, uninit_thread_count);
    }
#endif
#if defined(WINDOWS) && defined(DR_APP_EXPORTS)
    /* Now that the thread is in the main thread table we don't need to remember it */
    if (dr_api_entry)
        os_take_over_unmark_thread(d_r_get_thread_id());
#endif

    LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1,
        "\ndynamo_thread_init: %d thread(s) now, dcontext=" PFX ", #=%d, id=" TIDFMT
        ", pid=" PIDFMT "\n\n",
        GLOBAL_STAT(num_threads), dcontext, get_thread_num(d_r_get_thread_id()),
        d_r_get_thread_id(), get_process_id());

    DOLOG(1, LOG_STATS, { dump_global_stats(false); });
#ifdef DEBUG
    if (d_r_stats->loglevel > 0) {
        dcontext->logfile = open_log_file(thread_logfile_name(), NULL, 0);
        print_file(dcontext->logfile, "%s\n", dynamorio_version_string);
    } else {
        dcontext->logfile = INVALID_FILE;
    }
    DOLOG(1, LOG_TOP | LOG_THREADS, {
        LOG(THREAD, LOG_TOP | LOG_THREADS, 1, PRODUCT_NAME " built with: %s\n",
            DYNAMORIO_DEFINES);
        LOG(THREAD, LOG_TOP | LOG_THREADS, 1, PRODUCT_NAME " built on: %s\n",
            dynamorio_buildmark);
    });

    LOG(THREAD, LOG_TOP | LOG_THREADS, 1, "%sTHREAD %d (dcontext " PFX ")\n\n",
        client_thread ? "CLIENT " : "", d_r_get_thread_id(), dcontext);
    LOG(THREAD, LOG_TOP | LOG_THREADS, 1,
        "DR stack is " PFX "-" PFX " (passed in " PFX ")\n",
        dcontext->dstack - DYNAMORIO_STACK_SIZE, dcontext->dstack, dstack_in);
#endif

#ifdef DEADLOCK_AVOIDANCE
    locks_thread_init(dcontext);
#endif
    heap_thread_init(dcontext);
    DOSTATS({ stats_thread_init(dcontext); });
#ifdef KSTATS
    kstat_thread_init(dcontext);
#endif
    os_thread_init(dcontext, os_data);
    arch_thread_init(dcontext);
    synch_thread_init(dcontext);

    if (!DYNAMO_OPTION(thin_client))
        vm_areas_thread_init(dcontext);

    monitor_thread_init(dcontext);
    fcache_thread_init(dcontext);
    link_thread_init(dcontext);
    fragment_thread_init(dcontext);

    /* OS thread init after synch_thread_init and other setup can handle signals, etc. */
    os_thread_init_finalize(dcontext, os_data);

    /* This lock has served its purposes: A) a barrier to thread creation for those
     * iterating over threads, B) mutex for add_thread, and C) mutex for synch_field
     * to be set up.
     * So we release it to shrink the time spent w/ this big lock, in particular
     * to avoid holding it while running private lib thread init code (i#875).
     */
    d_r_mutex_unlock(&thread_initexit_lock);

    /* Set up client data needed in loader_thread_init for IS_CLIENT_THREAD */
    instrument_client_thread_init(dcontext, client_thread);

    loader_thread_init(dcontext);

    if (!DYNAMO_OPTION(thin_client)) {
        /* put client last, may depend on other thread inits.
         * Note that we are calling this prior to instrument_init()
         * now (PR 216936), which is required to initialize
         * the client dcontext field prior to instrument_init().
         */
        instrument_thread_init(dcontext, client_thread, mc != NULL);

#ifdef SIDELINE
        if (dynamo_options.sideline) {
            /* wake up sideline thread -- ok to call if thread already awake */
            sideline_start();
        }
#endif
    }

    /* must check # threads while holding thread_initexit_lock, yet cannot
     * call fcache_reset_all_caches_proactively while holding it due to
     * rank order of reset_pending_lock which we must also hold -- so we
     * set a local bool reset_at_nth_thread_pending
     */
    if (DYNAMO_OPTION(reset_at_nth_thread) != 0 && !reset_at_nth_thread_triggered &&
        (uint)d_r_get_num_threads() == DYNAMO_OPTION(reset_at_nth_thread)) {
        d_r_mutex_lock(&reset_pending_lock);
        if (!reset_at_nth_thread_triggered) {
            reset_at_nth_thread_triggered = true;
            reset_at_nth_thread_pending = true;
        }
        d_r_mutex_unlock(&reset_pending_lock);
    }

    DOLOG(1, LOG_STATS, { dump_thread_stats(dcontext, false); });

    if (reset_at_nth_thread_pending) {
        d_r_mutex_lock(&reset_pending_lock);
        /* fcache_reset_all_caches_proactively() will unlock */
        fcache_reset_all_caches_proactively(RESET_ALL);
    }
    return SUCCESS;
}

/* We don't free cur thread until after client exit event (PR 536058) except for
 * fragment_thread_exit().  Since this is called outside of dynamo_thread_exit()
 * on process exit we assume fine to skip enter_threadexit().
 */
void
dynamo_thread_exit_pre_client(dcontext_t *dcontext, thread_id_t id)
{
    /* fcache stats needs to examine fragment state, so run it before
     * fragment exit, but real fcache exit needs to be after fragment exit
     */
#ifdef DEBUG
    fcache_thread_exit_stats(dcontext);
#endif
    /* must abort now to avoid deleting possibly un-deletable fragments
     * monitor_thread_exit remains later b/c of monitor_remove_fragment calls
     */
    trace_abort_and_delete(dcontext);
    fragment_thread_exit(dcontext);
    IF_WINDOWS(loader_pre_client_thread_exit(dcontext));
    instrument_thread_exit_event(dcontext);
}

/* thread-specific cleanup */
/* Note : if this routine is not called by thread id, then other_thread should
 * be true and the calling thread should hold the thread_initexit_lock
 */
static int
dynamo_thread_exit_common(dcontext_t *dcontext, thread_id_t id,
                          IF_WINDOWS_(bool detach_stacked_callbacks) bool other_thread)
{
    dcontext_t *dcontext_tmp;
#ifdef WINDOWS
    dcontext_t *dcontext_next;
    int num_dcontext;
#endif
    bool on_dstack = !other_thread && is_currently_on_dstack(dcontext);
    /* cache this now for use after freeing dcontext */
    local_state_t *local_state = dcontext->local_state;

    if (INTERNAL_OPTION(nullcalls) || dcontext == NULL)
        return SUCCESS;

    /* make sure don't get into deadlock w/ flusher */
    enter_threadexit(dcontext);

    /* synch point so thread exiting can be prevented for critical periods */
    /* see comment at start of method for other thread exit */
    if (!other_thread)
        d_r_mutex_lock(&thread_initexit_lock);

    ASSERT_OWN_MUTEX(true, &thread_initexit_lock);
#ifdef WINDOWS
    /* need to clean up thread stack before clean up other thread data, but
     * after we're made nolinking
     */
    os_thread_stack_exit(dcontext);
    /* free the thread's application stack if requested */
    if (dcontext->free_app_stack) {
        byte *base;
        /* only used for nudge threads currently */
        ASSERT(dcontext->nudge_target == generic_nudge_target);
        if (get_stack_bounds(dcontext, &base, NULL)) {
            NTSTATUS res;
            ASSERT(base != NULL);
            res = nt_free_virtual_memory(base);
            ASSERT(NT_SUCCESS(res));
        } else {
            /* stack should be available here */
            ASSERT_NOT_REACHED();
        }
    }
#endif

#ifdef SIDELINE
    /* N.B.: do not clean up any data structures while sideline thread
     * is still running!  put it to sleep for duration of this routine!
     */
    if (!DYNAMO_OPTION(thin_client)) {
        if (dynamo_options.sideline) {
            /* put sideline thread to sleep */
            sideline_stop();
            /* sideline_stop will not return until sideline thread is asleep */
        }
    }
#endif

    LOG(GLOBAL, LOG_TOP | LOG_THREADS, 1,
        "\ndynamo_thread_exit (thread #%d id=" TIDFMT "): %d thread(s) now\n\n",
        get_thread_num(id), id, GLOBAL_STAT(num_threads) - 1);

    DOLOG(1, LOG_STATS, { dump_global_stats(false); });

    LOG(THREAD, LOG_STATS | LOG_THREADS, 1, "\n## Statistics for this thread:\n");

#ifdef PROFILE_RDTSC
    if (dynamo_options.profile_times) {
        int i;
        ASSERT(dcontext);
        LOG(THREAD, LOG_STATS | LOG_THREADS, 1, "\nTop ten cache times:\n");
        for (i = 0; i < 10; i++) {
            if (dcontext->cache_time[i] > (uint64)0) {
                uint top_part, bottom_part;
                divide_int64_print(dcontext->cache_time[i], kilo_hertz, false, 3,
                                   &top_part, &bottom_part);
                LOG(THREAD, LOG_STATS | LOG_THREADS, 1,
                    "\t#%2d = %6u.%.3u ms, %9d hits\n", i + 1, top_part, bottom_part,
                    (int)dcontext->cache_count[i]);
            }
        }
        LOG(THREAD, LOG_STATS | LOG_THREADS, 1, "\n");
    }
#endif

    /* In order to pass the client a dcontext in the process exit event
     * we do some thread cleanup early for the final thread so we can delay
     * the rest (PR 536058)
     */
    if (!dynamo_exited_and_cleaned)
        dynamo_thread_exit_pre_client(dcontext, id);
    /* PR 243759: don't free client_data until after all fragment deletion events */
    if (!DYNAMO_OPTION(thin_client))
        instrument_thread_exit(dcontext);

    /* i#920: we can't take segment/timer/asynch actions for other threads.
     * This must be called after dynamo_thread_exit_pre_client where
     * we called event callbacks.
     */
    if (!other_thread) {
        dynamo_thread_not_under_dynamo(dcontext);
#ifdef WINDOWS
        /* We don't do this inside os_thread_not_under_dynamo b/c we do it in
         * context switches.  os_loader_exit() will call this, but it has no
         * dcontext, so it won't swap internal TEB fields.
         */
        swap_peb_pointer(dcontext, false /*to app*/);
#endif
    }

    /* We clean up priv libs prior to setting tls dc to NULL so we can use
     * TRY_EXCEPT when calling the priv lib entry routine
     */
    if (!dynamo_exited ||
        (other_thread &&
         (IF_WINDOWS_ELSE(!doing_detach, true) ||
          dcontext->owning_thread != d_r_get_thread_id()))) /* else already did this */
        loader_thread_exit(dcontext);

    /* set tls dc to NULL prior to cleanup, to avoid problems handling
     * alarm signals received during cleanup (we'll suppress if tls
     * dc==NULL which seems the right thing to do: not worth our
     * effort to pass to another thread if thread-group-shared alarm,
     * and if thread-private then thread would have exited soon
     * anyway).  see PR 596127.
     */
    /* make sure we invalidate the dcontext before releasing the memory  */
    /* when cleaning up other threads, we cannot set their dcs to null,
     * but we only do this at dynamorio_app_exit so who cares
     */
    /* This must be called after instrument_thread_exit, which uses
     * get_thread_private_dcontext for app/dr state checks.
     */
    if (id == d_r_get_thread_id())
        set_thread_private_dcontext(NULL);

    fcache_thread_exit(dcontext);
    link_thread_exit(dcontext);
    monitor_thread_exit(dcontext);
    if (!DYNAMO_OPTION(thin_client))
        vm_areas_thread_exit(dcontext);
    synch_thread_exit(dcontext);
    arch_thread_exit(dcontext _IF_WINDOWS(detach_stacked_callbacks));
    os_thread_exit(dcontext, other_thread);
    DOLOG(1, LOG_STATS, { dump_thread_stats(dcontext, false); });
#ifdef KSTATS
    kstat_thread_exit(dcontext);
#endif
    DOSTATS({ stats_thread_exit(dcontext); });
    heap_thread_exit(dcontext);
#ifdef DEADLOCK_AVOIDANCE
    locks_thread_exit(dcontext);
#endif

#ifdef DEBUG
    if (dcontext->logfile != INVALID_FILE && dcontext->logfile != STDERR) {
        os_flush(dcontext->logfile);
        close_log_file(dcontext->logfile);
    }
#endif

    /* remove thread from threads hashtable */
    remove_thread(IF_WINDOWS_(NT_CURRENT_THREAD) id);

    dcontext_tmp = dcontext;
#ifdef WINDOWS
    /* clean up all the dcs */
    num_dcontext = 0;
    /* Already at one end of list. Delete through to other end. */
    while (dcontext_tmp) {
        num_dcontext++;
        dcontext_next = dcontext_tmp->prev_unused;
        delete_dynamo_context(dcontext_tmp,
                              dcontext_tmp == dcontext /*do not free dup cb stacks*/
                                  && !on_dstack /*do not free own stack*/);
        dcontext_tmp = dcontext_next;
    }
    LOG(GLOBAL, LOG_STATS | LOG_THREADS, 1, "\tdynamo contexts used: %d\n", num_dcontext);
#else  /* UNIX */
    delete_dynamo_context(dcontext_tmp, !on_dstack /*do not free own stack*/);
#endif /* UNIX */
    os_tls_exit(local_state, other_thread);

#ifdef SIDELINE
    /* see notes above -- we can now wake up sideline thread */
    if (dynamo_options.sideline && d_r_get_num_threads() > 0) {
        sideline_start();
    }
#endif
    if (!other_thread) {
        d_r_mutex_unlock(&thread_initexit_lock);
        /* FIXME: once thread_initexit_lock is released, we're not on
         * thread list, and a terminate targeting us could kill us in the middle
         * of this call -- but this can't come before the unlock b/c the lock's
         * in the data segment!  (see case 3121)
         * (note we do not re-protect for process exit, see !dynamo_exited check
         * in exiting_dynamorio)
         */
        if (!on_dstack) {
            EXITING_DR();
            /* else, caller will clean up stack and then call EXITING_DR(),
             * probably via dynamo_thread_stack_free_and_exit(), as the stack free
             * must be done before the exit
             */
        }
    }

    return SUCCESS;
}

/* NOINLINE because dynamo_thread_exit is a stopping point. */
NOINLINE int
dynamo_thread_exit(void)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    return dynamo_thread_exit_common(dcontext, d_r_get_thread_id(),
                                     IF_WINDOWS_(false) false);
}

/* NOTE : you must hold thread_initexit_lock to call this function! */
int
dynamo_other_thread_exit(thread_record_t *tr _IF_WINDOWS(bool detach_stacked_callbacks))
{
    /* FIXME: Usually a safe spot for cleaning other threads should be
     * under num_exits_dir_syscall, but for now rewinding all the way
     */
    KSTOP_REWIND_DC(tr->dcontext, thread_measured);
    KSTART_DC(tr->dcontext, thread_measured);
    return dynamo_thread_exit_common(tr->dcontext, tr->id,
                                     IF_WINDOWS_(detach_stacked_callbacks) true);
}

/* Called from another stack to finish cleaning up a thread.
 * The final steps are to free the stack and perform the exit hook.
 */
void
dynamo_thread_stack_free_and_exit(byte *stack)
{
    if (stack != NULL) {
        stack_free(stack, DYNAMORIO_STACK_SIZE);
        /* ASSUMPTION: if stack is NULL here, the exit was done earlier
         * (fixes case 6967)
         */
        EXITING_DR();
    }
}

#ifdef DR_APP_EXPORTS
/* API routine to initialize DR */
DR_APP_API int
dr_app_setup(void)
{
    /* FIXME: we either have to disallow the client calling this with
     * more than one thread running, or we have to suspend all the threads.
     * We should share the suspend-and-takeover loop (and for dr_app_setup_and_start
     * share the takeover portion) from dr_app_start().
     */
    int res;
    dcontext_t *dcontext;
    /* If this is a re-attach, .data might be read-only.
     * We'll re-protect at the end of dynamorio_app_init().
     */
    if (DATASEC_WRITABLE(DATASEC_RARELY_PROT) == 0)
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    dr_api_entry = true;
    dynamo_control_via_attach = true;
    res = dynamorio_app_init();
    /* For dr_api_entry, we do not install all our signal handlers during init (to avoid
     * races: i#2335): we delay until dr_app_start().  Plus the vsyscall hook is
     * not set up until we find out the syscall method.  Thus we're already
     * "os_process_not_under_dynamorio".
     * We can't as easily avoid initializing the thread TLS and then dropping
     * it, however, as parts of init assume we have TLS.
     */
    dcontext = get_thread_private_dcontext();
    dynamo_thread_not_under_dynamo(dcontext);
    return res;
}

/* API routine to exit DR */
DR_APP_API int
dr_app_cleanup(void)
{
    thread_record_t *tr;
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    dr_api_exit = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT); /* to keep properly nested */

    /* XXX: The dynamo_thread_[not_]under_dynamo() routines are not idempotent,
     * and must be balanced!  On Linux, they track the shared itimer refcount,
     * so a mismatch will lead to a refleak or negative refcount.
     * dynamorio_app_exit() will call dynamo_thread_not_under_dynamo(), so we
     * must ensure that we are under DR before calling it.  Therefore, we
     * require that the caller call dr_app_stop() before calling
     * dr_app_cleanup().  However, we cannot make a usage assertion to that
     * effect without addressing the FIXME comments in
     * dynamo_thread_not_under_dynamo() about updating tr->under_dynamo_control.
     */
    tr = thread_lookup(d_r_get_thread_id());
    if (tr != NULL && tr->dcontext != NULL) {
        os_process_under_dynamorio_initiate(tr->dcontext);
        os_process_under_dynamorio_complete(tr->dcontext);
        dynamo_thread_under_dynamo(tr->dcontext);
    }
    return dynamorio_app_exit();
}

/* Called by dr_app_start in arch-specific assembly file */
void
dr_app_start_helper(priv_mcontext_t *mc)
{
    apicheck(dynamo_initialized, PRODUCT_NAME " not initialized");
    LOG(GLOBAL, LOG_TOP, 1, "dr_app_start in thread " TIDFMT "\n", d_r_get_thread_id());
    LOG(THREAD_GET, LOG_TOP, 1, "dr_app_start\n");

    if (!INTERNAL_OPTION(nullcalls)) {
        /* Adjust the app stack to account for the return address + alignment.
         * See dr_app_start in x86.asm.
         */
        mc->xsp += DYNAMO_START_XSP_ADJUST;
        dynamo_start(mc);
        /* the interpreter takes over from here */
    }
}

/* Dummy routine that returns control to the app if it is currently
 * under dynamo control.
 * NOINLINE because dr_app_stop is a stopping point.
 */
DR_APP_API NOINLINE void
dr_app_stop(void)
{
    /* the application regains control in here */
}

/* NOINLINE because dr_app_stop_and_cleanup is a stopping point. */
DR_APP_API NOINLINE void
dr_app_stop_and_cleanup(void)
{
    dr_app_stop_and_cleanup_with_stats(NULL);
}

/* NOINLINE because dr_app_stop_and_cleanup_with_stats is a stopping point. */
DR_APP_API NOINLINE void
dr_app_stop_and_cleanup_with_stats(dr_stats_t *drstats)
{
    /* XXX i#95: today this is a full detach, while a separated dr_app_cleanup()
     * is not.  We should try and have dr_app_cleanup() take this detach path
     * here (and then we can simplify exit_synch_state()) but it's more complicated
     * and we need to resolve the unbounded dr_app_stop() time.
     */
    if (dynamo_initialized && !dynamo_exited && !doing_detach) {
#    ifdef WINDOWS
        /* dynamo_thread_exit_common will later swap to app. */
        swap_peb_pointer(get_thread_private_dcontext(), true /*to priv*/);
#    endif
        detach_on_permanent_stack(true /*internal*/, true /*do cleanup*/, drstats);
    }
    /* the application regains control in here */
}

DR_APP_API int
dr_app_setup_and_start(void)
{
    int r = dr_app_setup();
    if (r == SUCCESS)
        dr_app_start();
    return r;
}
#endif

/* For use by threads that start and stop whether dynamo controls them.
 */
void
dynamo_thread_under_dynamo(dcontext_t *dcontext)
{
    LOG(THREAD, LOG_ASYNCH, 2, "thread %d under DR control\n", dcontext->owning_thread);
    ASSERT(dcontext != NULL);
    /* FIXME: mark under_dynamo_control?
     * see comments in not routine below
     */
    os_thread_under_dynamo(dcontext);
#ifdef SIDELINE
    if (dynamo_options.sideline) {
        /* wake up sideline thread -- ok to call if thread already awake */
        sideline_start();
    }
#endif
    dcontext->currently_stopped = false;
    dcontext->go_native = false;
}

/* For use by threads that start and stop whether dynamo controls them.
 * This must be called by the owner of dcontext and not another
 * non-executing thread.
 */
void
dynamo_thread_not_under_dynamo(dcontext_t *dcontext)
{
    ASSERT_MESSAGE(CHKLVL_ASSERTS + 1 /*expensive*/, "can only act on executing thread",
                   dcontext == get_thread_private_dcontext());
    if (dcontext == NULL)
        return;
    LOG(THREAD, LOG_ASYNCH, 2, "thread %d not under DR control\n",
        dcontext->owning_thread);
    dcontext->currently_stopped = true;
    os_thread_not_under_dynamo(dcontext);
#ifdef SIDELINE
    /* FIXME: if # active threads is 0, then put sideline thread to sleep! */
    if (dynamo_options.sideline) {
        /* put sideline thread to sleep */
        sideline_stop();
    }
#endif
#ifdef DEBUG
    os_flush(dcontext->logfile);
#endif
}

/* Mark this thread as under DR, and take over other threads in the current process.
 */
void
dynamorio_take_over_threads(dcontext_t *dcontext)
{
    /* We repeatedly check if there are other threads in the process, since
     * while we're checking one may be spawning additional threads.
     */
    bool found_threads;
    uint attempts = 0;
    uint max_takeover_attempts = DYNAMO_OPTION(takeover_attempts);

    os_process_under_dynamorio_initiate(dcontext);
    /* We can start this thread now that we've set up process-wide actions such
     * as handling signals.
     */
    dynamo_thread_under_dynamo(dcontext);
    signal_event(dr_app_started);
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    dynamo_started = true;
    /* Similarly, with our signal handler back in place, we remove the TLS limit. */
    detacher_tid = INVALID_THREAD_ID;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    /* XXX i#1305: we should suspend all the other threads for DR init to
     * satisfy the parts of the init process that assume there are no races.
     */
    do {
        found_threads = os_take_over_all_unknown_threads(dcontext);
        attempts++;
        if (found_threads && !bb_lock_start)
            bb_lock_start = true;
        if (DYNAMO_OPTION(sleep_between_takeovers))
            os_thread_sleep(1);
    } while (found_threads && attempts < max_takeover_attempts);
    os_process_under_dynamorio_complete(dcontext);

    instrument_post_attach_event();

    /* End the barrier to new threads. */
    signal_event(dr_attach_finished);

    if (found_threads) {
        REPORT_FATAL_ERROR_AND_EXIT(FAILED_TO_TAKE_OVER_THREADS, 2,
                                    get_application_name(), get_application_pid());
    }
    char buf[16];
    int num_threads = d_r_get_num_threads();
    if (num_threads > 1) { /* avoid for early injection */
        snprintf(buf, BUFFER_SIZE_ELEMENTS(buf), "%d", num_threads);
        NULL_TERMINATE_BUFFER(buf);
        SYSLOG(SYSLOG_INFORMATION, INFO_ATTACHED, 3, buf, get_application_name(),
               get_application_pid());
    }
}

/* Called by dynamorio_app_take_over in arch-specific assembly file */
void
dynamorio_app_take_over_helper(priv_mcontext_t *mc)
{
    static bool have_taken_over = false; /* ASSUMPTION: not an actual write */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    APP_EXPORT_ASSERT(dynamo_initialized, PRODUCT_NAME " not initialized");
#ifdef RETURN_AFTER_CALL
    /* FIXME : this is set after dynamo_initialized, so a slight race with
     * an injected thread turning on .C protection before the main thread
     * sets this. */
    dr_preinjected = true; /* currently only relevant on Win32 */
#endif
    LOG(GLOBAL, LOG_TOP, 1, "taking over via preinject in %s\n", __FUNCTION__);

    if (!INTERNAL_OPTION(nullcalls) && !have_taken_over) {
        have_taken_over = true;
        LOG(GLOBAL, LOG_TOP, 1, "dynamorio_app_take_over\n");
        /* set this flag to indicate that we should run until the program dies: */
        automatic_startup = true;

        if (DYNAMO_OPTION(inject_primary))
            take_over_primary_thread();

        /* who knows when this was called -- no guarantee we control all threads --
         * unless we were auto-injected (preinject library calls this routine)
         */
        control_all_threads = automatic_startup;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

        if (IF_WINDOWS_ELSE(!dr_earliest_injected && !dr_early_injected, true)) {
            /* Adjust the app stack to account for the return address + alignment.
             * See dynamorio_app_take_over in x86.asm.
             */
            mc->xsp += DYNAMO_START_XSP_ADJUST;
        }

        /* For hotp_only and thin_client, the app should run native, except
         * for our hooks.
         * This is where apps hooked using appinit key are let go native.
         * Even though control is going to native app code, we want
         * automatic_startup and control_all_threads set.
         */
        if (!RUNNING_WITHOUT_CODE_CACHE())
            dynamo_start(mc);
        /* the interpreter takes over from here */
    } else
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
}

#ifdef WINDOWS
extern app_pc parent_early_inject_address; /* from os.c */

/* in arch-specific assembly file */
void
dynamorio_app_take_over(void);

DYNAMORIO_EXPORT void
dynamorio_app_init_and_early_takeover(uint inject_location, void *restore_code)
{
    int res;
    ASSERT(!dynamo_initialized && !dynamo_exited);
    /* This routine combines dynamorio_app_init() and dynamrio_app_takeover into
     * a single routine that also handles any early injection cleanup needed. */
    ASSERT_NOT_IMPLEMENTED(inject_location != INJECT_LOCATION_KiUserApc);
    /* currently only Ldr* hook points are known to work */
    ASSERT_CURIOSITY(INJECT_LOCATION_IS_LDR(inject_location));
    /* See notes in os.c DLLMain. When early injected we are unable to find
     * the address of LdrpLoadDll so we use the parent's value which is passed
     * to us at the start of restore_code. FIXME - if we start using multiple
     * inject locations we'll probably have to ensure we always pass this.
     */
    if (INJECT_LOCATION_IS_LDR(inject_location)) {
        parent_early_inject_address = *(app_pc *)restore_code;
    }
    dr_early_injected = true;
    dr_early_injected_location = inject_location;
    res = dynamorio_app_init();
    ASSERT(res == SUCCESS);
    ASSERT(dynamo_initialized && !dynamo_exited);
    LOG(GLOBAL, LOG_TOP, 1, "taking over via early injection in %s\n", __FUNCTION__);
    /* FIXME - restore code needs to be freed, but we have to return through it
     * first... could instead duplicate its tail here if we wrap this
     * routine in asm or eqv. pass the continuation state in as args. */
    ASSERT(inject_location != INJECT_LOCATION_KiUserApc);
    dynamorio_app_take_over();
}

/* Called with DR library mapped in but without its imports processed.
 */
void
dynamorio_earliest_init_takeover_C(byte *arg_ptr, priv_mcontext_t *mc)
{
    int res;
    bool earliest_inject;

    /* Windows-specific code for the most part */
    earliest_inject = earliest_inject_init(arg_ptr);

    /* Initialize now that DR dll imports are hooked up */
    if (earliest_inject) {
        dr_earliest_injected = true;
        dr_earliest_inject_args = arg_ptr;
    } else
        dr_early_injected = true;
    res = dynamorio_app_init();
    ASSERT(res == SUCCESS);
    ASSERT(dynamo_initialized && !dynamo_exited);
    LOG(GLOBAL, LOG_TOP, 1, "taking over via earliest injection in %s\n", __FUNCTION__);

    /* earliest_inject_cleanup() is called within dynamorio_app_init() to avoid
     * confusing the exec areas scan
     */

    dynamorio_app_take_over_helper(mc);
}
#endif /* WINDOWS */

/***************************************************************************
 * SELF-PROTECTION
 */

/* FIXME: even with -single_privileged_thread, we aren't fully protected,
 * because there's a window between us resuming the other threads and
 * returning to our caller where another thread could clobber our return
 * address or something.
 */
static void
dynamorio_protect(void)
{
    ASSERT(SELF_PROTECT_ON_CXT_SWITCH);
    LOG(GLOBAL, LOG_DISPATCH, 4, "dynamorio_protect thread=" TIDFMT "\n",
        d_r_get_thread_id());
    /* we don't protect local heap here, that's done lazily */

    d_r_mutex_lock(&protect_info->lock);
    ASSERT(protect_info->num_threads_unprot > 0);
    /* FIXME: nice to also catch double enters but would need to track more info */
    if (protect_info->num_threads_unprot <= 0) {
        /* Defensive code to prevent crashes from double exits (the theory
         * for case 7631/8030).  However, this precludes an extra exit+enter
         * pair from working properly (though an extra enter+exit will continue
         * to work), though such a pair would have crashed if another thread
         * had entered in the interim anyway.
         */
        protect_info->num_threads_unprot = 0;
        d_r_mutex_unlock(&protect_info->lock);
        return;
    }
    protect_info->num_threads_unprot--;
    if (protect_info->num_threads_unprot > 0) {
        /* other threads still in DR, cannot protect global memory */
        LOG(GLOBAL, LOG_DISPATCH, 4, "dynamorio_protect: not last thread => nop\n");
        d_r_mutex_unlock(&protect_info->lock);
        return;
    }

    SELF_PROTECT_GLOBAL(READONLY);

    if (INTERNAL_OPTION(single_privileged_thread)) {
        /* FIXME: want to resume threads and allow thread creation only
         * _after_ protect data segment, but lock is in data segment!
         */
        if (protect_info->num_threads_suspended > 0) {
            thread_record_t *tr;
            int i, num = 0;
            /* we do not need to grab the all_threads_lock because
             * no threads can be added or removed so who cares if we
             * access the data structure simultaneously with another
             * reader of it
             */
            for (i = 0; i < HASHTABLE_SIZE(ALL_THREADS_HASH_BITS); i++) {
                for (tr = all_threads[i]; tr; tr = tr->next) {
                    if (tr->under_dynamo_control) {
                        os_thread_resume(all_threads[i]);
                        num++;
                    }
                }
            }
            ASSERT(num == protect_info->num_threads_suspended);
            protect_info->num_threads_suspended = 0;
        }

        /* thread init/exit can proceed now */
        d_r_mutex_unlock(&thread_initexit_lock);
    }

    /* FIXME case 8073: temporary until we put in unprots in the
     * right places.  if we were to leave this here we'd want to combine
     * .fspdata and .cspdata for more efficient prot changes.
     */
    SELF_PROTECT_DATASEC(DATASEC_FREQ_PROT);
    SELF_PROTECT_DATASEC(DATASEC_CXTSW_PROT);

    d_r_mutex_unlock(&protect_info->lock);
}

static void
dynamorio_unprotect(void)
{
    ASSERT(SELF_PROTECT_ON_CXT_SWITCH);

    d_r_mutex_lock(
        &protect_info->lock); /* lock in unprot heap, not data segment, so safe! */
    protect_info->num_threads_unprot++;
    if (protect_info->num_threads_unprot == 1) {
        /* was protected, so we need to do the unprotection */
        SELF_UNPROTECT_DATASEC(DATASEC_CXTSW_PROT);
        /* FIXME case 8073: temporary until we put in unprots in the
         * right places.  if we were to leave this here we'd want to combine
         * .fspdata and .cspdata for more efficient prot changes.
         */
        SELF_UNPROTECT_DATASEC(DATASEC_FREQ_PROT);

        if (INTERNAL_OPTION(single_privileged_thread)) {
            /* FIXME: want to suspend all other threads _before_ unprotecting anything,
             * but need to guarantee no new threads while we're suspending them,
             * and can't do that without setting a lock => need data segment!
             */
            d_r_mutex_lock(&thread_initexit_lock);

            if (d_r_get_num_threads() > 1) {
                thread_record_t *tr;
                int i;
                /* current multiple-thread solution: suspend all other threads! */
                ASSERT(protect_info->num_threads_suspended == 0);
                /* we do not need to grab the all_threads_lock because
                 * no threads can be added or removed so who cares if we
                 * access the data structure simultaneously with another
                 * reader of it
                 */
                for (i = 0; i < HASHTABLE_SIZE(ALL_THREADS_HASH_BITS); i++) {
                    for (tr = all_threads[i]; tr; tr = tr->next) {
                        if (tr->under_dynamo_control) {
                            DEBUG_DECLARE(bool ok =)
                            os_thread_suspend(all_threads[i]);
                            ASSERT(ok);
                            protect_info->num_threads_suspended++;
                        }
                    }
                }
            }
            /* we don't unlock or resume threads until we re-enter cache */
        }

        SELF_PROTECT_GLOBAL(WRITABLE);
    }
    /* we don't re-protect local heap here, that's done at points where
     * it was protected lazily
     */
    d_r_mutex_unlock(&protect_info->lock);
    LOG(GLOBAL, LOG_DISPATCH, 4, "dynamorio_unprotect thread=" TIDFMT "\n",
        d_r_get_thread_id());
}

#ifdef DEBUG
const char *
get_data_section_name(app_pc pc)
{
    uint i;
    for (i = 0; i < DATASEC_NUM; i++) {
        if (pc >= datasec_start[i] && pc < datasec_end[i])
            return DATASEC_NAMES[i];
    }
    return NULL;
}

bool
check_should_be_protected(uint sec)
{
    /* Blindly asserting that a data section is protected is racy as
     * another thread could be in an unprot window.  We use some
     * heuristics to try and identify bugs where a section is left
     * unprot, but it's not easy.
     */
    if (/* case 8107: for INJECT_LOCATION_LdrpLoadImportModule we
         * load a helper library and end up in d_r_dispatch() for
         * syscall_while_native before DR is initialized.
         */
        !dynamo_initialized ||
#    ifdef WINDOWS
        /* case 8113: detach currently unprots .data prior to its
         * thread synch, so don't count anything after that
         */
        doing_detach ||
#    endif
        !TEST(DATASEC_SELFPROT[sec], DYNAMO_OPTION(protect_mask)) ||
        DATASEC_PROTECTED(sec))
        return true;
    STATS_INC(datasec_not_prot);
    /* FIXME: even checking d_r_get_num_threads()==1 is still racy as a thread could
     * exit, and it's not worth grabbing thread_initexit_lock here..
     */
    if (threads_ever_count == 1
#    ifdef DR_APP_EXPORTS
        /* For start/stop, can be other threads running around so we bail on
         * perfect protection
         */
        && !dr_api_entry
#    endif
    )
        return false;
    /* FIXME: no count of threads in DR or anything so can't conclude much
     * Just return true and hope developer looks at datasec_not_prot stats.
     * We do have an ASSERT_CURIOSITY on the stat in data_section_exit().
     */
    return true;
}

#    ifdef WINDOWS
/* Assumed to only be called about DR dll writable regions */
bool
data_sections_enclose_region(app_pc start, app_pc end)
{
    /* Rather than solve the general enclose problem by sorting,
     * we subtract each piece we find.
     * It used to be that on 32-bit .data|.fspdata|.cspdata|.nspdata formed
     * the only writable region, with .pdata between .data and .fspdata on 64.
     * But building with VS2012, I'm seeing the sections in other orders (i#1075).
     * And with x64 reachability we moved the interception buffer in .data,
     * and marking it +rx results in sub-section calls to here.
     */
    int i;
    bool found_start = false, found_end = false;
    ssize_t sz = end - start;
    for (i = 0; i < DATASEC_NUM; i++) {
        if (datasec_start[i] <= end && datasec_end[i] >= start) {
            byte *overlap_start = MAX(datasec_start[i], start);
            byte *overlap_end = MIN(datasec_end[i], end);
            sz -= overlap_end - overlap_start;
        }
    }
    return sz == 0;
}
#    endif /* WINDOWS */
#endif     /* DEBUG */

static void
get_data_section_bounds(uint sec)
{
    /* FIXME: on linux we should include .got and .dynamic in one of our
     * sections, requiring specifying the order of sections (case 3789)!
     * Should use an ld script to ensure that .nspdata is last, or find a unique
     * attribute to force separation (perhaps mark as rwx, then
     * remove the x at init time?)  ld 2.15 puts it at the end, but
     * ld 2.13 puts .got and .dynamic after it!  For now we simply
     * don't protect subsequent guys.
     * On win32 there are no other rw sections, fortunately.
     */
    ASSERT(sec >= 0 && sec < DATASEC_NUM);
    /* for DEBUG we use for data_sections_enclose_region() */
    ASSERT(IF_WINDOWS(IF_DEBUG(true ||))
               TEST(DATASEC_SELFPROT[sec], dynamo_options.protect_mask));
    d_r_mutex_lock(&datasec_lock[sec]);
    ASSERT(datasec_start[sec] == NULL);
    get_named_section_bounds(get_dynamorio_dll_start(), DATASEC_NAMES[sec],
                             &datasec_start[sec], &datasec_end[sec]);
    d_r_mutex_unlock(&datasec_lock[sec]);
    ASSERT(ALIGNED(datasec_start[sec], PAGE_SIZE));
    ASSERT(ALIGNED(datasec_end[sec], PAGE_SIZE));
    ASSERT(datasec_start[sec] < datasec_end[sec]);
#ifdef WINDOWS
    if (IF_DEBUG(true ||) TEST(DATASEC_SELFPROT[sec], dynamo_options.protect_mask))
        merge_writecopy_pages(datasec_start[sec], datasec_end[sec]);
#endif
}

#ifdef UNIX
/* We get into problems if we keep a .section open across string literals, etc.
 * (such as when wrapping a function to get its local-scope statics in that section),
 * but the VAR_IN_SECTION does the real work for us, just so long as we have one
 * .section decl somewhere.
 */
DECLARE_DATA_SECTION(RARELY_PROTECTED_SECTION, "w")
DECLARE_DATA_SECTION(FREQ_PROTECTED_SECTION, "w")
DECLARE_DATA_SECTION(NEVER_PROTECTED_SECTION, "w")
END_DATA_SECTION_DECLARATIONS()
#endif

static void
data_section_init(void)
{
    uint i;
    for (i = 0; i < DATASEC_NUM; i++) {
        if (datasec_start[i] != NULL) {
            /* We were called early due to an early syslog.
             * We still retain our slightly later normal init position so we can
             * log, etc. in normal runs.
             */
            return;
        }
        ASSIGN_INIT_LOCK_FREE(datasec_lock[i], datasec_selfprot_lock);
        /* for DEBUG we use for data_sections_enclose_region() */
        if (IF_WINDOWS(IF_DEBUG(true ||))
                TEST(DATASEC_SELFPROT[i], dynamo_options.protect_mask)) {
            get_data_section_bounds(i);
        }
    }
    DOCHECK(1, {
        /* ensure no overlaps */
        uint j;
        for (i = 0; i < DATASEC_NUM; i++) {
            for (j = i + 1; j < DATASEC_NUM; j++) {
                ASSERT(datasec_start[i] >= datasec_end[j] ||
                       datasec_start[j] >= datasec_end[i]);
            }
        }
    });
}

static void
data_section_exit(void)
{
    uint i;
    DOSTATS({
        /* There can't have been that many races.
         * A failure to re-protect should result in a ton of d_r_dispatch
         * entrances w/ .data unprot, so should show up here.
         * However, an app with threads that are initializing in DR and thus
         * unprotected .data while other threads are running new code (such as
         * on attach) can easily rack up hundreds of unprot cache entrances.
         */
        ASSERT_CURIOSITY(GLOBAL_STAT(datasec_not_prot) < 5000);
    });
    for (i = 0; i < DATASEC_NUM; i++)
        DELETE_LOCK(datasec_lock[i]);
}

#define DATASEC_WRITABLE_MOD(which, op)                 \
    ((which) == DATASEC_RARELY_PROT                     \
         ? (datasec_writable_rareprot op)               \
         : ((which) == DATASEC_CXTSW_PROT               \
                ? (datasec_writable_cxtswprot op)       \
                : ((which) == DATASEC_FREQ_PROT         \
                       ? (datasec_writable_freqprot op) \
                       : (ASSERT_NOT_REACHED(), datasec_writable_neverprot))))

/* WARNING: any DO_ONCE will call this routine, so don't call anything here
 * that has a DO_ONCE, to avoid deadlock!
 */
void
protect_data_section(uint sec, bool writable)
{
    ASSERT(sec >= 0 && sec < DATASEC_NUM);
    ASSERT(TEST(DATASEC_SELFPROT[sec], dynamo_options.protect_mask));
    /* We can be called very early before data_section_init() so init here
     * (data_section_init() has no dependences).
     */
    if (datasec_start[sec] == NULL) {
        /* should only happen early in init */
        ASSERT(!dynamo_initialized);
        data_section_init();
    }
    d_r_mutex_lock(&datasec_lock[sec]);
    ASSERT(datasec_start[sec] != NULL);
    /* if using libc, we cannot print while data segment is read-only!
     * thus, if making it writable, do that first, otherwise do it last.
     * w/ ntdll this is not a problem.
     */
    /* Remember that multiple threads can be doing (unprotect,protect) pairs of
     * calls simultaneously.  The datasec_lock makes each individual call atomic,
     * and if all calls are properly nested, our use of counters should result in
     * the proper protection only after the final protect call and not in the
     * middle of some other thread's writes to the data section.
     */
    if (writable) {
        /* On-context-switch protection has a separate mechanism for
         * only protecting when the final thread leaves DR
         */
        ASSERT_CURIOSITY(DATASEC_WRITABLE(sec) <= 2); /* shouldn't nest too deep! */
        if (DATASEC_WRITABLE(sec) == 0) {
            make_writable(datasec_start[sec], datasec_end[sec] - datasec_start[sec]);
            STATS_INC(datasec_prot_changes);
        } else
            STATS_INC(datasec_prot_wasted_calls);
        (void)DATASEC_WRITABLE_MOD(sec, ++);
    }
    LOG(TEST(DATASEC_SELFPROT[sec], SELFPROT_ON_CXT_SWITCH) ? THREAD_GET : GLOBAL,
        LOG_VMAREAS, TEST(DATASEC_SELFPROT[sec], SELFPROT_ON_CXT_SWITCH) ? 3U : 2U,
        "protect_data_section: thread " TIDFMT " %s (recur %d, stat %d) %s %s %d\n",
        d_r_get_thread_id(), DATASEC_WRITABLE(sec) == 1 ? "changing" : "nop",
        DATASEC_WRITABLE(sec), GLOBAL_STAT(datasec_not_prot), DATASEC_NAMES[sec],
        writable ? "rw" : "r", DATASEC_WRITABLE(sec));
    if (!writable) {
        ASSERT(DATASEC_WRITABLE(sec) > 0);
        (void)DATASEC_WRITABLE_MOD(sec, --);
        if (DATASEC_WRITABLE(sec) == 0) {
            make_unwritable(datasec_start[sec], datasec_end[sec] - datasec_start[sec]);
            STATS_INC(datasec_prot_changes);
        } else
            STATS_INC(datasec_prot_wasted_calls);
    }
    d_r_mutex_unlock(&datasec_lock[sec]);
}

/* enter/exit DR hooks */
void
entering_dynamorio(void)
{
    if (SELF_PROTECT_ON_CXT_SWITCH)
        dynamorio_unprotect();
    ASSERT(HOOK_ENABLED);
    LOG(GLOBAL, LOG_DISPATCH, 3, "entering_dynamorio thread=" TIDFMT "\n",
        d_r_get_thread_id());
    STATS_INC(num_entering_DR);
    if (INTERNAL_OPTION(single_thread_in_DR)) {
        acquire_recursive_lock(&thread_in_DR_exclusion);
        LOG(GLOBAL, LOG_DISPATCH, 3, "entering_dynamorio thread=" TIDFMT " count=%d\n",
            d_r_get_thread_id(), thread_in_DR_exclusion.count);
    }
}

void
exiting_dynamorio(void)
{
    ASSERT(HOOK_ENABLED);
    LOG(GLOBAL, LOG_DISPATCH, 3, "exiting_dynamorio thread=" TIDFMT "\n",
        d_r_get_thread_id());
    STATS_INC(num_exiting_DR);
    if (INTERNAL_OPTION(single_thread_in_DR)) {
        /* thread init/exit can proceed now */
        LOG(GLOBAL, LOG_DISPATCH, 3, "exiting_dynamorio thread=" TIDFMT " count=%d\n",
            d_r_get_thread_id(), thread_in_DR_exclusion.count - 1);
        release_recursive_lock(&thread_in_DR_exclusion);
    }
    if (SELF_PROTECT_ON_CXT_SWITCH && !dynamo_exited)
        dynamorio_protect();
}

/* Note this includes any stack guard pages */
bool
is_on_initstack(byte *esp)
{
    return (esp <= d_r_initstack && esp > d_r_initstack - DYNAMORIO_STACK_SIZE);
}

/* Note this includes any stack guard pages */
bool
is_on_dstack(dcontext_t *dcontext, byte *esp)
{
    return (esp <= dcontext->dstack && esp > dcontext->dstack - DYNAMORIO_STACK_SIZE);
}

bool
is_currently_on_dstack(dcontext_t *dcontext)
{
    byte *cur_esp;
    GET_STACK_PTR(cur_esp);
    return is_on_dstack(dcontext, cur_esp);
}

void
pre_second_thread(void)
{
    /* i#1111: nop-out bb_building_lock until 2nd thread created.
     * While normally we'll call this in the primary thread while not holding
     * the lock, it's possible on Windows for an externally injected thread
     * (or for a thread sneakily created by some native_exec code w/o going
     * through ntdll wrappers) to appear.  We solve the problem of the main
     * thread currently holding bb_building_lock and us turning its
     * unlock into an error by the bb_lock_would_have bool in
     * SHARED_BB_UNLOCK().
     */
    if (!bb_lock_start) {
        d_r_mutex_lock(&bb_building_lock);
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        bb_lock_start = true;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        d_r_mutex_unlock(&bb_building_lock);
    }
}
