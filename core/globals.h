/* **********************************************************
 * Copyright (c) 2011-2023 Google, Inc.  All rights reserved.
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
 * globals.h - global defines and typedefs, included in all files
 */

#ifndef _GLOBALS_H_
#define _GLOBALS_H_ 1

#include "configure.h"

#ifdef WINDOWS
/* Vista SDK compiler default is to set NTDDI_VERSION to NTDDI_LONGHORN, causing
 * problems w/ new Vista-only flags like in PROCESS_ALL_ACCESS in win32/injector.c.
 * The problematic flag there, PROCESS_QUERY_LIMITED_INFORMATION, is a subset of
 * PROCESS_QUERY_INFORMATION, so we don't lose anything by not asking for it on Vista.
 * But if they add new non-subset flags in the future we'd need dynamic dispatch,
 * as earlier Windows versions give access denied on unknown flags!
 */
#    define _WIN32_WINNT _WIN32_WINNT_NT4 /* ==0x0400; NTDDI_VERSION is set from this */

#    define WIN32_LEAN_AND_MEAN
/* Exclude rarely-used stuff from Windows headers */

/* XXX Case 1167: work in progress: bumping up warning level to 4 */
#    pragma warning(disable : 4054) // from function pointer to data pointer
#    pragma warning(disable : 4100) // 'envp' : unreferenced formal parameter
#    pragma warning(disable : 4127) // conditional expr is constant (majority of warnings)
#    pragma warning(disable : 4189) // local variable is initialized but not referenced
#    pragma warning( \
        disable : 4204) // nonstd extension: non-constant aggregate initializer
#    pragma warning(disable : 4210) // nonstd extension: function given file scope
#    pragma warning(disable : 4505) // unreferenced local function has been removed
#    pragma warning( \
        disable : 4702) // unreachable code (DEBUG=0, for INTERNAL_OPTION test)
#    pragma warning(disable : 4324) // structure was padded due to __declspec(align())
#    pragma warning(disable : 4709) // comma operator within array index expression
#    pragma warning(disable : 4214) // nonstd extension: bit field types other than int

/**************************************************/
/* warnings on compiling with VC 8.0, all on VC or PlatformSDK header files */

/* shows up in buildtools/VC/8.0/dist/VC/include/vadefs.h
 * supposed to include identifier in the pop pragma and they don't
 */
#    pragma warning( \
        disable : 4159) // #pragma pack has popped previously pushed identifier

/* FIXME case 191729: this is coming from our own code.  We could
 * switch to the _s versions when on Windows.
 */
#    pragma warning(disable : 4996) //'sscanf' was declared deprecated

/**************************************************/

#endif

#include "globals_shared.h"

/* currently we always export statistics structure */
#define DYNAMORIO_STATS_EXPORTS 1

#ifdef WINDOWS
#    define DYNAMORIO_EXPORT __declspec(dllexport)
#elif defined(USE_VISIBILITY_ATTRIBUTES)
/* PR 262804: we use "protected" instead of "default" to ensure our
 * own uses won't be preempted.  Note that for DR_APP_API in
 * lib/dr_app.h we get a link error trying to use "protected": but we
 * don't use dr_app_* internally anyway, so leaving as default.
 */
#    define DYNAMORIO_EXPORT __attribute__((visibility("protected")))
#else
/* visibility attribute not available in gcc < 3.3 (we use linker script) */
#    define DYNAMORIO_EXPORT
#endif

/* We always export nowadays. */
#define DR_API DYNAMORIO_EXPORT
#if (defined(DEBUG) && defined(BUILD_TESTS)) || defined(UNSUPPORTED_API)
#    define DR_UNS_EXCEPT_TESTS_API DR_API
#else
#    define DR_UNS_EXCEPT_TESTS_API /* nothing */
#endif
#ifdef UNSUPPORTED_API
/* TODO i#4045: Remove unsupported API support.  After i#3092's header refactoring,
 * we can't just change a define and export these anymore anyway: they would have
 * to be moved to the _api.h public headers.
 */
#    define DR_UNS_API DR_API
#else
#    define DR_UNS_API /* nothing */
#endif

#ifdef WINDOWS
#    define DISABLE_NULL_SANITIZER
#else
/* As per https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005fattribute.html,
 * we need to first check whether __has_attribute is defined, to ensure
 * portability. We saw issues in the Android-ARM cross compile without this.
 */
#    if defined __has_attribute
#        if __has_attribute(__no_sanitize__) && defined(HAVE_FNOSANITIZE_NULL)
/* The null sanitizer adds is-null checks for pointer dereferences. As part
 * of this, it stores and retrieves pointers from the stack frame. Each
 * pointer dereference uses a different stack location. So, if there are too
 * many pointer dereferences in a function (like dump_global_stats and
 * dump_thread_stats) it will increase the size of the stack frame a lot
 * (127KB and 147KB respectively for the mentioned examples) when the null
 * sanitizer is enabled. The following macro lets us disable this check for
 * methods annotated with it.
 */
#            define DISABLE_NULL_SANITIZER __attribute__((no_sanitize("null")))
#        endif
#    endif
#    ifndef DISABLE_NULL_SANITIZER
#        define DISABLE_NULL_SANITIZER
#    endif
#endif

#define INLINE_ONCE inline

#include <stdlib.h>
#include <stdio.h>

/* N.B.: some of these typedefs and defines are duplicated in
 * lib/globals_shared.h!
 */

#ifdef WINDOWS
#    include <windows.h>
typedef unsigned long ulong;
typedef unsigned short ushort;
/* We can't put this in globals_shared.h b/c it needs windows.h and
 * not all users of globals_shared.h want that included, so we
 * duplicate it here (we do have Linux file_t stuff in globals_shared.h)
 */
/* since a FILE cannot be used outside of the DLL it was created in,
 * we have to use HANDLE on Windows
 * we hide the distinction behind the file_t type
 */
typedef HANDLE file_t;
#    define INVALID_FILE INVALID_HANDLE_VALUE
#    define STDOUT (file_t) get_stdout_handle()
#    define STDERR (file_t) get_stderr_handle()
#    define STDIN (file_t) get_stdin_handle()
#    define DIRSEP '\\'
#    define ALT_DIRSEP '/'

#else /* UNIX */
/* uint, ushort, and ulong are in types.h */
#    if defined(MACOS) || defined(ANDROID)
typedef unsigned long ulong;
#    endif
#    include <sys/types.h> /* for wait */
#    define DIRSEP '/'
#    define ALT_DIRSEP DIRSEP
#endif

/* FIXME: what is range of thread_id_t on linux and on win32?
 * linux routines use -1 as sentinel, right?
 * on win32, are ids only 16 bits?
 * if so, change thread_id_t to be a signed int and use -1?
 * For now, based on observation, no process on linux and no thread on windows
 * has id 0 (on windows even a new thread in its init apc has a non-0 id)
 */
#define INVALID_THREAD_ID 0

typedef unsigned char uchar;
typedef byte *cache_pc; /* fragment cache pc */

#define SUCCESS 0
#define FAILURE 1

/* macros to make conditional compilation look prettier */
#ifdef DGC_DIAGNOSTICS
#    define _IF_DGCDIAG(x) , x
#    define IF_DGCDIAG_ELSE(x, y) x
#else
#    define _IF_DGCDIAG(x)
#    define IF_DGCDIAG_ELSE(x, y) y
#endif

/* make sure defines are consistent */
#if !defined(X86) && !defined(ARM) && !defined(AARCH64) && !defined(RISCV64)
#    error Must define X86, ARM, AARCH64 or RISCV64: no other platforms are supported
#endif

#if defined(PAPI) && defined(WINDOWS)
#    error PAPI does not work on WINDOWS
#endif

#ifdef DGC_DIAGNOSTICS
#    ifndef PROGRAM_SHEPHERDING
#        error DGC_DIAGNOSTICS requires PROGRAM_SHEPHERDING
#    endif
#    ifndef DEBUG
#        error DGC_DIAGNOSTICS requires DEBUG
#    endif
#endif

#ifndef PROGRAM_SHEPHERDING
#    ifdef SIMULATE_ATTACK
#        error SIMULATE_ATTACK requires PROGRAM_SHEPHERDING
#    endif
#endif

/* in buildmark.c */
extern const char dynamorio_version_string[];
extern const char dynamorio_buildmark[];

struct _instrlist_t;
struct _fragment_t;
typedef struct _fragment_t fragment_t;
struct _future_fragment_t;
typedef struct _future_fragment_t future_fragment_t;
struct _trace_t;
typedef struct _trace_t trace_t;
struct _linkstub_t;
typedef struct _linkstub_t linkstub_t;
struct _dcontext_t;
typedef struct _dcontext_t dcontext_t;
struct vm_area_vector_t;
typedef struct vm_area_vector_t vm_area_vector_t;
struct _coarse_info_t;
typedef struct _coarse_info_t coarse_info_t;
struct _coarse_freeze_info_t;
typedef struct _coarse_freeze_info_t coarse_freeze_info_t;
struct _module_data_t;

#if defined(RETURN_AFTER_CALL) || defined(RCT_IND_BRANCH)
struct _rct_module_table_t;
typedef struct _rct_module_table_t rct_module_table_t;

typedef enum {
    RCT_RAC = 0,
    RCT_RCT,
    RCT_NUM_TYPES,
} rct_type_t;
#endif

typedef struct _thread_record_t {
    thread_id_t id; /* thread id */
#ifdef WINDOWS
    HANDLE handle; /* win32 thread handle */
    bool retakeover;
#else
    process_id_t pid; /* thread group id */
    bool execve;      /* exiting due to execve (i#237/PR 498284) */
#endif
    uint num;                  /* creation ordinal */
    bool under_dynamo_control; /* used for deciding whether to intercept events */
    dcontext_t *dcontext;      /* allows other threads to see this thread's context */
    struct _thread_record_t *next;
} thread_record_t;

/* We don't include dr_api.h, that's for external use. */
#ifdef DR_APP_EXPORTS
/* we only export app interface if DR_APP_EXPORTS is defined */
#    include "dr_app.h"
/* a few always-exported routines are part of the app interface */
#    undef DYNAMORIO_EXPORT
#    define DYNAMORIO_EXPORT DR_APP_API
#endif

/* AArch64 Scalable Vector Extension's vector length in bits. This depends on
 * the hardware implementation and can be one of:
 * 128 256 384 512 640 768 896 1024 1152 1280 1408 1536 1664 1792 1920 2048
 * See https://developer.arm.com/documentation/102476/0100/Introducing-SVE
 * This variable stores the length for off-line decoding.
 */
extern int sve_veclen;

#include "heap.h"
#include "options_struct.h"
#include "utils.h"
#include "options.h"
#include "os_exports.h"
#include "arch_exports.h"
#include "drlibc.h"
#include "vmareas.h"
#include "instrlist.h"
#include "dispatch.h"

#include "dr_stats.h"

/* did the client request a premature exit at a potentially awkward spot
 * (nudge handler, signal handler)?
 */
extern bool client_requested_exit;

typedef struct _client_to_do_list_t {
    /* used to make a list of fragments to delete/replace */
    /* deletes frag at tag if ilist is null else replaces it with ilist */
    instrlist_t *ilist;
    app_pc tag;
    struct _client_to_do_list_t *next;
} client_todo_list_t;

/* Clients need a separate list to queue up flush requests from dr_flush() */
typedef struct _client_flush_req_t {
    app_pc start;
    size_t size;
    uint flush_id; /* client supplied identifier for this flush */
    void (*flush_callback)(int);
    struct _client_flush_req_t *next;
} client_flush_req_t;

/* for -thin_client we don't allocate client_data currently, also client_data could be
 * NULL during thread startup or teardown (i.e. mutex_wait_contended_lock() usage) */
#define IS_CLIENT_THREAD(dcontext)                        \
    ((dcontext) != NULL && dcontext != GLOBAL_DCONTEXT && \
     (dcontext)->client_data != NULL && (dcontext)->client_data->is_client_thread)
#ifdef DEBUG
/* For use after dcontext->client_data is NULL */
#    define IS_CLIENT_THREAD_EXITING(dcontext)                \
        ((dcontext) != NULL && dcontext != GLOBAL_DCONTEXT && \
         (dcontext)->is_client_thread_exiting)
#endif

/* Client interface-specific data for dcontexts */
typedef struct _client_data_t {
    /* field for use by user via exported API */
    void *user_field;
    client_todo_list_t *to_do;
    client_flush_req_t *flush_list;
    mutex_t sideline_mutex;
    /* fields for doing release and debug build checks against erroneous API usage */
    module_data_t *no_delete_mod_data;

    /* Client-owned threads, such as a client nudge thread, require special
     * synchronization support. is_client_thread means that the thread is currently
     * completely owned by the client.  client_thread_safe_for_sync is use to mark
     * client-owned threads that are safe for synch_with_all_threads synchronization but
     * are in dynamo/native code (such as in dr_thread_yield(), dr_sleep(),
     * dr_mutex_lock() and dr_messagebox()).  Note it does not need to be set when
     * the client is in client library code.  For dr_mutex_lock() we set client_grab_mutex
     * to the client mutex that is being locked so that we can set
     * client_thread_safe_for_sync only around the actual wait.
     * FIXME - PR 231301, we may need a way for clients that call ntdll directly to
     * mark client_thread_safe_for_synch for client-owned threads when calling out to
     * ntdll. Especially if they're calling system calls that wait or take a long time
     * to finish etc. Applies to generated code and other libraries called by the client
     * lib as well.
     */
    bool is_client_thread; /* NOTE - use IS_CLIENT_THREAD() */
    bool client_thread_safe_for_synch;
    /* i#1420: we add a field to indicate if we are in a safe syscall spot
     * for THREAD_SYNCH_TERMINATED_AND_CLEANED.
     */
    bool at_safe_to_terminate_syscall;
    bool suspendable;      /* suspend w/ synchall: PR 609569 */
    bool left_unsuspended; /* not suspended by synchall: PR 609569 */
    uint mutex_count;      /* mutex nesting: for PR 558463 */
    void *client_grab_mutex;
#ifdef DEBUG
    bool is_translating;
#endif
#ifdef LINUX
    /* i#4041: Pass the real translation for signals in rseq sequences. */
    app_pc last_special_xl8;
#endif

    /* flags for asserts on linux and for getting param base right on windows */
    bool in_pre_syscall;
    bool in_post_syscall;
    /* flag for dr_syscall_invoke_another() */
    bool invoke_another_syscall;
    /* flags for dr_get_mcontext (i#117/PR 395156) */
    bool mcontext_in_dcontext;
    bool suspended;
    /* 2 other ways to point at a context for dr_{g,s}et_mcontext() */
    priv_mcontext_t *cur_mc;
    os_cxt_ptr_t os_cxt;

    /* The error code of last failed API routine. Not updated on successful API calls
     * but only upon failures.
     */
    dr_error_code_t error_code;
} client_data_t;

#ifdef UNIX
/* i#61/PR 211530: nudges on Linux do not use separate threads */
typedef struct _pending_nudge_t {
    nudge_arg_t arg;
    struct _pending_nudge_t *next;
} pending_nudge_t;
#endif

/* size of each Dynamo thread-private stack */
#define DYNAMORIO_STACK_SIZE dynamo_options.stack_size

/* global flags */
extern bool automatic_startup;       /* ignore start/stop api, run entire program */
extern bool control_all_threads;     /* ok for "weird" things to happen -- not all
                                        threads are under our control */
extern bool dynamo_heap_initialized; /* has dynamo_heap been initialized? */
extern bool dynamo_initialized;      /* has dynamo been initialized? */
extern bool dynamo_started;          /* has DR initiated takeover of the app? */
extern bool dynamo_exited;           /* has dynamo exited? */
extern bool dynamo_exited_all_other_threads; /* has dynamo exited and synched? */
extern bool dynamo_exited_and_cleaned;       /* has dynamo component cleanup started? */
#ifdef DEBUG
extern bool dynamo_exited_log_and_stats; /* are stats and logfile shut down? */
#endif
extern bool dynamo_resetting;           /* in middle of global reset? */
extern bool dynamo_all_threads_synched; /* are all other threads suspended safely? */
/* This indicates mostly whether there might be other threads in the process when
 * DR initializes, which equates to attaching (vs being launched by DR) on Linux.
 * On Windows though we can't really tell the difference due to our late injection,
 * so this is always set on Windows.
 */
extern bool dynamo_control_via_attach;
/* Not guarded by DR_APP_EXPORTS because later detach implementations might not
 * go through the app interface.
 */
extern bool doing_detach;
extern thread_id_t detacher_tid;

extern event_t dr_app_started;
extern event_t dr_attach_finished;

extern bool standalone_library; /* used as standalone library */
#ifdef UNIX
extern bool post_execve; /* have we performed an execve? */
/* i#237/PR 498284: vfork threads that execve need to be separately delay-freed */
extern int num_execve_threads;
#endif

/* global instance of statistics struct */
extern dr_statistics_t *d_r_stats;

/* the process-wide logfile */
extern file_t main_logfile;

/* initial stack so we don't have to use app's */
extern byte *d_r_initstack;
extern mutex_t initstack_mutex;
extern byte *initstack_app_xsp;

#ifdef WINDOWS
/* PR203701: separate stack for error reporting when the dstack is exhausted */
extern byte *exception_stack;
#endif

/* keeps track of how many threads are in cleanup_and_terminate so that we know
 * if any threads could still be using shared resources even if they aren't on
 * the all_threads list */
extern volatile int exiting_thread_count;
/* Tracks newly created threads not yet on the all_threads list. */
extern volatile int uninit_thread_count;

/* Called before a second thread is ever scheduled. */
void
pre_second_thread(void);

bool
is_on_initstack(byte *esp);

bool
is_on_dstack(dcontext_t *dcontext, byte *esp);
bool
is_currently_on_dstack(dcontext_t *dcontext);

#ifdef WINDOWS
extern bool dr_early_injected;
extern int dr_early_injected_location;
extern bool dr_earliest_injected;
extern bool dr_injected_primary_thread;
extern bool dr_injected_secondary_thread;
extern bool dr_late_injected_primary_thread;

#endif
#ifdef RETURN_AFTER_CALL
extern bool dr_preinjected;
#endif
/* flags to indicate when DR is being initialized / exited using the API */
extern bool dr_api_entry;
extern bool dr_api_exit;

/* in dynamo.c */
/* 12-bit addressed hash table takes up 16K, has capacity of 4096.
 * XXX: We currently never resize, assuming won't be seeing more than
 * a few thousand threads: it should be simple to swap for a resizing
 * table using generic_table_t though.
 */
#define ALL_THREADS_HASH_BITS 12
extern thread_record_t **all_threads;
extern mutex_t all_threads_lock;
DYNAMORIO_EXPORT int
dynamorio_app_init(void);
/* dynamorio_app_init() can be called in two parts: */
void
dynamorio_app_init_part_one_options();
int
dynamorio_app_init_part_two_finalize();
int
dynamorio_app_exit(void);
dcontext_t *
standalone_init(void);
void
standalone_exit(void);
thread_record_t *
thread_lookup(thread_id_t tid);
void
add_thread(IF_WINDOWS_ELSE_NP(HANDLE hthread, process_id_t pid), thread_id_t tid,
           bool under_dynamo_control, dcontext_t *dcontext);
bool
remove_thread(IF_WINDOWS_(HANDLE hthread) thread_id_t tid);
uint
get_thread_num(thread_id_t tid);
int
d_r_get_num_threads(void);
bool
is_last_app_thread(void);
void
get_list_of_threads(thread_record_t ***list, int *num);
bool
is_thread_known(thread_id_t tid);
#ifdef UNIX
void
get_list_of_threads_ex(thread_record_t ***list, int *num, bool include_execve);
void
mark_thread_execve(thread_record_t *tr, bool execve);
#endif
bool
is_thread_initialized(void);
int
dynamo_thread_init(byte *dstack_in, priv_mcontext_t *mc, void *os_data,
                   bool client_thread);
int
dynamo_thread_exit(void);
void
dynamo_thread_stack_free_and_exit(byte *stack);
int
dynamo_other_thread_exit(thread_record_t *tr _IF_WINDOWS(bool detach_stacked_callbacks));
void
dynamo_thread_under_dynamo(dcontext_t *dcontext);
void
dynamo_thread_not_under_dynamo(dcontext_t *dcontext);
/* used for synch to prevent thread creation/deletion in critical periods */
extern mutex_t thread_initexit_lock;
dcontext_t *
create_new_dynamo_context(bool initial, byte *dstack_in, priv_mcontext_t *mc);
void
initialize_dynamo_context(dcontext_t *dcontext);
dcontext_t *
create_callback_dcontext(dcontext_t *old_dcontext);
int
dynamo_nullcalls_exit(void);
int
dynamo_process_exit(void);
#ifdef UNIX
void
dynamorio_fork_init(dcontext_t *dcontext);
#endif
/* This calls dynamo_thread_under_dynamo() for the current thread as well. */
void
dynamorio_take_over_threads(dcontext_t *dcontext);
dr_statistics_t *
get_dr_stats(void);

/* functions needed by detach */
int
dynamo_shared_exit(thread_record_t *toexit _IF_WINDOWS(bool detach_stacked_callbacks));
/* perform exit tasks that require full thread data structs */
void
dynamo_process_exit_with_thread_info(void);
/* thread cleanup prior to clean exit event */
void
dynamo_thread_exit_pre_client(dcontext_t *dcontext, thread_id_t id);
void
dynamo_exit_post_detach(void);

/* enter/exit DR hooks */
void
entering_dynamorio(void);
void
exiting_dynamorio(void);

void
handle_system_call(dcontext_t *dcontext);

/* self-protection */
void
protect_data_section(uint sec, bool writable);
#ifdef DEBUG
const char *
get_data_section_name(app_pc pc);
bool
check_should_be_protected(uint sec);
#    ifdef WINDOWS
bool
data_sections_enclose_region(app_pc start, app_pc end);
#    endif
#endif /* DEBUG */

/* all the locks used to protect shared data structures during multi-operation
 * sequences, exported so that micro-operations can assert that one is held
 */
extern mutex_t bb_building_lock;
extern volatile bool bb_lock_start;
extern recursive_lock_t change_linking_lock;

/* make args easier to read for protection change calls
 * since only two possibilities not using new type
 */
enum { READONLY = false, WRITABLE = true };

/* Values for unprotected_context_t.exit_reason, stored in a ushort. */
enum {
    /* Default.  All other reasons must clear after setting. */
    EXIT_REASON_SELFMOD = 0,
    /* Floating-point state PC needs updating (i#698). */
    EXIT_REASON_FLOAT_PC_FNSAVE,
    EXIT_REASON_FLOAT_PC_FXSAVE,
    EXIT_REASON_FLOAT_PC_FXSAVE64,
    EXIT_REASON_FLOAT_PC_XSAVE,
    EXIT_REASON_FLOAT_PC_XSAVE64,
    /* Additional types of system call gateways. */
    EXIT_REASON_NI_SYSCALL_INT_0x81,
    EXIT_REASON_NI_SYSCALL_INT_0x82,
    /* Single step exception needs to be forged. */
    EXIT_REASON_SINGLE_STEP,
    /* We need to raise a kernel xfer event on an rseq-native abort. */
    EXIT_REASON_RSEQ_ABORT,
};

/* Number of nested calls into native modules that we support.  This number
 * needs to equal the number of stubs in x86.asm:back_from_native_retstubs,
 * which is checked at startup in native_exec.c.
 * FIXME: Remove this limitation if we ever need to support true mutual
 * recursion between native and non-native modules.
 */
enum { MAX_NATIVE_RETSTACK = 10 };

typedef struct _retaddr_and_retloc_t {
    app_pc retaddr;
    app_pc retloc;
} retaddr_and_retloc_t;

/* To handle TRY/EXCEPT/FINALLY setjmp */
typedef struct try_except_context_t {
    /* FIXME: we are using a local dr_jmp_buf which is relatively
     * small so minimal risk of dstack pressure.  Alternatively, we
     * can disallow nesting and have a single buffer per dcontext.
     */
    /* N.B.: offsetof(try_except_context_t, context) is hardcoded in asm_defines.asm */
    dr_jmp_buf_t context;

    struct try_except_context_t *prev_context;
} try_except_context_t;

/* We do support TRY pre-dynamo_initialized via this global struct.
 * This, along with safe_read pc ranges, satisfies most TRY uses that
 * don't have a dcontext (i#350).
 */
typedef struct _try_except_t {
    try_except_context_t *try_except_state; /* for TRY/EXCEPT/FINALLY */
    bool unwinding_exception;               /* NYI support for TRY/FINALLY -
                                             * marks exception until an EXCEPT handles */
} try_except_t;

extern try_except_t global_try_except;
#ifdef UNIX
extern thread_id_t global_try_tid;
#endif

typedef struct {
    /* WARNING: if you change the offsets of any of these fields,
     * you must also change the offsets in <arch>/<arch.asm>
     */
    priv_mcontext_t mcontext; /* real machine context (in globals_shared.h + mcxtx.h) */
#ifdef UNIX
    int dr_errno; /* errno used for DR (no longer used for app) */
#endif
    bool at_syscall;    /* for shared deletion syscalls_synch_flush,
                         * as well as syscalls handled from d_r_dispatch,
                         * and for reset to identify when at syscalls
                         */
    ushort exit_reason; /* Allows multiplexing LINK_SPECIAL_EXIT */
    /* Above fields are padded to 8 bytes on all archs except Win x86-32. */

    /* Spill slots for inlined clean calls. */
    reg_t inline_spill_slots[CLEANCALL_NUM_INLINE_SLOTS];
} unprotected_context_t;

/* dynamo-specific context associated with each active app thread
 * N.B.: make sure to update these routines as necessary if
 * you add or remove fields:
 *   create_new_dynamo_context
 *   create_callback_dcontext
 *   initialize_dynamo_context
 *   swap_dcontexts
 * if you add any pointers to data structures, make sure callback_setup()
 *   clears them to prevent stale pointers on callback return
 */
struct _dcontext_t {
    /* NOTE: For any field to survive across callback stack switches it must either
     * be indirected through a modular field or explicitly copied in
     * create_callback_dcontext() (like the modular fields are).
     */

    /* WARNING: if you change the offsets of any of these fields, up through
     * ignore_enterexit, you must also change the offsets in <arch>/<arch.s>
     */

    /* if SELFPROT_DCONTEXT, must split dcontext into unprotected and
     * protected fields depending on whether they must be read-only
     * when in the code cache.
     * we waste sizeof(unprotected_context_t) bytes to provide runtime flexibility:
     */
    union {
        /* we use separate_upcontext if
         *    (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
         * else we use the inlined upcontext
         */
        unprotected_context_t *separate_upcontext;
        unprotected_context_t upcontext;
    } upcontext;
    /* HACK for x86.asm lack of runtime param access: this is either
     * a self-ptr (to inlined upcontext) or if we separate upcontext it points there.
     */
    unprotected_context_t *upcontext_ptr;

    /* The next application pc to execute.
     * Also used to store the cache pc to execute when entering the code cache,
     * and set to the sentinel value BACK_TO_NATIVE_AFTER_SYSCALL for native_exec.
     * FIXME: change to a union?
     */
    app_pc next_tag;

    linkstub_t *last_exit; /* last exit from cache */
    byte *dstack;          /* thread-private dynamo stack */

    bool is_exiting; /* flag for exiting thread */
#ifdef WINDOWS
    /* i#249: TEB field isolation */
    int app_errno;
    void *app_fls_data;
    void *priv_fls_data;
    void *app_nt_rpc;
    void *priv_nt_rpc;
    void *app_nls_cache;
    void *priv_nls_cache;
    void *app_static_tls;
    void *priv_static_tls;
    void *app_stack_limit;
    void *app_stack_base;
    /* we need this to restore ptrs for other threads on detach */
    byte *teb_base;
    /* storage for an extra app value around sysenter system calls for the
     * case 5441 Sygate interoperability hack */
    /* FIXME - this needs to be moved into the upcontext as is written to
     * in cache by ignore/shared_syscall, ramifications? */
    app_pc sysenter_storage;

    /* used to avoid enter/exit hooks for certain system calls (see case 4942) */
    bool ignore_enterexit;
#endif

    /* Coarse-grain cache exits require extra state storage as they do not
     * use per-exit separate data structures
     */
    union {
        /* Indirect branches store on exit the source tag (with the type of
         * branch coming from fake linkstubs), while direct store the source unit
         */
        app_pc src_tag;
        coarse_info_t *dir_exit;
    } coarse_exit;

    dr_where_am_i_t whereami; /* where control is at the moment */
#ifdef UNIX
    /* On ARM based machines, char is unsigned by default.
     * https://www.arm.linux.org.uk/docs/faqs/signedchar.php
     * But we need _signed_ char, so we use sbyte which explicitly qualifies
     * as that.
     */
    sbyte signals_pending; /* != 0: pending; < 0: currently handling one */
#endif

    /************* end of offset-crucial fields *********************/

    /* FIXME: now that we initialize a new thread's dcontext right away, and
     * a new callback's as well, we should be able to get rid of this
     */
    bool initialized; /* has this context been used yet? */
    thread_id_t owning_thread;
#ifdef UNIX
    process_id_t owning_process; /* handle shared address space w/o shared pid */
#endif
#ifdef MACOS
    uint thread_port; /* mach_port_t */
#endif
    thread_record_t *thread_record; /* so don't have to do a thread_lookup */
    void *allocated_start;          /* used for cache alignment */
    fragment_t *last_fragment;      /* cached value of linkstub_fragment(last_exit) */

    int sys_num; /* holds normalized syscall number */
#ifdef WINDOWS
    reg_t *sys_param_base; /* used for post_system_call */
#endif
#if defined(UNIX) || defined(X64)
    reg_t sys_param0; /* used for post_system_call */
    reg_t sys_param1; /* used for post_system_call */
    reg_t sys_param2; /* used for post_system_call */
    reg_t sys_param3; /* used for post_system_call */
#endif
#ifdef UNIX
    reg_t sys_param4; /* used for post_system_call i#173 and i#2759 */
    bool sys_was_int; /* was the last system call via do_int_syscall? */
    bool sys_xbp;     /* PR 313715: store orig xbp */
#    ifdef DEBUG
    bool mprot_multi_areas; /* PR 410921: mprotect of 2 or more vmareas? */
#    endif
#endif
#ifdef MACOS
    reg_t app_xdx; /* stores orig xdx during sysenter */
#endif

    /* Holds the ISA mode of the thread.
     * For x86, default is to decode as base platform, but we want runtime ability to
     * decode/encode 32-bit from 64-bit dynamorio.dll (we don't support the
     * other way around: PR 236203).  For ARM we must support swapping.
     */
    dr_isa_mode_t isa_mode;
#ifdef ARM
    /* Extra state (e.g., IT block state) used for decode/encode
     * The actual type is not uint, we use them for better abstraction
     * and better performace (avoiding a void * with separate allocation).
     */
    uint encode_state[2]; /* encode_state_t in arm/decode_private.h */
    uint decode_state[2]; /* decode_state_t in arm/decode_private.h */
#endif

    /* to make things more modular these are void*: */
    void *link_field;
    void *monitor_field;
    void *fcache_field;
    void *fragment_field;
    void *heap_field;
    void *vm_areas_field;
    void *os_field;
    void *synch_field;
#ifdef UNIX
    void *signal_field;
    void *pcprofile_field;
#endif
    void *private_code; /* various thread-private routines */

#ifdef TRACE_HEAD_CACHE_INCR
    cache_pc trace_head_pc; /* HACK to jmp to trace head w/o a prefix */
#endif

#ifdef WINDOWS
    /* these fields used for "stack" of contexts for callbacks */
    dcontext_t *prev_unused;
    /* need to be able to tell which dcontexts in callback stack are valid */
    bool valid;
    /* special slot used to deal with callback returns, where we want to
     * restore state of prior guy on callback stack, yet need current state
     * to restore native state prior to callback return interrupt/syscall.
     * Also used as temp next_tag slot for fcache_enter_indirect and cb ret.
     */
    reg_t nonswapped_scratch;
#endif

    /* next_tag holds the do_syscall entry point, so we need another
     * slot to hold asynch targets for APCs to know next target and
     * for NtContinue and sigreturn to set next target
     */
    app_pc asynch_target;

    /* must store post-intercepted-syscall target to allow using normal
     * d_r_dispatch() for native_exec syscalls
     */
    app_pc native_exec_postsyscall;

    /* Stack of app return addresses and stack locations of callsites where we
     * called into a native module.
     */
    retaddr_and_retloc_t native_retstack[MAX_NATIVE_RETSTACK];
    uint native_retstack_cur;

#ifdef PROGRAM_SHEPHERDING
    bool alloc_no_reserve; /* to implement executable_if_alloc policy */
#endif

#ifdef CUSTOM_TRACES_RET_REMOVAL
    int num_calls;
    int num_rets;
    int call_depth;
#endif

#ifdef CHECK_RETURNS_SSE2
    int call_depth;
    void *call_stack;
#endif

#ifdef DEBUG
    file_t logfile;
    thread_local_statistics_t *thread_stats;
    bool expect_last_syscall_to_fail;
    /* HACK to avoid recursion on pclookup for target invoking disassembly
     * during decode_fragment() for a coarse target
     */
    bool in_opnd_disassemble;
#endif /* DEBUG */
#ifdef DEADLOCK_AVOIDANCE
    thread_locks_t *thread_owned_locks;
#endif
#ifdef KSTATS
    thread_kstats_t *thread_kstats;
#endif

#ifdef PROFILE_RDTSC
    uint64 cache_enter_time;
    uint64 start_time; /* records start time for profiling */
    fragment_t *prev_fragment;
    /* top ten times spent in cache */
    uint64 cache_frag_count; /* num frag execs in single cache period */
    uint64 cache_time[10];   /* top ten times spent in cache */
    uint64 cache_count[10];  /* top ten cache_frag_counts */
#endif

    /* client interface-specific data */
    client_data_t *client_data;
#ifdef DEBUG
    /* i#2237: on exit we delete client_data before some IS_CLIENT_THREAD asserts. */
    bool is_client_thread_exiting;
#endif

    /* FIXME trace_sysenter_exit is used to capture an exit from a trace that
     * ends in a SYSENTER and to enable trace head marking. So it's really a
     * monitor-centric variable. It's placed in the context for now so that
     * it's not shared across contexts (as the monitor data is). Cross-context
     * sharing of this field could cause inadvertent trace head marking,
     * for example, during a callback. A longer-term fix may be to move it to
     * a context-private non-shared monitor struct.
     */
    bool trace_sysenter_exit;

    /* DR sets this field to indicate that it's forging an exception that
     * may appear to originate in DR but should be passed on to the app. */
    app_pc forged_exception_addr;

    /* DR sets this field to indicate that it should forge a single
     * step exception when app comes to that address. */
    app_pc single_step_addr;
#ifdef HOT_PATCHING_INTERFACE
    /* Fix for case 5367. */
    bool nudge_thread;             /* True only if this is a nudge thread. */
    dr_jmp_buf_t hotp_excpt_state; /* To handle hot patch exceptions. */
#endif
    try_except_t try_except; /* for TRY/EXCEPT/FINALLY */

#ifdef WINDOWS
    /* for ASLR_SHARED_CONTENT, note per callback, not per thread to
     * track properties of a syscall or a syscall pair.  Even we don't
     * expect to get APCs or callbacks while processing normal DLLs
     * this should be per dcontext.
     */
    aslr_syscall_context_t aslr_context;

    /* Initially memset to 0 in create dcontext.  If this is a nudge (or
     * internal detach) thread (as detected by start address in intercept_apc),
     * nudge_target is set to the corresponding nudge routine (currently always
     * generic_nudge_target).  We can then check this
     * value in those routines after we come out of the cache as a security
     * measure (xref case 552).  This gives us some protection against an
     * attacker leveraging our own detach routines and the like.  FIXME - if
     * the attacker is able to specify the start address for a newly created
     * thread then they can fake this. */
    void *nudge_target;

    /* If free_app_stack is set, we free the application stack during thread exit
     * cleanup.  Used for nudge threads. */
    bool free_app_stack;

    /* used when a nudge invokes dr_exit_process() */
    bool nudge_terminate_process;
    uint nudge_exit_code;
#endif /* WINDOWS */

    /* we keep an absolute address pointer to our tls state so that we
     * can access it from other threads
     */
    local_state_t *local_state;

#ifdef WINDOWS
    /* case 8721: saving the win32 start address so we can print it in the
     * ldmp.  An alternative solution is to call NtQueryInformationThread
     * with ThreadQuerySetWin32StartAddress at dump time.  According to
     * Nebbet, however, a thread calling ZwReplyWaitReplyPort or
     * ZwReplyWaitRecievePort will clobber the start address.
     */
    app_pc win32_start_addr;
#endif

    /* Used to abort bb building on decode faults.  Not persistent across cache. */
    void *bb_build_info;

#ifdef UNIX
    pending_nudge_t *nudge_pending;
    /* frag we unlinked to expedite nudge delivery */
    fragment_t *interrupted_for_nudge;
#    ifdef DEBUG
    /* i#238/PR 499179: check that libc errno hasn't changed */
    int libc_errno;
#    endif
#else
#    ifdef DEBUG
    bool post_syscall;
#    endif
#endif
    /* The start/stop API doesn't change thread_record_t.under_dynamo_control,
     * but we need some indication so we add a custom field.
     */
    bool currently_stopped;
    /* This is a flag requesting that this thread go native. */
    bool go_native;
#ifdef LINUX
    /* State for handling restartable sequences ("rseq"). */
    rseq_entry_state_t rseq_entry_state;
#endif
};

/* sentinel value for dcontext_t* used to indicate
 * "global rather than a particular thread"
 */
#define GLOBAL_DCONTEXT ((dcontext_t *)PTR_UINT_MINUS_1)

/* FIXME: why do we need to force the inline for this simple function? */
static INLINE_FORCED priv_mcontext_t *
get_mcontext(dcontext_t *dcontext)
{
    if (TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask))
        return &(dcontext->upcontext.separate_upcontext->mcontext);
    else
        return &(dcontext->upcontext.upcontext.mcontext);
}

/* A number of routines (dump_mbi*, dump_mcontext, dump_callstack,
 * print_modules) have an argument on whether to dump in an xml friendly format
 * (for xml diagnostics files). We use these defines for readability. */
enum { DUMP_XML = true, DUMP_NOT_XML = false };

/* io.c */
/* to avoid transparency problems we must have our own vnsprintf and sscanf */
#include <stdarg.h> /* for va_list */
int
d_r_snprintf(char *s, size_t max, const char *fmt, ...);
int
d_r_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
int
d_r_snprintf_wide(wchar_t *s, size_t max, const wchar_t *fmt, ...);
int
d_r_vsnprintf_wide(wchar_t *s, size_t max, const wchar_t *fmt, va_list ap);
#undef snprintf /* defined on macos */
#define snprintf d_r_snprintf
#undef _snprintf
#define _snprintf d_r_snprintf
#undef vsnprintf
#define vsnprintf d_r_vsnprintf
#define snwprintf d_r_snprintf_wide
#define _snwprintf d_r_snprintf_wide
int
d_r_sscanf(const char *str, const char *format, ...);
int
d_r_vsscanf(const char *str, const char *fmt, va_list ap);
const char *
d_r_parse_int(const char *sp, uint64 *res_out, uint base, uint width, bool is_signed);
ssize_t
utf16_to_utf8_size(const wchar_t *src, size_t max_chars,
                   size_t *written /*unicode chars*/);
#define sscanf d_r_sscanf

/* string.c */
#ifdef UNIX
/* i#3348: Use unique names to avoid conflicts during static linking.  We'd
 * prefer to just invoke the d_r_ version explicitly, but on Windows we need the
 * regular name to use the ntdll versions.  Thus we use macro indirection.
 */
#    undef strlen
#    define strlen d_r_strlen
#    undef strlen
#    define strlen d_r_strlen
#    undef wcslen
#    define wcslen d_r_wcslen
#    undef strchr
#    define strchr d_r_strchr
#    undef strrchr
#    define strrchr d_r_strrchr
#    undef strncpy
#    define strncpy d_r_strncpy
#    undef strncat
#    define strncat d_r_strncat
#    undef memmove
#    define memmove d_r_memmove
#    undef strcmp
#    define strcmp d_r_strcmp
#    undef strncmp
#    define strncmp d_r_strncmp
#    undef memcmp
#    define memcmp d_r_memcmp
#    undef strstr
#    define strstr d_r_strstr
#    undef tolower
#    define tolower d_r_tolower
#    undef strcasecmp
#    define strcasecmp d_r_strcasecmp
#    undef strtoul
#    define strtoul d_r_strtoul
#endif
size_t
strlen(const char *str);
size_t
wcslen(const wchar_t *str);
char *
strchr(const char *str, int c);
char *
strrchr(const char *str, int c);
char *
strncpy(char *dst, const char *src, size_t n);
char *
strncat(char *dest, const char *src, size_t n);
void *
memmove(void *dst, const void *src, size_t n);
int
strcmp(const char *left, const char *right);
int
strncmp(const char *left, const char *right, size_t n);
int
memcmp(const void *left_v, const void *right_v, size_t n);
char *
strstr(const char *haystack, const char *needle);
int
tolower(int c);
int
strcasecmp(const char *left, const char *right);
unsigned long
strtoul(const char *str, char **end, int base);

/* Code cleanliness rules */
#ifdef WINDOWS
#    define strcasecmp _stricmp
#    define strncasecmp _strnicmp
#    define wcscasecmp _wcsicmp
#endif

#if !defined(NOT_DYNAMORIO_CORE_PROPER) && !defined(NOT_DYNAMORIO_CORE)
#    undef printf
#    define printf printf_forbidden_function
#    undef sprintf /* defined on macos */
#    define sprintf sprintf_forbidden_function
#    define swprintf swprintf_forbidden_function
#    undef vsprintf /* defined on macos */
#    define vsprintf vsprintf_forbidden_function

/* libc independence */
#    define mprotect mprotect_forbidden_function
#    define mmap mmap_forbidden_function
#    define munmap munmap_forbidden_function
#    define getppid getppid_forbidden_function
#    define sched_yield sched_yield_forbidden_function
#    define dup dup_forbidden_function
#    define sigaltstack sigaltstack_forbidden_function
#    define setitimer setitimer_forbidden_function
#    define _exit _exit_forbidden_function
#    if !(defined(MACOS) && defined(AARCH64))
#        define gettimeofday gettimeofday_forbidden_function
#    endif
#    define time time_forbidden_function
#    define modify_ldt modify_ldt_forbidden_function
#endif

#endif /* _GLOBALS_H_ */
