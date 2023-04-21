/* ******************************************************************************
 * Copyright (c) 2010-2023 Google, Inc.  All rights reserved.
 * Copyright (c) 2010-2011 Massachusetts Institute of Technology  All rights reserved.
 * Copyright (c) 2002-2010 VMware, Inc.  All rights reserved.
 * ******************************************************************************/

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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * instrument.c - interface for instrumentation
 */

#include "../globals.h" /* just to disable warning C4206 about an empty file */

#include "instrument.h"
#include "instr.h"
#include "instr_create_shared.h"
#include "instrlist.h"
#include "decode.h"
#include "disassemble.h"
#include "ir_utils.h"
#include "../fragment.h"
#include "../fcache.h"
#include "../emit.h"
#include "../link.h"
#include "../monitor.h" /* for mark_trace_head */
#include <stdarg.h>     /* for varargs */
#include "../nudge.h"   /* for nudge_internal() */
#include "../synch.h"
#include "../annotations.h"
#include "../translate.h"
#ifdef UNIX
#    include <sys/time.h>       /* ITIMER_* */
#    include "../unix/module.h" /* redirect_* functions */
#endif

/* in utils.c, not exported to everyone */
extern ssize_t
do_file_write(file_t f, const char *fmt, va_list ap);

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
#    undef ASSERT_TRUNCATE
#    undef ASSERT_BITFIELD_TRUNCATE
#    undef ASSERT_NOT_REACHED
#    define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#    define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* PR 200065: User passes us the shared library, we look up "dr_init"
 * or "dr_client_main" and call it.  From there, the client can register which events
 * it wishes to receive.
 */
#define INSTRUMENT_INIT_NAME_LEGACY "dr_init"
#define INSTRUMENT_INIT_NAME "dr_client_main"

/* PR 250952: version check
 * If changing this, don't forget to update:
 * - lib/dr_defines.h _USES_DR_VERSION_
 * - api/docs/footer.html
 */
#define USES_DR_VERSION_NAME "_USES_DR_VERSION_"
/* Should we expose this for use in samples/tracedump.c?
 * Also, if we change this, need to change the symlink generation
 * in core/CMakeLists.txt: at that point should share single define.
 */
/* OLDEST_COMPATIBLE_VERSION now comes from configure.h */
/* The 3rd version number, the bugfix/patch number, should not affect
 * compatibility, so our version check number simply uses:
 *   major*100 + minor
 * Which gives us room for 100 minor versions per major.
 */
#define NEWEST_COMPATIBLE_VERSION CURRENT_API_VERSION

/* Store the unique not-part-of-version build number (the version
 * BUILD_NUMBER is limited to 64K and is not guaranteed to be unique)
 * somewhere accessible at a customer site.  We could alternatively
 * pull it out of our DYNAMORIO_DEFINES string.
 */
DR_API const char *unique_build_number = STRINGIFY(UNIQUE_BUILD_NUMBER);

/* The flag d_r_client_avx512_code_in_use is described in arch.h. */
#define DR_CLIENT_AVX512_CODE_IN_USE_NAME "_DR_CLIENT_AVX512_CODE_IN_USE_"

/* Acquire when registering or unregistering event callbacks
 * Also held when invoking events, which happens much more often
 * than registration changes, so we use rwlock
 */
DECLARE_CXTSWPROT_VAR(static read_write_lock_t callback_registration_lock,
                      INIT_READWRITE_LOCK(callback_registration_lock));

/* Structures for maintaining lists of event callbacks */
typedef void (*callback_t)(void);
typedef struct _callback_list_t {
    callback_t *callbacks; /* array of callback functions */
    size_t num;            /* number of callbacks registered */
    size_t size;           /* allocated space (may be larger than num) */
} callback_list_t;

/* This is a little convoluted.  The following is a macro to iterate
 * over a list of callbacks and call each function.  We use a macro
 * instead of a function so we can pass the function type and perform
 * a typecast.  We need to copy the callback list before iterating to
 * support the possibility of one callback unregistering another and
 * messing up the list while we're iterating.  We'll optimize the case
 * for 5 or fewer registered callbacks and stack-allocate the temp
 * list.  Otherwise, we'll heap-allocate the temp.
 *
 * We allow the args to use the var "idx" to access the client index.
 *
 * We consider the first registered callback to have the highest
 * priority and call it last.  If we gave the last registered callback
 * the highest priority, a client could re-register a routine to
 * increase its priority.  That seems a little weird.
 */
/*
 */
#define FAST_COPY_SIZE 5
#define call_all_ret(ret, retop, postop, vec, type, ...)                         \
    do {                                                                         \
        size_t idx, num;                                                         \
        /* we will be called even if no callbacks (i.e., (vec).num == 0) */      \
        /* we guarantee we're in DR state at all callbacks and clean calls */    \
        /* XXX: add CLIENT_ASSERT here */                                        \
        d_r_read_lock(&callback_registration_lock);                              \
        num = (vec).num;                                                         \
        if (num == 0) {                                                          \
            d_r_read_unlock(&callback_registration_lock);                        \
        } else if (num <= FAST_COPY_SIZE) {                                      \
            callback_t tmp[FAST_COPY_SIZE];                                      \
            memcpy(tmp, (vec).callbacks, num * sizeof(callback_t));              \
            d_r_read_unlock(&callback_registration_lock);                        \
            for (idx = 0; idx < num; idx++) {                                    \
                ret retop(((type)tmp[num - idx - 1])(__VA_ARGS__)) postop;       \
            }                                                                    \
        } else {                                                                 \
            callback_t *tmp = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, callback_t, num, \
                                               ACCT_OTHER, UNPROTECTED);         \
            memcpy(tmp, (vec).callbacks, num * sizeof(callback_t));              \
            d_r_read_unlock(&callback_registration_lock);                        \
            for (idx = 0; idx < num; idx++) {                                    \
                ret retop(((type)tmp[num - idx - 1])(__VA_ARGS__)) postop;       \
            }                                                                    \
            HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, tmp, callback_t, num, ACCT_OTHER,   \
                            UNPROTECTED);                                        \
        }                                                                        \
    } while (0)

/* It's less error-prone if we just have one call_all macro.  We'll
 * reuse call_all_ret above for callbacks that don't have a return
 * value by assigning to a dummy var.  Note that this means we'll
 * have to pass an int-returning type to call_all()
 */
#define call_all(vec, type, ...)                          \
    do {                                                  \
        int dummy UNUSED;                                 \
        call_all_ret(dummy, =, , vec, type, __VA_ARGS__); \
    } while (0)

/* Lists of callbacks for each event type.  Note that init and nudge
 * callback lists are kept in the client_lib_t data structure below.
 * We could store all lists on a per-client basis, but we can iterate
 * over these lists slightly more efficiently if we store all
 * callbacks for a specific event in a single list.
 */
static callback_list_t exit_callbacks = {
    0,
};
static callback_list_t post_attach_callbacks = {
    0,
};
static callback_list_t pre_detach_callbacks = {
    0,
};
static callback_list_t thread_init_callbacks = {
    0,
};
static callback_list_t thread_exit_callbacks = {
    0,
};
#ifdef UNIX
static callback_list_t fork_init_callbacks = {
    0,
};
#endif
static callback_list_t low_on_memory_callbacks = {
    0,
};
static callback_list_t bb_callbacks = {
    0,
};
static callback_list_t trace_callbacks = {
    0,
};
static callback_list_t end_trace_callbacks = {
    0,
};
static callback_list_t fragdel_callbacks = {
    0,
};
static callback_list_t restore_state_callbacks = {
    0,
};
static callback_list_t restore_state_ex_callbacks = {
    0,
};
static callback_list_t module_load_callbacks = {
    0,
};
static callback_list_t module_unload_callbacks = {
    0,
};
static callback_list_t filter_syscall_callbacks = {
    0,
};
static callback_list_t pre_syscall_callbacks = {
    0,
};
static callback_list_t post_syscall_callbacks = {
    0,
};
static callback_list_t kernel_xfer_callbacks = {
    0,
};
#ifdef WINDOWS
static callback_list_t exception_callbacks = {
    0,
};
#else
static callback_list_t signal_callbacks = {
    0,
};
#endif
#ifdef PROGRAM_SHEPHERDING
static callback_list_t security_violation_callbacks = {
    0,
};
#endif
static callback_list_t clean_call_insertion_callbacks = {
    0,
};
static callback_list_t persist_ro_size_callbacks = {
    0,
};
static callback_list_t persist_ro_callbacks = {
    0,
};
static callback_list_t resurrect_ro_callbacks = {
    0,
};
static callback_list_t persist_rx_size_callbacks = {
    0,
};
static callback_list_t persist_rx_callbacks = {
    0,
};
static callback_list_t resurrect_rx_callbacks = {
    0,
};
static callback_list_t persist_rw_size_callbacks = {
    0,
};
static callback_list_t persist_rw_callbacks = {
    0,
};
static callback_list_t resurrect_rw_callbacks = {
    0,
};
static callback_list_t persist_patch_callbacks = {
    0,
};

/* An array of client libraries.  We use a static array instead of a
 * heap-allocated list so we can load the client libs before
 * initializing DR's heap.
 */
typedef struct _client_lib_t {
    client_id_t id;
    char path[MAXIMUM_PATH];
    /* PR 366195: dlopen() handle truly is opaque: != start */
    shlib_handle_t lib;
    app_pc start;
    app_pc end;
    /* The raw option string, which after i#1736 contains token-delimiting quotes */
    char options[MAX_OPTION_LENGTH];
    /* The option string with token-delimiting quotes removed for backward compat */
    char legacy_options[MAX_OPTION_LENGTH];
    /* The parsed options: */
    int argc;
    const char **argv;
    /* We need to associate nudge events with a specific client so we
     * store that list here in the client_lib_t instead of using a
     * single global list.
     */
    callback_list_t nudge_callbacks;
} client_lib_t;

/* these should only be modified prior to instrument_init(), since no
 * readers of the client_libs array (event handlers, etc.) use synch
 */
static client_lib_t client_libs[MAX_CLIENT_LIBS] = { {
    0,
} };
static size_t num_client_libs = 0;

static void *persist_user_data[MAX_CLIENT_LIBS];

#ifdef WINDOWS
/* private kernel32 lib, used to print to console */
static bool print_to_console;
static shlib_handle_t priv_kernel32;
typedef BOOL(WINAPI *kernel32_WriteFile_t)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static kernel32_WriteFile_t kernel32_WriteFile;

static ssize_t
dr_write_to_console_varg(bool to_stdout, const char *fmt, ...);
#endif

bool client_requested_exit;

#ifdef WINDOWS
/* used for nudge support */
static bool block_client_nudge_threads = false;
DECLARE_CXTSWPROT_VAR(static int num_client_nudge_threads, 0);
#endif
/* # of sideline threads */
DECLARE_CXTSWPROT_VAR(static int num_client_sideline_threads, 0);
/* protects block_client_nudge_threads and incrementing num_client_nudge_threads */
DECLARE_CXTSWPROT_VAR(static mutex_t client_thread_count_lock,
                      INIT_LOCK_FREE(client_thread_count_lock));

static vm_area_vector_t *client_aux_libs;

static bool track_where_am_i;

#ifdef WINDOWS
DECLARE_CXTSWPROT_VAR(static mutex_t client_aux_lib64_lock,
                      INIT_LOCK_FREE(client_aux_lib64_lock));
#endif

/****************************************************************************/
/* INTERNAL ROUTINES */

static bool
char_is_quote(char c)
{
    return c == '"' || c == '\'' || c == '`';
}

static void
parse_option_array(client_id_t client_id, const char *opstr, int *argc OUT,
                   const char ***argv OUT, size_t max_token_size)
{
    const char **a;
    int cnt;
    const char *s;
    char *token =
        HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, char, max_token_size, ACCT_CLIENT, UNPROTECTED);

    for (cnt = 0, s = dr_get_token(opstr, token, max_token_size); s != NULL;
         s = dr_get_token(s, token, max_token_size)) {
        cnt++;
    }
    cnt++; /* add 1 so 0 can be "app" */

    a = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, const char *, cnt, ACCT_CLIENT, UNPROTECTED);

    cnt = 0;
    a[cnt] = dr_strdup(dr_get_client_path(client_id) HEAPACCT(ACCT_CLIENT));
    cnt++;
    for (s = dr_get_token(opstr, token, max_token_size); s != NULL;
         s = dr_get_token(s, token, max_token_size)) {
        a[cnt] = dr_strdup(token HEAPACCT(ACCT_CLIENT));
        cnt++;
    }

    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, token, char, max_token_size, ACCT_CLIENT,
                    UNPROTECTED);

    *argc = cnt;
    *argv = a;
}

static bool
free_option_array(int argc, const char **argv)
{
    int i;
    for (i = 0; i < argc; i++) {
        dr_strfree(argv[i] HEAPACCT(ACCT_CLIENT));
    }
    HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, argv, char *, argc, ACCT_CLIENT, UNPROTECTED);
    return true;
}

static void
add_callback(callback_list_t *vec, void (*func)(void), bool unprotect)
{
    if (func == NULL) {
        CLIENT_ASSERT(false, "trying to register a NULL callback");
        return;
    }
    if (standalone_library) {
        CLIENT_ASSERT(false, "events not supported in standalone library mode");
        return;
    }

    d_r_write_lock(&callback_registration_lock);
    /* Although we're receiving a pointer to a callback_list_t, we're
     * usually modifying a static var.
     */
    if (unprotect) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    }

    /* We may already have an open slot since we allocate in twos and
     * because we don't bother to free the storage when we remove the
     * callback.  Check and only allocate if necessary.
     */
    if (vec->num == vec->size) {
        callback_t *tmp = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, callback_t,
                                           vec->size + 2, /* Let's allocate 2 */
                                           ACCT_OTHER, UNPROTECTED);

        if (tmp == NULL) {
            CLIENT_ASSERT(false, "out of memory: can't register callback");
            d_r_write_unlock(&callback_registration_lock);
            return;
        }

        if (vec->callbacks != NULL) {
            memcpy(tmp, vec->callbacks, vec->num * sizeof(callback_t));
            HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, vec->callbacks, callback_t, vec->size,
                            ACCT_OTHER, UNPROTECTED);
        }
        vec->callbacks = tmp;
        vec->size += 2;
    }

    vec->callbacks[vec->num] = func;
    vec->num++;

    if (unprotect) {
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
    d_r_write_unlock(&callback_registration_lock);
}

static bool
remove_callback(callback_list_t *vec, void (*func)(void), bool unprotect)
{
    size_t i;
    bool found = false;

    if (func == NULL) {
        CLIENT_ASSERT(false, "trying to unregister a NULL callback");
        return false;
    }

    d_r_write_lock(&callback_registration_lock);
    /* Although we're receiving a pointer to a callback_list_t, we're
     * usually modifying a static var.
     */
    if (unprotect) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    }

    for (i = 0; i < vec->num; i++) {
        if (vec->callbacks[i] == func) {
            size_t j;
            /* shift down the entries on the tail */
            for (j = i; j < vec->num - 1; j++) {
                vec->callbacks[j] = vec->callbacks[j + 1];
            }

            vec->num -= 1;
            found = true;
            break;
        }
    }

    if (unprotect) {
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    }
    d_r_write_unlock(&callback_registration_lock);

    return found;
}

/* This should only be called prior to instrument_init(),
 * since no readers of the client_libs array use synch
 * and since this routine assumes .data is writable.
 */
static void
add_client_lib(const char *path, const char *id_str, const char *options)
{
    client_id_t id;
    shlib_handle_t client_lib;
    DEBUG_DECLARE(size_t i);

    ASSERT(!dynamo_initialized);

    /* if ID not specified, we'll default to 0 */
    id = (id_str == NULL) ? 0 : strtoul(id_str, NULL, 16);

#ifdef DEBUG
    /* Check for conflicting IDs */
    for (i = 0; i < num_client_libs; i++) {
        CLIENT_ASSERT(client_libs[i].id != id, "Clients have the same ID");
    }
#endif

    if (num_client_libs == MAX_CLIENT_LIBS) {
        CLIENT_ASSERT(false, "Max number of clients reached");
        return;
    }

    LOG(GLOBAL, LOG_INTERP, 4, "about to load client library %s\n", path);

    client_lib =
        load_shared_library(path, IF_X64_ELSE(DYNAMO_OPTION(reachable_client), true));
    if (client_lib == NULL) {
        char msg[MAXIMUM_PATH * 4];
        char err[MAXIMUM_PATH * 2];
        shared_library_error(err, BUFFER_SIZE_ELEMENTS(err));
        snprintf(msg, BUFFER_SIZE_ELEMENTS(msg),
                 ".\n\tError opening instrumentation library %s:\n\t%s", path, err);
        NULL_TERMINATE_BUFFER(msg);

        /* PR 232490 - malformed library names or incorrect
         * permissions shouldn't blow up an app in release builds as
         * they may happen at customer sites with a third party
         * client.
         */
        /* PR 408318: 32-vs-64 errors should NOT be fatal to continue
         * in debug build across execve chains.  Xref i#147.
         * XXX: w/ -private_loader, err always equals "error in private loader"
         * and so we never match here!
         */
        IF_UNIX(if (strstr(err, "wrong ELF class") == NULL))
        CLIENT_ASSERT(false, msg);
        SYSLOG(SYSLOG_ERROR, CLIENT_LIBRARY_UNLOADABLE, 4, get_application_name(),
               get_application_pid(), path, msg);
    } else {
        /* PR 250952: version check */
        int *uses_dr_version =
            (int *)lookup_library_routine(client_lib, USES_DR_VERSION_NAME);
        if (uses_dr_version == NULL || *uses_dr_version < OLDEST_COMPATIBLE_VERSION ||
            *uses_dr_version > NEWEST_COMPATIBLE_VERSION) {
            /* not a fatal usage error since we want release build to continue */
            CLIENT_ASSERT(false,
                          "client library is incompatible with this version of DR");
            SYSLOG(SYSLOG_ERROR, CLIENT_VERSION_INCOMPATIBLE, 2, get_application_name(),
                   get_application_pid());
        } else {
            size_t idx = num_client_libs++;
            client_libs[idx].id = id;
            client_libs[idx].lib = client_lib;
            app_pc client_start, client_end;
#if defined(STATIC_LIBRARY) && defined(LINUX)
            // For DR under static+linux we know that the client and DR core
            // code are built into the app itself. To avoid various edge cases
            // in finding the "library" bounds, delegate this boundary discovery
            // to the dll bounds functions. xref i#3387.
            client_start = get_dynamorio_dll_start();
            client_end = get_dynamorio_dll_end();
            ASSERT(client_start <= (app_pc)uses_dr_version &&
                   (app_pc)uses_dr_version < client_end);
#else
            DEBUG_DECLARE(bool ok =)
            shared_library_bounds(client_lib, (byte *)uses_dr_version, NULL,
                                  &client_start, &client_end);
            ASSERT(ok);
#endif
            client_libs[idx].start = client_start;
            client_libs[idx].end = client_end;

            LOG(GLOBAL, LOG_INTERP, 1, "loaded %s at " PFX "-" PFX "\n", path,
                client_libs[idx].start, client_libs[idx].end);
#ifdef X64
            /* Now that we map the client within the constraints, this request
             * should always succeed.
             */
            if (DYNAMO_OPTION(reachable_client)) {
                request_region_be_heap_reachable(client_libs[idx].start,
                                                 client_libs[idx].end -
                                                     client_libs[idx].start);
            }
#endif
            strncpy(client_libs[idx].path, path,
                    BUFFER_SIZE_ELEMENTS(client_libs[idx].path));
            NULL_TERMINATE_BUFFER(client_libs[idx].path);

            if (options != NULL) {
                strncpy(client_libs[idx].options, options,
                        BUFFER_SIZE_ELEMENTS(client_libs[idx].options));
                NULL_TERMINATE_BUFFER(client_libs[idx].options);
            }
#ifdef X86
            bool *client_avx512_code_in_use = (bool *)lookup_library_routine(
                client_lib, DR_CLIENT_AVX512_CODE_IN_USE_NAME);
            if (client_avx512_code_in_use != NULL) {
                if (*client_avx512_code_in_use)
                    d_r_set_client_avx512_code_in_use();
            }
#endif

            /* We'll look up dr_client_main and call it in instrument_init */
        }
    }
}

void
instrument_load_client_libs(void)
{
    if (CLIENTS_EXIST()) {
        char buf[MAX_LIST_OPTION_LENGTH];
        char *path;

        string_option_read_lock();
        strncpy(buf, INTERNAL_OPTION(client_lib), BUFFER_SIZE_ELEMENTS(buf));
        string_option_read_unlock();
        NULL_TERMINATE_BUFFER(buf);

        /* We're expecting path;ID;options triples */
        path = buf;
        do {
            char *id = NULL;
            char *options = NULL;
            char *next_path = NULL;

            id = strstr(path, ";");
            if (id != NULL) {
                id[0] = '\0';
                id++;

                options = strstr(id, ";");
                if (options != NULL) {
                    options[0] = '\0';
                    options++;

                    next_path = strstr(options, ";");
                    if (next_path != NULL) {
                        next_path[0] = '\0';
                        next_path++;
                    }
                }
            }

#ifdef STATIC_LIBRARY
            /* We ignore client library paths and allow client code anywhere in the app.
             * We have a check in load_shared_library() to avoid loading
             * a 2nd copy of the app.
             * We do support passing client ID and options via the first -client_lib.
             */
            add_client_lib(get_application_name(), id == NULL ? "0" : id,
                           options == NULL ? "" : options);
            break;
#endif
            add_client_lib(path, id, options);
            path = next_path;
        } while (path != NULL);
    }
}

static void
init_client_aux_libs(void)
{
    if (client_aux_libs == NULL) {
        VMVECTOR_ALLOC_VECTOR(client_aux_libs, GLOBAL_DCONTEXT, VECTOR_SHARED,
                              client_aux_libs);
    }
}

void
instrument_init(void)
{
    size_t i;

    init_client_aux_libs();

#ifdef X86
    if (IF_WINDOWS_ELSE(!dr_earliest_injected, !DYNAMO_OPTION(early_inject))) {
        /* A client that had been compiled with AVX-512 may clobber an application's
         * state. AVX-512 context switching will not be lazy in this case.
         */
        if (d_r_is_client_avx512_code_in_use())
            d_r_set_avx512_code_in_use(true, NULL);
    }
#endif

    if (num_client_libs > 0) {
        /* We no longer distinguish in-DR vs in-client crashes, as many crashes in
         * the DR lib are really client bugs.
         * We expect most end-user tools to call dr_set_client_name() so we
         * have generic defaults here:
         */
        set_exception_strings("Tool", "your tool's issue tracker");
    }

    /* Iterate over the client libs and call each init routine */
    for (i = 0; i < num_client_libs; i++) {
        void (*init)(client_id_t, int, const char **) =
            (void (*)(client_id_t, int, const char **))(
                lookup_library_routine(client_libs[i].lib, INSTRUMENT_INIT_NAME));
        void (*legacy)(client_id_t) = (void (*)(client_id_t))(
            lookup_library_routine(client_libs[i].lib, INSTRUMENT_INIT_NAME_LEGACY));

        /* we can't do this in instrument_load_client_libs() b/c vmheap
         * is not set up at that point
         */
        all_memory_areas_lock();
        update_all_memory_areas(client_libs[i].start, client_libs[i].end,
                                /* FIXME: need to walk the sections: but may be
                                 * better to obfuscate from clients anyway.
                                 * We can't set as MEMPROT_NONE as that leads to
                                 * bugs if the app wants to interpret part of
                                 * its code section (xref PR 504629).
                                 */
                                MEMPROT_READ, DR_MEMTYPE_IMAGE);
        all_memory_areas_unlock();

        /* i#1736: parse the options up front */
        parse_option_array(client_libs[i].id, client_libs[i].options,
                           &client_libs[i].argc, &client_libs[i].argv, MAX_OPTION_LENGTH);

#ifdef STATIC_LIBRARY
        /* We support the app having client code anywhere, so there does not
         * have to be an init routine that we call.  This means the app
         * may have to iterate modules on its own.
         */
#else
        /* Since the user has to register all other events, it
         * doesn't make sense to provide the -client_lib
         * option for a module that doesn't export an init routine.
         */
        CLIENT_ASSERT(init != NULL || legacy != NULL,
                      "client does not export a dr_client_main or dr_init routine");
#endif
        if (init != NULL)
            (*init)(client_libs[i].id, client_libs[i].argc, client_libs[i].argv);
        else if (legacy != NULL)
            (*legacy)(client_libs[i].id);
    }

    /* We now initialize the 1st thread before coming here, so we can
     * hand the client a dcontext; so we need to specially generate
     * the thread init event now.  An alternative is to have
     * dr_get_global_drcontext(), but that's extra complexity for no
     * real reason.
     * We raise the thread init event prior to the module load events
     * so the client can access a dcontext in module load events (i#1339).
     */
    if (thread_init_callbacks.num > 0) {
        instrument_thread_init(get_thread_private_dcontext(), false, false);
    }

    /* If the client just registered the module-load event, let's
     * assume it wants to be informed of *all* modules and tell it
     * which modules are already loaded.  If the client registers the
     * event later, it will need to use the module iterator routines
     * to retrieve currently loaded modules.  We use the dr_module_iterator
     * exposed to the client to avoid locking issues.
     */
    if (module_load_callbacks.num > 0) {
        dr_module_iterator_t *mi = dr_module_iterator_start();
        while (dr_module_iterator_hasnext(mi)) {
            module_data_t *data = dr_module_iterator_next(mi);
            instrument_module_load(data, true /*already loaded*/);
            /* XXX; more efficient to set this flag during dr_module_iterator_start */
            os_module_set_flag(data->start, MODULE_LOAD_EVENT);
            dr_free_module_data(data);
        }
        dr_module_iterator_stop(mi);
    }
}

static void
free_callback_list(callback_list_t *vec)
{
    if (vec->callbacks != NULL) {
        HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, vec->callbacks, callback_t, vec->size,
                        ACCT_OTHER, UNPROTECTED);
        vec->callbacks = NULL;
    }
    vec->size = 0;
    vec->num = 0;
}

static void
free_all_callback_lists()
{
    free_callback_list(&exit_callbacks);
    free_callback_list(&post_attach_callbacks);
    free_callback_list(&pre_detach_callbacks);
    free_callback_list(&thread_init_callbacks);
    free_callback_list(&thread_exit_callbacks);
#ifdef UNIX
    free_callback_list(&fork_init_callbacks);
#endif
    free_callback_list(&low_on_memory_callbacks);
    free_callback_list(&bb_callbacks);
    free_callback_list(&trace_callbacks);
    free_callback_list(&end_trace_callbacks);
    free_callback_list(&fragdel_callbacks);
    free_callback_list(&restore_state_callbacks);
    free_callback_list(&restore_state_ex_callbacks);
    free_callback_list(&module_load_callbacks);
    free_callback_list(&module_unload_callbacks);
    free_callback_list(&filter_syscall_callbacks);
    free_callback_list(&pre_syscall_callbacks);
    free_callback_list(&post_syscall_callbacks);
    free_callback_list(&kernel_xfer_callbacks);
#ifdef WINDOWS
    free_callback_list(&exception_callbacks);
#else
    free_callback_list(&signal_callbacks);
#endif
    free_callback_list(&clean_call_insertion_callbacks);
#ifdef PROGRAM_SHEPHERDING
    free_callback_list(&security_violation_callbacks);
#endif
    free_callback_list(&persist_ro_size_callbacks);
    free_callback_list(&persist_ro_callbacks);
    free_callback_list(&resurrect_ro_callbacks);
    free_callback_list(&persist_rx_size_callbacks);
    free_callback_list(&persist_rx_callbacks);
    free_callback_list(&resurrect_rx_callbacks);
    free_callback_list(&persist_rw_size_callbacks);
    free_callback_list(&persist_rw_callbacks);
    free_callback_list(&resurrect_rw_callbacks);
    free_callback_list(&persist_patch_callbacks);
}

void
instrument_exit_event(void)
{
    /* Note - currently own initexit lock when this is called (see PR 227619). */

    /* support dr_get_mcontext() from the exit event */
    if (!standalone_library)
        get_thread_private_dcontext()->client_data->mcontext_in_dcontext = true;
    call_all(exit_callbacks, int (*)(),
             /* It seems the compiler is confused if we pass no var args
              * to the call_all macro.  Bogus NULL arg */
             NULL);
}

void
instrument_post_attach_event(void)
{
    if (!dynamo_control_via_attach) {
        ASSERT(post_attach_callbacks.num == 0);
        return;
    }
    call_all(post_attach_callbacks, int (*)(), NULL);
}

void
instrument_pre_detach_event(void)
{
    call_all(pre_detach_callbacks, int (*)(), NULL);
}

void
instrument_exit(void)
{
    /* Note - currently own initexit lock when this is called (see PR 227619). */

    if (IF_DEBUG_ELSE(true, doing_detach)) {
        /* Unload all client libs and free any allocated storage */
        size_t i;
        for (i = 0; i < num_client_libs; i++) {
            free_callback_list(&client_libs[i].nudge_callbacks);
            unload_shared_library(client_libs[i].lib);
            if (client_libs[i].argv != NULL)
                free_option_array(client_libs[i].argc, client_libs[i].argv);
        }
        free_all_callback_lists();
    }

    vmvector_delete_vector(GLOBAL_DCONTEXT, client_aux_libs);
    client_aux_libs = NULL;
    num_client_libs = 0;
#ifdef WINDOWS
    DELETE_LOCK(client_aux_lib64_lock);
#endif
    DELETE_LOCK(client_thread_count_lock);
    DELETE_READWRITE_LOCK(callback_registration_lock);
}

bool
is_in_client_lib(app_pc addr)
{
    /* NOTE: we use this routine for detecting exceptions in
     * clients. If we add a callback on that event we'll have to be
     * sure to deliver it only to the right client.
     */
    if (is_in_client_lib_ignore_aux(addr))
        return true;
    if (client_aux_libs != NULL && vmvector_overlap(client_aux_libs, addr, addr + 1))
        return true;
    return false;
}

bool
is_in_client_lib_ignore_aux(app_pc addr)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if ((addr >= (app_pc)client_libs[i].start) && (addr < client_libs[i].end)) {
            return true;
        }
    }
    return false;
}

bool
get_client_bounds(client_id_t client_id, app_pc *start /*OUT*/, app_pc *end /*OUT*/)
{
    if (client_id >= num_client_libs)
        return false;
    if (start != NULL)
        *start = (app_pc)client_libs[client_id].start;
    if (end != NULL)
        *end = (app_pc)client_libs[client_id].end;
    return true;
}

const char *
get_client_path_from_addr(app_pc addr)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if ((addr >= (app_pc)client_libs[i].start) && (addr < client_libs[i].end)) {
            return client_libs[i].path;
        }
    }

    return "";
}

bool
is_valid_client_id(client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return true;
        }
    }
    return false;
}

void
dr_register_exit_event(void (*func)(void))
{
    add_callback(&exit_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_exit_event(void (*func)(void))
{
    return remove_callback(&exit_callbacks, (void (*)(void))func, true);
}

bool
dr_register_post_attach_event(void (*func)(void))
{
    if (!dynamo_control_via_attach)
        return false;
    add_callback(&post_attach_callbacks, (void (*)(void))func, true);
    return true;
}

bool
dr_unregister_post_attach_event(void (*func)(void))
{
    return remove_callback(&post_attach_callbacks, (void (*)(void))func, true);
}

void
dr_register_pre_detach_event(void (*func)(void))
{
    /* We do not want to rule out detaching when there was no attach, so we do
     * not check dynamo_control_via_attach.
     */
    add_callback(&pre_detach_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_pre_detach_event(void (*func)(void))
{
    return remove_callback(&pre_detach_callbacks, (void (*)(void))func, true);
}

void
dr_register_bb_event(dr_emit_flags_t (*func)(void *drcontext, void *tag, instrlist_t *bb,
                                             bool for_trace, bool translating))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for bb event when code_api is disabled");
        return;
    }

    add_callback(&bb_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_bb_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                               instrlist_t *bb, bool for_trace,
                                               bool translating))
{
    return remove_callback(&bb_callbacks, (void (*)(void))func, true);
}

void
dr_register_trace_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                instrlist_t *trace, bool translating))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for trace event when code_api is disabled");
        return;
    }

    add_callback(&trace_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_trace_event(dr_emit_flags_t (*func)(void *drcontext, void *tag,
                                                  instrlist_t *trace, bool translating))
{
    return remove_callback(&trace_callbacks, (void (*)(void))func, true);
}

void
dr_register_end_trace_event(dr_custom_trace_action_t (*func)(void *drcontext, void *tag,
                                                             void *next_tag))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for end-trace event when code_api is disabled");
        return;
    }

    add_callback(&end_trace_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_end_trace_event(dr_custom_trace_action_t (*func)(void *drcontext, void *tag,
                                                               void *next_tag))
{
    return remove_callback(&end_trace_callbacks, (void (*)(void))func, true);
}

void
dr_register_delete_event(void (*func)(void *drcontext, void *tag))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for delete event when code_api is disabled");
        return;
    }

    add_callback(&fragdel_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_delete_event(void (*func)(void *drcontext, void *tag))
{
    return remove_callback(&fragdel_callbacks, (void (*)(void))func, true);
}

void
dr_register_restore_state_event(void (*func)(void *drcontext, void *tag,
                                             dr_mcontext_t *mcontext, bool restore_memory,
                                             bool app_code_consistent))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for restore state event when code_api is disabled");
        return;
    }

    add_callback(&restore_state_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_restore_state_event(void (*func)(void *drcontext, void *tag,
                                               dr_mcontext_t *mcontext,
                                               bool restore_memory,
                                               bool app_code_consistent))
{
    return remove_callback(&restore_state_callbacks, (void (*)(void))func, true);
}

void
dr_register_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                dr_restore_state_info_t *info))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for restore_state_ex event when code_api disabled");
        return;
    }

    add_callback(&restore_state_ex_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_restore_state_ex_event(bool (*func)(void *drcontext, bool restore_memory,
                                                  dr_restore_state_info_t *info))
{
    return remove_callback(&restore_state_ex_callbacks, (void (*)(void))func, true);
}

void
dr_register_thread_init_event(void (*func)(void *drcontext))
{
    add_callback(&thread_init_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_thread_init_event(void (*func)(void *drcontext))
{
    return remove_callback(&thread_init_callbacks, (void (*)(void))func, true);
}

void
dr_register_thread_exit_event(void (*func)(void *drcontext))
{
    add_callback(&thread_exit_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_thread_exit_event(void (*func)(void *drcontext))
{
    return remove_callback(&thread_exit_callbacks, (void (*)(void))func, true);
}

#ifdef UNIX
void
dr_register_fork_init_event(void (*func)(void *drcontext))
{
    add_callback(&fork_init_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_fork_init_event(void (*func)(void *drcontext))
{
    return remove_callback(&fork_init_callbacks, (void (*)(void))func, true);
}
#endif

void
dr_register_low_on_memory_event(void (*func)())
{
    add_callback(&low_on_memory_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_low_on_memory_event(void (*func)())
{
    return remove_callback(&low_on_memory_callbacks, (void (*)(void))func, true);
}

void
dr_register_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                           bool loaded))
{
    add_callback(&module_load_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_module_load_event(void (*func)(void *drcontext, const module_data_t *info,
                                             bool loaded))
{
    return remove_callback(&module_load_callbacks, (void (*)(void))func, true);
}

void
dr_register_module_unload_event(void (*func)(void *drcontext, const module_data_t *info))
{
    add_callback(&module_unload_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_module_unload_event(void (*func)(void *drcontext,
                                               const module_data_t *info))
{
    return remove_callback(&module_unload_callbacks, (void (*)(void))func, true);
}

#ifdef WINDOWS
void
dr_register_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt))
{
    add_callback(&exception_callbacks, (bool (*)(void))func, true);
}

bool
dr_unregister_exception_event(bool (*func)(void *drcontext, dr_exception_t *excpt))
{
    return remove_callback(&exception_callbacks, (bool (*)(void))func, true);
}
#else
void
dr_register_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                    dr_siginfo_t *siginfo))
{
    add_callback(&signal_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_signal_event(dr_signal_action_t (*func)(void *drcontext,
                                                      dr_siginfo_t *siginfo))
{
    return remove_callback(&signal_callbacks, (void (*)(void))func, true);
}
#endif /* WINDOWS */

void
dr_register_filter_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    add_callback(&filter_syscall_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_filter_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return remove_callback(&filter_syscall_callbacks, (void (*)(void))func, true);
}

void
dr_register_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    add_callback(&pre_syscall_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_pre_syscall_event(bool (*func)(void *drcontext, int sysnum))
{
    return remove_callback(&pre_syscall_callbacks, (void (*)(void))func, true);
}

void
dr_register_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    add_callback(&post_syscall_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_post_syscall_event(void (*func)(void *drcontext, int sysnum))
{
    return remove_callback(&post_syscall_callbacks, (void (*)(void))func, true);
}

void
dr_register_kernel_xfer_event(void (*func)(void *drcontext,
                                           const dr_kernel_xfer_info_t *info))
{
    add_callback(&kernel_xfer_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_kernel_xfer_event(void (*func)(void *drcontext,
                                             const dr_kernel_xfer_info_t *info))
{
    return remove_callback(&kernel_xfer_callbacks, (void (*)(void))func, true);
}

#ifdef PROGRAM_SHEPHERDING
void
dr_register_security_event(void (*func)(void *drcontext, void *source_tag,
                                        app_pc source_pc, app_pc target_pc,
                                        dr_security_violation_type_t violation,
                                        dr_mcontext_t *mcontext,
                                        dr_security_violation_action_t *action))
{
    add_callback(&security_violation_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_security_event(void (*func)(void *drcontext, void *source_tag,
                                          app_pc source_pc, app_pc target_pc,
                                          dr_security_violation_type_t violation,
                                          dr_mcontext_t *mcontext,
                                          dr_security_violation_action_t *action))
{
    return remove_callback(&security_violation_callbacks, (void (*)(void))func, true);
}
#endif

void
dr_register_clean_call_insertion_event(void (*func)(void *drcontext, instrlist_t *ilist,
                                                    instr_t *where,
                                                    dr_cleancall_save_t call_flags))
{
    add_callback(&clean_call_insertion_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_clean_call_insertion_event(void (*func)(void *drcontext, instrlist_t *ilist,
                                                      instr_t *where,
                                                      dr_cleancall_save_t call_flags))
{
    return remove_callback(&clean_call_insertion_callbacks, (void (*)(void))func, true);
}

void
dr_register_nudge_event(void (*func)(void *drcontext, uint64 argument), client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            add_callback(&client_libs[i].nudge_callbacks, (void (*)(void))func,
                         /* the nudge callback list is stored on the heap, so
                          * we don't need to unprotect the .data section when
                          * we update the list */
                         false);
            return;
        }
    }

    CLIENT_ASSERT(false, "dr_register_nudge_event: invalid client ID");
}

bool
dr_unregister_nudge_event(void (*func)(void *drcontext, uint64 argument), client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return remove_callback(&client_libs[i].nudge_callbacks, (void (*)(void))func,
                                   /* the nudge callback list is stored on the heap, so
                                    * we don't need to unprotect the .data section when
                                    * we update the list */
                                   false);
        }
    }

    CLIENT_ASSERT(false, "dr_unregister_nudge_event: invalid client ID");
    return false;
}

dr_config_status_t
dr_nudge_client_ex(process_id_t process_id, client_id_t client_id, uint64 argument,
                   uint timeout_ms)
{
    if (process_id == get_process_id()) {
        size_t i;
#ifdef WINDOWS
        pre_second_thread();
#endif
        for (i = 0; i < num_client_libs; i++) {
            if (client_libs[i].id == client_id) {
                if (client_libs[i].nudge_callbacks.num == 0) {
                    CLIENT_ASSERT(false, "dr_nudge_client: no nudge handler registered");
                    return false;
                }
                return nudge_internal(process_id, NUDGE_GENERIC(client), argument,
                                      client_id, timeout_ms);
            }
        }
        return false;
    } else {
        return nudge_internal(process_id, NUDGE_GENERIC(client), argument, client_id,
                              timeout_ms);
    }
}

bool
dr_nudge_client(client_id_t client_id, uint64 argument)
{
    return dr_nudge_client_ex(get_process_id(), client_id, argument, 0) == DR_SUCCESS;
}

#ifdef WINDOWS
DR_API
bool
dr_is_nudge_thread(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "invalid parameter to dr_is_nudge_thread");
    return dcontext->nudge_target != NULL;
}
#endif

void
instrument_client_thread_init(dcontext_t *dcontext, bool client_thread)
{
    if (dcontext->client_data == NULL) {
        /* We use PROTECTED partly to keep it local (unprotected "local" heap is
         * global and adds complexity to thread exit: i#271).
         */
        dcontext->client_data =
            HEAP_TYPE_ALLOC(dcontext, client_data_t, ACCT_OTHER, PROTECTED);
        memset(dcontext->client_data, 0x0, sizeof(client_data_t));

        ASSIGN_INIT_LOCK_FREE(dcontext->client_data->sideline_mutex, sideline_mutex);
        CLIENT_ASSERT(dynamo_initialized || thread_init_callbacks.num == 0 ||
                          client_thread,
                      "1st call to instrument_thread_init should have no cbs");
    }

    if (client_thread) {
        ATOMIC_INC(int, num_client_sideline_threads);
        /* We don't call dynamo_thread_not_under_dynamo() b/c we want itimers. */
        dcontext->thread_record->under_dynamo_control = false;
        dcontext->client_data->is_client_thread = true;
        dcontext->client_data->suspendable = true;
    }
}

void
instrument_thread_init(dcontext_t *dcontext, bool client_thread, bool valid_mc)
{
    /* Note that we're called twice for the initial thread: once prior
     * to instrument_init() (PR 216936) to set up the dcontext client
     * field (at which point there should be no callbacks since client
     * has not had a chance to register any) (now split out, but both
     * routines are called prior to instrument_init()), and once after
     * instrument_init() to call the client event.
     */
#ifdef WINDOWS
    bool swap_peb = false;
#endif

    if (client_thread) {
        /* no init event */
        return;
    }

#ifdef WINDOWS
    /* i#996: we might be in app's state.
     * It is simpler to check and swap here than earlier on thread init paths.
     */
    if (dr_using_app_state(dcontext)) {
        swap_peb_pointer(dcontext, true /*to priv*/);
        swap_peb = true;
    }
#endif

    /* i#117/PR 395156: support dr_get_mcontext() from the thread init event */
    if (valid_mc)
        dcontext->client_data->mcontext_in_dcontext = true;
    call_all(thread_init_callbacks, int (*)(void *), (void *)dcontext);
    if (valid_mc)
        dcontext->client_data->mcontext_in_dcontext = false;
#ifdef WINDOWS
    if (swap_peb)
        swap_peb_pointer(dcontext, false /*to app*/);
#endif
}

#ifdef UNIX
void
instrument_fork_init(dcontext_t *dcontext)
{
    call_all(fork_init_callbacks, int (*)(void *), (void *)dcontext);
}
#endif

void
instrument_low_on_memory()
{
    call_all(low_on_memory_callbacks, int (*)());
}

/* PR 536058: split the exit event from thread cleanup, to provide a
 * dcontext in the process exit event
 */
void
instrument_thread_exit_event(dcontext_t *dcontext)
{
    if (IS_CLIENT_THREAD(dcontext)
        /* if nudge thread calls dr_exit_process() it will be marked as a client
         * thread: rule it out here so we properly clean it up
         */
        IF_WINDOWS(&&dcontext->nudge_target == NULL)) {
        ATOMIC_DEC(int, num_client_sideline_threads);
        /* no exit event */
        return;
    }

    /* i#1394: best-effort to try to avoid crashing thread exit events
     * where thread init was never called.
     */
    if (!dynamo_initialized)
        return;

    /* support dr_get_mcontext() from the exit event */
    dcontext->client_data->mcontext_in_dcontext = true;
    /* Note - currently own initexit lock when this is called (see PR 227619). */
    call_all(thread_exit_callbacks, int (*)(void *), (void *)dcontext);
}

void
instrument_thread_exit(dcontext_t *dcontext)
{
#ifdef DEBUG
    client_todo_list_t *todo;
    client_flush_req_t *flush;
#endif

#ifdef DEBUG
    /* i#271: avoid racy crashes by not freeing in release build. */

    DELETE_LOCK(dcontext->client_data->sideline_mutex);

    /* could be heap space allocated for the todo list */
    todo = dcontext->client_data->to_do;
    while (todo != NULL) {
        client_todo_list_t *next_todo = todo->next;
        if (todo->ilist != NULL) {
            instrlist_clear_and_destroy(dcontext, todo->ilist);
        }
        HEAP_TYPE_FREE(dcontext, todo, client_todo_list_t, ACCT_CLIENT, PROTECTED);
        todo = next_todo;
    }

    /* could be heap space allocated for the flush list */
    flush = dcontext->client_data->flush_list;
    while (flush != NULL) {
        client_flush_req_t *next_flush = flush->next;
        HEAP_TYPE_FREE(dcontext, flush, client_flush_req_t, ACCT_CLIENT, PROTECTED);
        flush = next_flush;
    }

    HEAP_TYPE_FREE(dcontext, dcontext->client_data, client_data_t, ACCT_OTHER, PROTECTED);
    dcontext->client_data = NULL;              /* for mutex_wait_contended_lock() */
    dcontext->is_client_thread_exiting = true; /* for is_using_app_peb() */

#endif /* DEBUG */
}

bool
dr_bb_hook_exists(void)
{
    return (bb_callbacks.num > 0);
}

bool
dr_trace_hook_exists(void)
{
    return (trace_callbacks.num > 0);
}

bool
dr_fragment_deleted_hook_exists(void)
{
    return (fragdel_callbacks.num > 0);
}

bool
dr_end_trace_hook_exists(void)
{
    return (end_trace_callbacks.num > 0);
}

bool
dr_thread_exit_hook_exists(void)
{
    return (thread_exit_callbacks.num > 0);
}

bool
dr_exit_hook_exists(void)
{
    return (exit_callbacks.num > 0);
}

bool
dr_xl8_hook_exists(void)
{
    return (restore_state_callbacks.num > 0 || restore_state_ex_callbacks.num > 0);
}

bool
dr_modload_hook_exists(void)
{
    /* We do not support (as documented in the module event doxygen)
     * the client changing this during bb building, as that will mess
     * up USE_BB_BUILDING_LOCK_STEADY_STATE().
     */
    return module_load_callbacks.num > 0;
}

bool
hide_tag_from_client(app_pc tag)
{
#ifdef WINDOWS
    /* Case 10009: Basic blocks that consist of a single jump into the
     * interception buffer should be obscured from clients.  Clients
     * will see the displaced code, so we'll provide the address of this
     * block if the client asks for the address of the displaced code.
     *
     * Note that we assume the jump is the first instruction in the
     * BB for any blocks that jump to the interception buffer.
     */
    if (is_intercepted_app_pc(tag, NULL) ||
        /* Displaced app code is now in the landing pad, so skip the
         * jump from the interception buffer to the landing pad
         */
        is_in_interception_buffer(tag) ||
        /* Landing pads that exist between hook points and the trampolines
         * shouldn't be seen by the client too.  PR 250294.
         */
        is_on_interception_initial_route(tag) ||
        /* PR 219351: if we lose control on a callback and get it back on
         * one of our syscall trampolines, we'll appear at the jmp out of
         * the interception buffer to the int/sysenter instruction.  The
         * problem is that our syscall trampolines, unlike our other
         * intercepted code, are hooked earlier than the real action point
         * and we have displaced app code at the start of the interception
         * buffer: we hook at the wrapper entrance and return w/ a jmp to
         * the sysenter/int instr.  When creating bbs at the start we hack
         * it to make it look like there is no hook.  But on retaking control
         * we end up w/ this jmp out that won't be solved w/ our normal
         * mechanism for other hook jmp-outs: so we just suppress and the
         * client next sees the post-syscall bb.  It already saw a gap.
         */
        is_syscall_trampoline(tag, NULL))
        return true;
#endif
    return false;
}

#ifdef DEBUG
/* PR 214962: client must set translation fields */
static void
check_ilist_translations(instrlist_t *ilist)
{
    /* Ensure client set the translation field for all non-meta
     * instrs, even if it didn't return DR_EMIT_STORE_TRANSLATIONS
     * (since we may decide ourselves to store)
     */
    instr_t *in;
    for (in = instrlist_first(ilist); in != NULL; in = instr_get_next(in)) {
        if (!instr_opcode_valid(in)) {
            CLIENT_ASSERT(INTERNAL_OPTION(fast_client_decode), "level 0 instr found");
        } else if (instr_is_app(in)) {
            DOLOG(LOG_INTERP, 1, {
                if (instr_get_translation(in) == NULL) {
                    d_r_loginst(get_thread_private_dcontext(), 1, in,
                                "translation is NULL");
                }
            });
            CLIENT_ASSERT(instr_get_translation(in) != NULL,
                          "translation field must be set for every app instruction");
        } else {
            /* The meta instr could indeed not affect app state, but
             * better I think to assert and make them put in an
             * empty restore event callback in that case. */
            DOLOG(LOG_INTERP, 1, {
                if (instr_get_translation(in) != NULL && !instr_is_our_mangling(in) &&
                    !dr_xl8_hook_exists()) {
                    d_r_loginst(get_thread_private_dcontext(), 1, in,
                                "translation != NULL");
                }
            });
            CLIENT_ASSERT(instr_get_translation(in) == NULL ||
                              instr_is_our_mangling(in) || dr_xl8_hook_exists(),
                          /* FIXME: if multiple clients, we need to check that this
                           * particular client has the callback: but we have
                           * no way to do that other than looking at library
                           * bounds...punting for now */
                          "a meta instr should not have its translation field "
                          "set without also having a restore_state callback");
        }
    }
}
#endif

/* Returns true if the bb hook is called */
bool
instrument_basic_block(dcontext_t *dcontext, app_pc tag, instrlist_t *bb, bool for_trace,
                       bool translating, dr_emit_flags_t *emitflags)
{
    dr_emit_flags_t ret = DR_EMIT_DEFAULT;

    /* return false if no BB hooks are registered */
    if (bb_callbacks.num == 0)
        return false;

    if (hide_tag_from_client(tag)) {
        LOG(THREAD, LOG_INTERP, 3, "hiding tag " PFX " from client\n", tag);
        return false;
    }

    /* do not expand or up-decode the instrlist, client gets to choose
     * whether and how to do that
     */

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\ninstrument_basic_block ******************\n");
    LOG(THREAD, LOG_INTERP, 3, "\nbefore instrumentation:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, bb, THREAD);
#endif

    /* i#117/PR 395156: allow dr_[gs]et_mcontext where accurate */
    if (!translating && !for_trace)
        dcontext->client_data->mcontext_in_dcontext = true;

    /* Note - currently we are couldbelinking and hold the
     * bb_building lock when this is called (see PR 227619).
     */
    /* We or together the return values */
    call_all_ret(ret, |=, , bb_callbacks,
                 int (*)(void *, void *, instrlist_t *, bool, bool), (void *)dcontext,
                 (void *)tag, bb, for_trace, translating);
    if (emitflags != NULL)
        *emitflags = ret;
    DOCHECK(1, { check_ilist_translations(bb); });

    dcontext->client_data->mcontext_in_dcontext = false;

    if (IF_DEBUG_ELSE(for_trace, false)) {
        CLIENT_ASSERT(instrlist_get_return_target(bb) == NULL &&
                          instrlist_get_fall_through_target(bb) == NULL,
                      "instrlist_set_return/fall_through_target"
                      " cannot be used on traces");
    }

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\nafter instrumentation:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, bb, THREAD);
#endif

    return true;
}

/* Give the user the completely mangled and optimized trace just prior
 * to emitting into code cache, user gets final crack at it
 */
dr_emit_flags_t
instrument_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace, bool translating)
{
    dr_emit_flags_t ret = DR_EMIT_DEFAULT;
#ifdef UNSUPPORTED_API
    instr_t *instr;
#endif
    if (trace_callbacks.num == 0)
        return DR_EMIT_DEFAULT;

        /* do not expand or up-decode the instrlist, client gets to choose
         * whether and how to do that
         */

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\ninstrument_trace ******************\n");
    LOG(THREAD, LOG_INTERP, 3, "\nbefore instrumentation:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#endif

        /* We always pass Level 3 instrs to the client, since we no longer
         * expose the expansion routines.
         */
#ifdef UNSUPPORTED_API
    for (instr = instrlist_first_expanded(dcontext, trace); instr != NULL;
         instr = instr_get_next_expanded(dcontext, trace, instr)) {
        instr_decode(dcontext, instr);
    }
    /* ASSUMPTION: all ctis are already at Level 3, so we don't have
     * to do a separate pass to fix up intra-list targets like
     * instrlist_decode_cti() does
     */
#endif

    /* i#117/PR 395156: allow dr_[gs]et_mcontext where accurate */
    if (!translating)
        dcontext->client_data->mcontext_in_dcontext = true;

    /* We or together the return values */
    call_all_ret(ret, |=, , trace_callbacks, int (*)(void *, void *, instrlist_t *, bool),
                 (void *)dcontext, (void *)tag, trace, translating);

    DOCHECK(1, { check_ilist_translations(trace); });

    CLIENT_ASSERT(instrlist_get_return_target(trace) == NULL &&
                      instrlist_get_fall_through_target(trace) == NULL,
                  "instrlist_set_return/fall_through_target"
                  " cannot be used on traces");

    dcontext->client_data->mcontext_in_dcontext = false;

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\nafter instrumentation:\n");
    if (d_r_stats->loglevel >= 3 && (d_r_stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#endif

    return ret;
}

/* Notify user when a fragment is deleted from the cache
 * FIXME PR 242544: how does user know whether this is a shadowed copy or the
 * real thing?  The user might free memory that shouldn't be freed!
 */
void
instrument_fragment_deleted(dcontext_t *dcontext, app_pc tag, uint flags)
{
    if (fragdel_callbacks.num == 0)
        return;

#ifdef WINDOWS
    /* Case 10009: We don't call the basic block hook for blocks that
     * are jumps to the interception buffer, so we'll hide them here
     * as well.
     */
    if (!TEST(FRAG_IS_TRACE, flags) && hide_tag_from_client(tag))
        return;
#endif

    /* PR 243008: we don't expose GLOBAL_DCONTEXT, so change to NULL.
     * Our comments warn the user about this.
     */
    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = NULL;

    call_all(fragdel_callbacks, int (*)(void *, void *), (void *)dcontext, (void *)tag);
}

bool
instrument_restore_state(dcontext_t *dcontext, bool restore_memory,
                         dr_restore_state_info_t *info)
{
    bool res = true;
    /* Support both legacy and extended handlers */
    if (restore_state_callbacks.num > 0) {
        call_all(restore_state_callbacks,
                 int (*)(void *, void *, dr_mcontext_t *, bool, bool), (void *)dcontext,
                 info->fragment_info.tag, info->mcontext, restore_memory,
                 info->fragment_info.app_code_consistent);
    }
    if (restore_state_ex_callbacks.num > 0) {
        /* i#220/PR 480565: client has option of failing the translation.
         * We fail it if any client wants to, short-circuiting in that case.
         * This does violate the "priority order" of events where the
         * last one is supposed to have final say b/c it won't even
         * see the event (xref i#424).
         */
        call_all_ret(res, = res &&, , restore_state_ex_callbacks,
                     int (*)(void *, bool, dr_restore_state_info_t *), (void *)dcontext,
                     restore_memory, info);
    }
    CLIENT_ASSERT(!restore_memory || res,
                  "translation should not fail for restore_memory=true");
    return res;
}

/* The client may need to translate memory (e.g., DRWRAP_REPLACE_RETADDR using
 * sentinel addresses outside of the code cache as targets) even when the register
 * state already contains application values.
 */
bool
instrument_restore_nonfcache_state_prealloc(dcontext_t *dcontext, bool restore_memory,
                                            INOUT priv_mcontext_t *mcontext,
                                            OUT dr_mcontext_t *client_mcontext)
{
    if (!dr_xl8_hook_exists())
        return true;
    dr_restore_state_info_t client_info;
    dr_mcontext_init(client_mcontext);
    priv_mcontext_to_dr_mcontext(client_mcontext, mcontext);
    client_info.raw_mcontext = client_mcontext;
    client_info.raw_mcontext_valid = true;
    client_info.mcontext = client_mcontext;
    client_info.fragment_info.tag = NULL;
    client_info.fragment_info.cache_start_pc = NULL;
    client_info.fragment_info.is_trace = false;
    client_info.fragment_info.app_code_consistent = true;
    client_info.fragment_info.ilist = NULL;
    bool res = instrument_restore_state(dcontext, restore_memory, &client_info);
    dr_mcontext_to_priv_mcontext(mcontext, client_mcontext);
    return res;
}

/* The large dr_mcontext_t on the stack makes a difference, so we provide two
 * versions to avoid a double alloc on the same callstack.
 */
bool
instrument_restore_nonfcache_state(dcontext_t *dcontext, bool restore_memory,
                                   INOUT priv_mcontext_t *mcontext)
{
    dr_mcontext_t client_mcontext;
    return instrument_restore_nonfcache_state_prealloc(dcontext, restore_memory, mcontext,
                                                       &client_mcontext);
}

/* Ask whether to end trace prior to adding next_tag fragment.
 * Return values:
 *   CUSTOM_TRACE_DR_DECIDES = use standard termination criteria
 *   CUSTOM_TRACE_END_NOW    = end trace
 *   CUSTOM_TRACE_CONTINUE   = do not end trace
 */
dr_custom_trace_action_t
instrument_end_trace(dcontext_t *dcontext, app_pc trace_tag, app_pc next_tag)
{
    dr_custom_trace_action_t ret = CUSTOM_TRACE_DR_DECIDES;

    if (end_trace_callbacks.num == 0)
        return ret;

    /* Highest priority callback decides how to end the trace (see
     * call_all_ret implementation)
     */
    call_all_ret(ret, =, , end_trace_callbacks, int (*)(void *, void *, void *),
                 (void *)dcontext, (void *)trace_tag, (void *)next_tag);

    return ret;
}

/* Looks up module containing pc (assumed to be fully loaded).
 * If it exists and its client module load event has not been called, calls it.
 */
void
instrument_module_load_trigger(app_pc pc)
{
    if (CLIENTS_EXIST()) {
        module_area_t *ma;
        module_data_t *client_data = NULL;
        os_get_module_info_lock();
        ma = module_pc_lookup(pc);
        if (ma != NULL && !TEST(MODULE_LOAD_EVENT, ma->flags)) {
            /* switch to write lock */
            os_get_module_info_unlock();
#ifdef LINUX
            /* i#3385: re-try to initialize dynamic information, because
             * it failed during the first flat-mmap that loaded the module.
             * We don't perform this if there are no clients, assuming
             * DynamoRIO doesn't use os_module_data_t information itself.
             *
             * XXX: add a regression test later. See PR #5947 for how to
             * reproduce this situation.
             */
            if (!ma->os_data.have_dynamic_info) {
                os_module_update_dynamic_info(ma->start, ma->end - ma->start, false);
            }
#endif
            os_get_module_info_write_lock();
            ma = module_pc_lookup(pc);
            if (ma != NULL && !TEST(MODULE_LOAD_EVENT, ma->flags)) {
                ma->flags |= MODULE_LOAD_EVENT;
                client_data = copy_module_area_to_module_data(ma);
                os_get_module_info_write_unlock();
                instrument_module_load(client_data, true /*i#884: already loaded*/);
                dr_free_module_data(client_data);
            } else
                os_get_module_info_write_unlock();
        } else
            os_get_module_info_unlock();
    }
}

/* Notify user when a module is loaded */
void
instrument_module_load(module_data_t *data, bool previously_loaded)
{
    /* Note - during DR initialization this routine is called before we've set up a
     * dcontext for the main thread and before we've called instrument_init.  It's okay
     * since there's no way a callback will be registered and we'll return immediately. */
    dcontext_t *dcontext;

    if (module_load_callbacks.num == 0)
        return;

    dcontext = get_thread_private_dcontext();

    /* client shouldn't delete this */
    dcontext->client_data->no_delete_mod_data = data;

    call_all(module_load_callbacks, int (*)(void *, module_data_t *, bool),
             (void *)dcontext, data, previously_loaded);

    dcontext->client_data->no_delete_mod_data = NULL;
}

/* Notify user when a module is unloaded */
void
instrument_module_unload(module_data_t *data)
{
    dcontext_t *dcontext;

    if (module_unload_callbacks.num == 0)
        return;

    dcontext = get_thread_private_dcontext();
    /* client shouldn't delete this */
    dcontext->client_data->no_delete_mod_data = data;

    call_all(module_unload_callbacks, int (*)(void *, module_data_t *), (void *)dcontext,
             data);

    dcontext->client_data->no_delete_mod_data = NULL;
}

/* returns whether this sysnum should be intercepted */
bool
instrument_filter_syscall(dcontext_t *dcontext, int sysnum)
{
    bool ret = false;
    /* if client does not filter then we don't intercept anything */
    if (filter_syscall_callbacks.num == 0)
        return ret;
    /* if any client wants to intercept, then we intercept */
    call_all_ret(ret, =, || ret, filter_syscall_callbacks, bool (*)(void *, int),
                 (void *)dcontext, sysnum);
    return ret;
}

/* returns whether this syscall should execute */
bool
instrument_pre_syscall(dcontext_t *dcontext, int sysnum)
{
    bool exec = true;
    dcontext->client_data->in_pre_syscall = true;
    /* clear flag from dr_syscall_invoke_another() */
    dcontext->client_data->invoke_another_syscall = false;
    if (pre_syscall_callbacks.num > 0) {
        dr_where_am_i_t old_whereami = dcontext->whereami;
        dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;
        DODEBUG({
            /* Avoid the common mistake of forgetting a filter event. */
            CLIENT_ASSERT(filter_syscall_callbacks.num > 0,
                          "A filter event must be "
                          "provided when using pre- and post-syscall events");
        });
        /* Skip syscall if any client wants to skip it, but don't short-circuit,
         * as skipping syscalls is usually done when the effect of the syscall
         * will be emulated in some other way.  The app is typically meant to
         * think that the syscall succeeded.  Thus, other tool components
         * should see the syscall as well (xref i#424).
         */
        call_all_ret(exec, =, &&exec, pre_syscall_callbacks, bool (*)(void *, int),
                     (void *)dcontext, sysnum);
        dcontext->whereami = old_whereami;
    }
    dcontext->client_data->in_pre_syscall = false;
    return exec;
}

void
instrument_post_syscall(dcontext_t *dcontext, int sysnum)
{
    dr_where_am_i_t old_whereami = dcontext->whereami;
    if (post_syscall_callbacks.num == 0)
        return;
    DODEBUG({
        /* Avoid the common mistake of forgetting a filter event. */
        CLIENT_ASSERT(filter_syscall_callbacks.num > 0,
                      "A filter event must be "
                      "provided when using pre- and post-syscall events");
    });
    dcontext->whereami = DR_WHERE_SYSCALL_HANDLER;
    dcontext->client_data->in_post_syscall = true;
    call_all(post_syscall_callbacks, int (*)(void *, int), (void *)dcontext, sysnum);
    dcontext->client_data->in_post_syscall = false;
    dcontext->whereami = old_whereami;
}

bool
instrument_invoke_another_syscall(dcontext_t *dcontext)
{
    return dcontext->client_data->invoke_another_syscall;
}

bool
instrument_kernel_xfer(dcontext_t *dcontext, dr_kernel_xfer_type_t type,
                       os_cxt_ptr_t source_os_cxt, dr_mcontext_t *source_dmc,
                       priv_mcontext_t *source_mc, app_pc target_pc, reg_t target_xsp,
                       os_cxt_ptr_t target_os_cxt, priv_mcontext_t *target_mc, int sig)
{
    if (kernel_xfer_callbacks.num == 0) {
        return false;
    }
    LOG(THREAD, LOG_INTERP, 3, "%s: type=%d\n", __FUNCTION__, type);
    dr_kernel_xfer_info_t info;
    info.type = type;
    info.source_mcontext = NULL;
    info.target_pc = target_pc;
    info.target_xsp = target_xsp;
    info.sig = sig;
    dr_mcontext_t dr_mcontext;
    dr_mcontext.size = sizeof(dr_mcontext);
    dr_mcontext.flags = DR_MC_CONTROL | DR_MC_INTEGER;

    if (source_dmc != NULL)
        info.source_mcontext = source_dmc;
    else if (source_mc != NULL) {
        if (priv_mcontext_to_dr_mcontext(&dr_mcontext, source_mc))
            info.source_mcontext = &dr_mcontext;
    } else if (!is_os_cxt_ptr_null(source_os_cxt)) {
        if (os_context_to_mcontext(&dr_mcontext, NULL, source_os_cxt))
            info.source_mcontext = &dr_mcontext;
    }
    /* Our compromise to reduce context copying is to provide the PC and XSP inline,
     * and only get more if the user calls dr_get_mcontext(), which we support again
     * without any copying if not used by taking in a raw os_context_t.
     */
    dcontext->client_data->os_cxt = target_os_cxt;
    dcontext->client_data->cur_mc = target_mc;
    call_all(kernel_xfer_callbacks, int (*)(void *, const dr_kernel_xfer_info_t *),
             (void *)dcontext, &info);
    set_os_cxt_ptr_null(&dcontext->client_data->os_cxt);
    dcontext->client_data->cur_mc = NULL;
    return true;
}

#ifdef WINDOWS
/* Notify user of exceptions.  Note: not called for RaiseException */
bool
instrument_exception(dcontext_t *dcontext, dr_exception_t *exception)
{
    bool res = true;
    /* Ensure that dr_get_mcontext() called from instrument_kernel_xfer() from
     * dr_redirect_execution() will get the source context.
     * cur_mc will later be clobbered by instrument_kernel_xfer() which is ok:
     * the redirect ends the callback calling.
     */
    dcontext->client_data->cur_mc = dr_mcontext_as_priv_mcontext(exception->mcontext);
    /* We short-circuit if any client wants to "own" the fault and not pass on.
     * This does violate the "priority order" of events where the last one is
     * supposed to have final say b/c it won't even see the event: but only one
     * registrant should own it (xref i#424).
     */
    call_all_ret(res, = res &&, , exception_callbacks, bool (*)(void *, dr_exception_t *),
                 (void *)dcontext, exception);
    dcontext->client_data->cur_mc = NULL;
    return res;
}
#else
dr_signal_action_t
instrument_signal(dcontext_t *dcontext, dr_siginfo_t *siginfo)
{
    dr_signal_action_t ret = DR_SIGNAL_DELIVER;
    /* We short-circuit if any client wants to do other than deliver to the app.
     * This does violate the "priority order" of events where the last one is
     * supposed to have final say b/c it won't even see the event: but only one
     * registrant should own the signal (xref i#424).
     */
    call_all_ret(ret, = ret == DR_SIGNAL_DELIVER ?, : ret, signal_callbacks,
                 dr_signal_action_t(*)(void *, dr_siginfo_t *), (void *)dcontext,
                 siginfo);
    return ret;
}

bool
dr_signal_hook_exists(void)
{
    return (signal_callbacks.num > 0);
}
#endif /* WINDOWS */

#ifdef PROGRAM_SHEPHERDING
/* Notify user when a security violation is detected */
void
instrument_security_violation(dcontext_t *dcontext, app_pc target_pc,
                              security_violation_t violation, action_type_t *action)
{
    dr_security_violation_type_t dr_violation;
    dr_security_violation_action_t dr_action, dr_action_original;
    app_pc source_pc = NULL;
    fragment_t *last;
    dr_mcontext_t dr_mcontext;
    dr_mcontext_init(&dr_mcontext);

    if (security_violation_callbacks.num == 0)
        return;

    if (!priv_mcontext_to_dr_mcontext(&dr_mcontext, get_mcontext(dcontext)))
        return;

    /* FIXME - the source_tag, source_pc, and context can all be incorrect if the
     * violation ends up occurring in the middle of a bb we're building.  See case
     * 7380 which we should fix in interp.c.
     */

    /* Obtain the source addr to pass to the client.  xref case 285 --
     * we're using the more heavy-weight solution 2) here, but that
     * should be okay since we already have the overhead of calling
     * into the client. */
    last = dcontext->last_fragment;
    if (!TEST(FRAG_FAKE, last->flags)) {
        cache_pc pc = EXIT_CTI_PC(last, dcontext->last_exit);
        source_pc = recreate_app_pc(dcontext, pc, last);
    }
    /* FIXME - set pc field of dr_mcontext_t.  We'll probably want it
     * for thread start and possibly apc/callback events as well.
     */

    switch (violation) {
    case STACK_EXECUTION_VIOLATION: dr_violation = DR_RCO_STACK_VIOLATION; break;
    case HEAP_EXECUTION_VIOLATION: dr_violation = DR_RCO_HEAP_VIOLATION; break;
    case RETURN_TARGET_VIOLATION: dr_violation = DR_RCT_RETURN_VIOLATION; break;
    case RETURN_DIRECT_RCT_VIOLATION:
        ASSERT(false); /* Not a client fault, should be NOT_REACHED(). */
        dr_violation = DR_UNKNOWN_VIOLATION;
        break;
    case INDIRECT_CALL_RCT_VIOLATION:
        dr_violation = DR_RCT_INDIRECT_CALL_VIOLATION;
        break;
    case INDIRECT_JUMP_RCT_VIOLATION:
        dr_violation = DR_RCT_INDIRECT_JUMP_VIOLATION;
        break;
    default:
        ASSERT(false); /* Not a client fault, should be NOT_REACHED(). */
        dr_violation = DR_UNKNOWN_VIOLATION;
        break;
    }

    switch (*action) {
    case ACTION_TERMINATE_PROCESS: dr_action = DR_VIOLATION_ACTION_KILL_PROCESS; break;
    case ACTION_CONTINUE: dr_action = DR_VIOLATION_ACTION_CONTINUE; break;
    case ACTION_TERMINATE_THREAD: dr_action = DR_VIOLATION_ACTION_KILL_THREAD; break;
    case ACTION_THROW_EXCEPTION: dr_action = DR_VIOLATION_ACTION_THROW_EXCEPTION; break;
    default:
        ASSERT(false); /* Not a client fault, should be NOT_REACHED(). */
        dr_action = DR_VIOLATION_ACTION_CONTINUE;
        break;
    }
    dr_action_original = dr_action;

    /* NOTE - last->tag should be valid here (even if the frag is fake since the
     * coarse wrappers set the tag).  FIXME - for traces we really want the bb tag not
     * the trace tag, should get that. Of course the only real reason we pass source
     * tag is because we can't always give a valid source_pc. */

    /* Note that the last registered function gets the final crack at
     * changing the action.
     */
    call_all(security_violation_callbacks,
             int (*)(void *, void *, app_pc, app_pc, dr_security_violation_type_t,
                     dr_mcontext_t *, dr_security_violation_action_t *),
             (void *)dcontext, last->tag, source_pc, target_pc, dr_violation,
             &dr_mcontext, &dr_action);

    if (dr_action != dr_action_original) {
        switch (dr_action) {
        case DR_VIOLATION_ACTION_KILL_PROCESS: *action = ACTION_TERMINATE_PROCESS; break;
        case DR_VIOLATION_ACTION_KILL_THREAD: *action = ACTION_TERMINATE_THREAD; break;
        case DR_VIOLATION_ACTION_THROW_EXCEPTION: *action = ACTION_THROW_EXCEPTION; break;
        case DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT:
            /* FIXME - not safe to implement till case 7380 is fixed. */
            CLIENT_ASSERT(false,
                          "action DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT "
                          "not yet supported.");
            /* note - no break, fall through */
        case DR_VIOLATION_ACTION_CONTINUE: *action = ACTION_CONTINUE; break;
        default:
            CLIENT_ASSERT(false,
                          "Security violation event callback returned invalid "
                          "action value.");
        }
    }
}
#endif

/* Notify the client of a nudge. */
void
instrument_nudge(dcontext_t *dcontext, client_id_t id, uint64 arg)
{
    size_t i;

    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT &&
           dcontext == get_thread_private_dcontext());
    /* synch_with_all_threads and flush API assume that client nudge threads
     * hold no dr locks and are !couldbelinking while in client lib code */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(!is_couldbelinking(dcontext));

    /* find the client the nudge is intended for */
    for (i = 0; i < num_client_libs; i++) {
        /* until we have nudge-arg support (PR 477454), nudges target the 1st client */
        if (IF_VMX86_ELSE(true, client_libs[i].id == id)) {
            break;
        }
    }

    if (i == num_client_libs || client_libs[i].nudge_callbacks.num == 0)
        return;

#ifdef WINDOWS
    /* count the number of nudge events so we can make sure they're
     * all finished before exiting
     */
    d_r_mutex_lock(&client_thread_count_lock);
    if (block_client_nudge_threads) {
        /* FIXME - would be nice if there was a way to let the external agent know that
         * the nudge event wasn't delivered (but this only happens when the process
         * is detaching or exiting). */
        d_r_mutex_unlock(&client_thread_count_lock);
        return;
    }

    /* atomic to avoid locking around the dec */
    ATOMIC_INC(int, num_client_nudge_threads);
    d_r_mutex_unlock(&client_thread_count_lock);

    /* We need to mark this as a client controlled thread for synch_with_all_threads
     * and otherwise treat it as native.  Xref PR 230836 on what to do if this
     * thread hits native_exec_syscalls hooks.
     * XXX: this requires extra checks for "not a nudge thread" after IS_CLIENT_THREAD
     * in get_stack_bounds() and instrument_thread_exit_event(): maybe better
     * to have synchall checks do extra checks and have IS_CLIENT_THREAD be
     * false for nudge threads at exit time?
     */
    dcontext->client_data->is_client_thread = true;
    dcontext->thread_record->under_dynamo_control = false;
#else
    /* support calling dr_get_mcontext() on this thread.  the app
     * context should be intact in the current mcontext except
     * pc which we set from next_tag.
     */
    CLIENT_ASSERT(!dcontext->client_data->mcontext_in_dcontext,
                  "internal inconsistency in where mcontext is");
    dcontext->client_data->mcontext_in_dcontext = true;
    /* officially get_mcontext() doesn't always set pc: we do anyway */
    get_mcontext(dcontext)->pc = dcontext->next_tag;
#endif

    call_all(client_libs[i].nudge_callbacks, int (*)(void *, uint64), (void *)dcontext,
             arg);

#ifdef UNIX
    dcontext->client_data->mcontext_in_dcontext = false;
#else
    dcontext->thread_record->under_dynamo_control = true;
    dcontext->client_data->is_client_thread = false;

    ATOMIC_DEC(int, num_client_nudge_threads);
#endif
}

int
get_num_client_threads(void)
{
    int num = IF_WINDOWS_ELSE(num_client_nudge_threads, 0);
    num += num_client_sideline_threads;
    return num;
}

#ifdef WINDOWS
/* wait for all nudges to finish */
void
wait_for_outstanding_nudges()
{
    /* block any new nudge threads from starting */
    d_r_mutex_lock(&client_thread_count_lock);
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    block_client_nudge_threads = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    DOLOG(1, LOG_TOP, {
        if (num_client_nudge_threads > 0) {
            LOG(GLOBAL, LOG_TOP, 1,
                "Waiting for %d nudges to finish - app is about to kill all threads "
                "except the current one.\n",
                num_client_nudge_threads);
        }
    });

    /* don't wait if the client requested exit: after all the client might
     * have done so from a nudge, and if the client does want to exit it's
     * its own problem if it misses nudges (and external nudgers should use
     * a finite timeout)
     */
    if (client_requested_exit) {
        d_r_mutex_unlock(&client_thread_count_lock);
        return;
    }

    while (num_client_nudge_threads > 0) {
        /* yield with lock released to allow nudges to finish */
        d_r_mutex_unlock(&client_thread_count_lock);
        dr_thread_yield();
        d_r_mutex_lock(&client_thread_count_lock);
    }
    d_r_mutex_unlock(&client_thread_count_lock);
}
#endif /* WINDOWS */

/****************************************************************************/
/* EXPORTED ROUTINES */

DR_API
/* Creates a DR context that can be used in a standalone program.
 * WARNING: this context cannot be used as the drcontext for a thread
 * running under DR control!  It is only for standalone programs that
 * wish to use DR as a library of disassembly, etc. routines.
 */
void *
dr_standalone_init(void)
{
    dcontext_t *dcontext = standalone_init();
    return (void *)dcontext;
}

DR_API
void
dr_standalone_exit(void)
{
    standalone_exit();
}

DR_API
/* Aborts the process immediately */
void
dr_abort(void)
{
    if (TEST(DUMPCORE_DR_ABORT, dynamo_options.dumpcore_mask))
        os_dump_core("dr_abort");
    os_terminate(NULL, TERMINATE_PROCESS);
}

DR_API
void
dr_abort_with_code(int exit_code)
{
    if (TEST(DUMPCORE_DR_ABORT, dynamo_options.dumpcore_mask))
        os_dump_core("dr_abort");
    os_terminate_with_code(NULL, TERMINATE_PROCESS, exit_code);
}

DR_API
void
dr_exit_process(int exit_code)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    /* Prevent cleanup from waiting for nudges as this may be called
     * from a nudge!
     * Also suppress leak asserts, as it's hard to clean up from
     * some situations (such as DrMem -crash_at_error).
     */
    client_requested_exit = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
#ifdef WINDOWS
    if (dcontext != NULL && dcontext->nudge_target != NULL) {
        /* we need to free the nudge thread stack which may involved
         * switching stacks so we have the nudge thread invoke
         * os_terminate for us
         */
        nudge_thread_cleanup(dcontext, true /*kill process*/, exit_code);
        CLIENT_ASSERT(false, "shouldn't get here");
    }
#endif
    if (!is_currently_on_dstack(dcontext)
            IF_UNIX(&&!is_currently_on_sigaltstack(dcontext))) {
        /* if on app stack or sigaltstack, avoid incorrect leak assert at exit */
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
        dr_api_exit = true;
        SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT); /* to keep properly nested */
    }
    os_terminate_with_code(dcontext, /* dcontext is required */
                           TERMINATE_CLEANUP | TERMINATE_PROCESS, exit_code);
    CLIENT_ASSERT(false, "shouldn't get here");
}

DR_API
bool
dr_create_memory_dump(dr_memory_dump_spec_t *spec)
{
    if (spec->size != sizeof(dr_memory_dump_spec_t))
        return false;
#ifdef WINDOWS
    if (TEST(DR_MEMORY_DUMP_LDMP, spec->flags))
        return os_dump_core_live(spec->label, spec->ldmp_path, spec->ldmp_path_size);
#endif
    return false;
}

DR_API
/* Returns true if all DynamoRIO caches are thread private. */
bool
dr_using_all_private_caches(void)
{
    return !SHARED_FRAGMENTS_ENABLED();
}

DR_API
void
dr_request_synchronized_exit(void)
{
    SYSLOG_INTERNAL_WARNING_ONCE("dr_request_synchronized_exit deprecated: "
                                 "use dr_set_process_exit_behavior instead");
}

DR_API
void
dr_set_process_exit_behavior(dr_exit_flags_t flags)
{
    if ((!DYNAMO_OPTION(multi_thread_exit) && TEST(DR_EXIT_MULTI_THREAD, flags)) ||
        (DYNAMO_OPTION(multi_thread_exit) && !TEST(DR_EXIT_MULTI_THREAD, flags))) {
        options_make_writable();
        dynamo_options.multi_thread_exit = TEST(DR_EXIT_MULTI_THREAD, flags);
        options_restore_readonly();
    }
    if ((!DYNAMO_OPTION(skip_thread_exit_at_exit) &&
         TEST(DR_EXIT_SKIP_THREAD_EXIT, flags)) ||
        (DYNAMO_OPTION(skip_thread_exit_at_exit) &&
         !TEST(DR_EXIT_SKIP_THREAD_EXIT, flags))) {
        options_make_writable();
        dynamo_options.skip_thread_exit_at_exit = TEST(DR_EXIT_SKIP_THREAD_EXIT, flags);
        options_restore_readonly();
    }
}

void
dr_allow_unsafe_static_behavior(void)
{
    loader_allow_unsafe_static_behavior();
}

DR_API
/* Returns the option string passed along with a client path via DR's
 * -client_lib option.
 */
/* i#1736: we now token-delimit with quotes, but for backward compat we need to
 * pass a version w/o quotes for dr_get_options().
 */
const char *
dr_get_options(client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            /* If we already converted, pass the result */
            if (client_libs[i].legacy_options[0] != '\0' ||
                client_libs[i].options[0] == '\0')
                return client_libs[i].legacy_options;
            /* For backward compatibility, we need to remove the token-delimiting
             * quotes.  We tokenize, and then re-assemble the flat string.
             * i#1755: however, for legacy custom frontends that are not re-quoting
             * like drrun now is, we need to avoid removing any quotes from the
             * original strings.  We try to detect this by assuming a frontend will
             * either re-quote everything or nothing.  Ideally we would check all
             * args, but that would require plumbing info from getword() or
             * duplicating its functionality: so instead our heuristic is just checking
             * the first and last chars.
             */
            if (!char_is_quote(client_libs[i].options[0]) ||
                /* Emptry string already detected above */
                !char_is_quote(
                    client_libs[i].options[strlen(client_libs[i].options) - 1])) {
                /* At least one arg is not quoted => better use original */
                snprintf(client_libs[i].legacy_options,
                         BUFFER_SIZE_ELEMENTS(client_libs[i].legacy_options), "%s",
                         client_libs[i].options);
            } else {
                int j;
                size_t sofar = 0;
                for (j = 1 /*skip client lib*/; j < client_libs[i].argc; j++) {
                    if (!print_to_buffer(
                            client_libs[i].legacy_options,
                            BUFFER_SIZE_ELEMENTS(client_libs[i].legacy_options), &sofar,
                            "%s%s", (j == 1) ? "" : " ", client_libs[i].argv[j]))
                        break;
                }
            }
            NULL_TERMINATE_BUFFER(client_libs[i].legacy_options);
            return client_libs[i].legacy_options;
        }
    }
    CLIENT_ASSERT(false, "dr_get_options(): invalid client id");
    return NULL;
}

DR_API
bool
dr_get_option_array(client_id_t id, int *argc OUT, const char ***argv OUT)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            *argc = client_libs[i].argc;
            *argv = client_libs[i].argv;
            return true;
        }
    }
    CLIENT_ASSERT(false, "dr_get_option_array(): invalid client id");
    return false;
}

DR_API
/* Returns the path to the client library.  Client must pass its ID */
const char *
dr_get_client_path(client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return client_libs[i].path;
        }
    }

    CLIENT_ASSERT(false, "dr_get_client_path(): invalid client id");
    return NULL;
}

DR_API
byte *
dr_get_client_base(client_id_t id)
{
    size_t i;
    for (i = 0; i < num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return client_libs[i].start;
        }
    }

    CLIENT_ASSERT(false, "dr_get_client_base(): invalid client id");
    return NULL;
}

DR_API
bool
dr_set_client_name(const char *name, const char *report_URL)
{
    /* Although set_exception_strings() accepts NULL, clients should pass real vals. */
    if (name == NULL || report_URL == NULL)
        return false;
    set_exception_strings(name, report_URL);
    return true;
}

bool
dr_set_client_version_string(const char *version)
{
    if (version == NULL)
        return false;
    set_display_version(version);
    return true;
}

DR_API const char *
dr_get_application_name(void)
{
#ifdef UNIX
    return get_application_short_name();
#else
    return get_application_short_unqualified_name();
#endif
}

void
set_client_error_code(dcontext_t *dcontext, dr_error_code_t error_code)
{
    if (dcontext == NULL || dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();

    dcontext->client_data->error_code = error_code;
}

dr_error_code_t
dr_get_error_code(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;

    if (dcontext == GLOBAL_DCONTEXT)
        dcontext = get_thread_private_dcontext();

    CLIENT_ASSERT(dcontext != NULL, "invalid drcontext");
    return dcontext->client_data->error_code;
}

int
dr_num_app_args(void)
{
    /* XXX i#2662: Add support for Windows. */
    return num_app_args();
}

int
dr_get_app_args(OUT dr_app_arg_t *args_array, int args_count)
{
    /* XXX i#2662: Add support for Windows. */
    return get_app_args(args_array, (int)args_count);
}

const char *
dr_app_arg_as_cstring(IN dr_app_arg_t *app_arg, char *buf, int buf_size)
{
    if (app_arg == NULL) {
        set_client_error_code(NULL, DR_ERROR_INVALID_PARAMETER);
        return NULL;
    }

    switch (app_arg->encoding) {
    case DR_APP_ARG_CSTR_COMPAT: return (char *)app_arg->start;
    case DR_APP_ARG_UTF_16:
        /* XXX i#2662: To implement using utf16_to_utf8(). */
        ASSERT_NOT_IMPLEMENTED(false);
        set_client_error_code(NULL, DR_ERROR_NOT_IMPLEMENTED);
        break;
    default: set_client_error_code(NULL, DR_ERROR_UNKNOWN_ENCODING);
    }

    return NULL;
}

DR_API process_id_t
dr_get_process_id(void)
{
    return (process_id_t)get_process_id();
}

DR_API process_id_t
dr_get_process_id_from_drcontext(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_get_process_id_from_drcontext: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_get_process_id_from_drcontext: drcontext is invalid");
#ifdef UNIX
    return dcontext->owning_process;
#else
    return dr_get_process_id();
#endif
}

#ifdef UNIX
DR_API
process_id_t
dr_get_parent_id(void)
{
    return get_parent_id();
}
#endif

#ifdef WINDOWS
DR_API
process_id_t
dr_convert_handle_to_pid(HANDLE process_handle)
{
    ASSERT(POINTER_MAX == INVALID_PROCESS_ID);
    return process_id_from_handle(process_handle);
}

DR_API
HANDLE
dr_convert_pid_to_handle(process_id_t pid)
{
    return process_handle_from_id(pid);
}

DR_API
/**
 * Returns information about the version of the operating system.
 * Returns whether successful.
 */
bool
dr_get_os_version(dr_os_version_info_t *info)
{
    int ver;
    uint sp_major, sp_minor, build_number;
    const char *release_id, *edition;
    get_os_version_ex(&ver, &sp_major, &sp_minor, &build_number, &release_id, &edition);
    if (info->size > offsetof(dr_os_version_info_t, version)) {
        switch (ver) {
        case WINDOWS_VERSION_10_1803: info->version = DR_WINDOWS_VERSION_10_1803; break;
        case WINDOWS_VERSION_10_1709: info->version = DR_WINDOWS_VERSION_10_1709; break;
        case WINDOWS_VERSION_10_1703: info->version = DR_WINDOWS_VERSION_10_1703; break;
        case WINDOWS_VERSION_10_1607: info->version = DR_WINDOWS_VERSION_10_1607; break;
        case WINDOWS_VERSION_10_1511: info->version = DR_WINDOWS_VERSION_10_1511; break;
        case WINDOWS_VERSION_10: info->version = DR_WINDOWS_VERSION_10; break;
        case WINDOWS_VERSION_8_1: info->version = DR_WINDOWS_VERSION_8_1; break;
        case WINDOWS_VERSION_8: info->version = DR_WINDOWS_VERSION_8; break;
        case WINDOWS_VERSION_7: info->version = DR_WINDOWS_VERSION_7; break;
        case WINDOWS_VERSION_VISTA: info->version = DR_WINDOWS_VERSION_VISTA; break;
        case WINDOWS_VERSION_2003: info->version = DR_WINDOWS_VERSION_2003; break;
        case WINDOWS_VERSION_XP: info->version = DR_WINDOWS_VERSION_XP; break;
        case WINDOWS_VERSION_2000: info->version = DR_WINDOWS_VERSION_2000; break;
        case WINDOWS_VERSION_NT: info->version = DR_WINDOWS_VERSION_NT; break;
        default: CLIENT_ASSERT(false, "unsupported windows version");
        };
    } else
        return false; /* struct too small for any info */
    if (info->size > offsetof(dr_os_version_info_t, service_pack_major)) {
        info->service_pack_major = sp_major;
        if (info->size > offsetof(dr_os_version_info_t, service_pack_minor)) {
            info->service_pack_minor = sp_minor;
        }
    }
    if (info->size > offsetof(dr_os_version_info_t, build_number)) {
        info->build_number = build_number;
    }
    if (info->size > offsetof(dr_os_version_info_t, release_id)) {
        dr_snprintf(info->release_id, BUFFER_SIZE_ELEMENTS(info->release_id), "%s",
                    release_id);
        NULL_TERMINATE_BUFFER(info->release_id);
    }
    if (info->size > offsetof(dr_os_version_info_t, edition)) {
        dr_snprintf(info->edition, BUFFER_SIZE_ELEMENTS(info->edition), "%s", edition);
        NULL_TERMINATE_BUFFER(info->edition);
    }
    return true;
}

DR_API
bool
dr_is_wow64(void)
{
    return is_wow64_process(NT_CURRENT_PROCESS);
}

DR_API
void *
dr_get_app_PEB(void)
{
    return get_own_peb();
}
#endif

DR_API
/* Retrieves the current time */
void
dr_get_time(dr_time_t *time)
{
    convert_millis_to_date(query_time_millis(), time);
}

DR_API
uint64
dr_get_milliseconds(void)
{
    return query_time_millis();
}

DR_API
uint64
dr_get_microseconds(void)
{
    return query_time_micros();
}

DR_API
uint
dr_get_random_value(uint max)
{
    return (uint)get_random_offset(max);
}

DR_API
void
dr_set_random_seed(uint seed)
{
    d_r_set_random_seed(seed);
}

DR_API
uint
dr_get_random_seed(void)
{
    return d_r_get_random_seed();
}

/***************************************************************************
 * MEMORY ALLOCATION
 */

DR_API
/* Allocates memory from DR's memory pool specific to the
 * thread associated with drcontext.
 */
void *
dr_thread_alloc(void *drcontext, size_t size)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    /* For back-compat this is guaranteed-reachable. */
    return heap_reachable_alloc(dcontext, size HEAPACCT(ACCT_CLIENT));
}

DR_API
/* Frees thread-specific memory allocated by dr_thread_alloc.
 * size must be the same size passed to dr_thread_alloc.
 */
void
dr_thread_free(void *drcontext, void *mem, size_t size)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_thread_free: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_thread_free: drcontext is invalid");
    heap_reachable_free(dcontext, mem, size HEAPACCT(ACCT_CLIENT));
}

DR_API
/* Allocates memory from DR's global memory pool.
 */
void *
dr_global_alloc(size_t size)
{
    /* For back-compat this is guaranteed-reachable. */
    return heap_reachable_alloc(GLOBAL_DCONTEXT, size HEAPACCT(ACCT_CLIENT));
}

DR_API
/* Frees memory allocated by dr_global_alloc.
 * size must be the same size passed to dr_global_alloc.
 */
void
dr_global_free(void *mem, size_t size)
{
    heap_reachable_free(GLOBAL_DCONTEXT, mem, size HEAPACCT(ACCT_CLIENT));
}

DR_API
/* PR 352427: API routine to allocate executable memory */
void *
dr_nonheap_alloc(size_t size, uint prot)
{
    CLIENT_ASSERT(
        !TESTALL(DR_MEMPROT_WRITE | DR_MEMPROT_EXEC, prot) ||
            !DYNAMO_OPTION(satisfy_w_xor_x),
        "reachable executable client memory is not supported with -satisfy_w_xor_x");
    return heap_mmap_ex(size, size, prot, false /*no guard pages*/,
                        /* For back-compat we preserve reachability. */
                        VMM_SPECIAL_MMAP | VMM_REACHABLE);
}

DR_API
void
dr_nonheap_free(void *mem, size_t size)
{
    heap_munmap_ex(mem, size, false /*no guard pages*/, VMM_SPECIAL_MMAP | VMM_REACHABLE);
}

static void *
raw_mem_alloc(size_t size, uint prot, void *addr, dr_alloc_flags_t flags)
{
    byte *p;
    heap_error_code_t error_code;

    CLIENT_ASSERT(ALIGNED(addr, PAGE_SIZE), "addr is not page size aligned");
    if (!TEST(DR_ALLOC_NON_DR, flags)) {
        /* memory alloc/dealloc and updating DR list must be atomic */
        dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    }
    addr = (void *)ALIGN_BACKWARD(addr, PAGE_SIZE);
    size = ALIGN_FORWARD(size, PAGE_SIZE);
#ifdef WINDOWS
    if (TEST(DR_ALLOC_LOW_2GB, flags)) {
        CLIENT_ASSERT(!TEST(DR_ALLOC_COMMIT_ONLY, flags),
                      "cannot combine commit-only and low-2GB");
        p = os_heap_reserve_in_region(NULL, (byte *)(ptr_uint_t)0x80000000, size,
                                      &error_code, TEST(DR_MEMPROT_EXEC, flags));
        if (p != NULL && !TEST(DR_ALLOC_RESERVE_ONLY, flags)) {
            if (!os_heap_commit(p, size, prot, &error_code)) {
                os_heap_free(p, size, &error_code);
                p = NULL;
            }
        }
    } else
#endif
    {
        /* We specify that DR_ALLOC_LOW_2GB only applies to x64, so it's
         * ok that the Linux kernel will ignore MAP_32BIT for 32-bit.
         */
#ifdef UNIX
        uint os_flags = TEST(DR_ALLOC_LOW_2GB, flags) ? RAW_ALLOC_32BIT : 0;
#else
        uint os_flags = TEST(DR_ALLOC_RESERVE_ONLY, flags)
            ? RAW_ALLOC_RESERVE_ONLY
            : (TEST(DR_ALLOC_COMMIT_ONLY, flags) ? RAW_ALLOC_COMMIT_ONLY : 0);
#endif
        if (IF_WINDOWS(TEST(DR_ALLOC_COMMIT_ONLY, flags) &&) addr != NULL &&
            !app_memory_pre_alloc(get_thread_private_dcontext(), addr, size, prot, false,
                                  true /*update*/, false /*!image*/)) {
            p = NULL;
        } else {
            p = os_raw_mem_alloc(addr, size, prot, os_flags, &error_code);
        }
    }

    if (p != NULL) {
        if (TEST(DR_ALLOC_NON_DR, flags)) {
            all_memory_areas_lock();
            update_all_memory_areas(p, p + size, prot, DR_MEMTYPE_DATA);
            all_memory_areas_unlock();
        } else {
            /* this routine updates allmem for us: */
            add_dynamo_vm_area((app_pc)p, ((app_pc)p) + size, prot,
                               true _IF_DEBUG("fls cb in private lib"));
        }
        RSTATS_ADD_PEAK(client_raw_mmap_size, size);
    }
    if (!TEST(DR_ALLOC_NON_DR, flags))
        dynamo_vm_areas_unlock();
    return p;
}

static bool
raw_mem_free(void *addr, size_t size, dr_alloc_flags_t flags)
{
    bool res;
    heap_error_code_t error_code;
    byte *p = addr;
#ifdef UNIX
    uint os_flags = TEST(DR_ALLOC_LOW_2GB, flags) ? RAW_ALLOC_32BIT : 0;
#else
    uint os_flags = TEST(DR_ALLOC_RESERVE_ONLY, flags)
        ? RAW_ALLOC_RESERVE_ONLY
        : (TEST(DR_ALLOC_COMMIT_ONLY, flags) ? RAW_ALLOC_COMMIT_ONLY : 0);
#endif
    size = ALIGN_FORWARD(size, PAGE_SIZE);
    if (TEST(DR_ALLOC_NON_DR, flags)) {
        /* use lock to avoid racy update on parallel memory allocation,
         * e.g. allocation from another thread at p happens after os_heap_free
         * but before remove_from_all_memory_areas
         */
        all_memory_areas_lock();
    } else {
        /* memory alloc/dealloc and updating DR list must be atomic */
        dynamo_vm_areas_lock(); /* if already hold lock this is a nop */
    }
    res = os_raw_mem_free(p, size, os_flags, &error_code);
    if (TEST(DR_ALLOC_NON_DR, flags)) {
        remove_from_all_memory_areas(p, p + size);
        all_memory_areas_unlock();
    } else {
        /* this routine updates allmem for us: */
        remove_dynamo_vm_area((app_pc)addr, ((app_pc)addr) + size);
    }
    if (!TEST(DR_ALLOC_NON_DR, flags))
        dynamo_vm_areas_unlock();
    if (res)
        RSTATS_SUB(client_raw_mmap_size, size);
    return res;
}

DR_API
void *
dr_raw_mem_alloc(size_t size, uint prot, void *addr)
{
    return raw_mem_alloc(size, prot, addr, DR_ALLOC_NON_DR);
}

DR_API
bool
dr_raw_mem_free(void *addr, size_t size)
{
    return raw_mem_free(addr, size, DR_ALLOC_NON_DR);
}

static void *
custom_memory_shared(bool alloc, void *drcontext, dr_alloc_flags_t flags, size_t size,
                     uint prot, void *addr, bool *free_res)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(alloc || free_res != NULL, "must ask for free_res on free");
    CLIENT_ASSERT(alloc || addr != NULL, "cannot free NULL");
    CLIENT_ASSERT(!TESTALL(DR_ALLOC_NON_DR | DR_ALLOC_CACHE_REACHABLE, flags),
                  "dr_custom_alloc: cannot combine non-DR and cache-reachable");
    CLIENT_ASSERT(!alloc || TEST(DR_ALLOC_FIXED_LOCATION, flags) || addr == NULL,
                  "dr_custom_alloc: address only honored for fixed location");
#ifdef WINDOWS
    CLIENT_ASSERT(!TESTANY(DR_ALLOC_RESERVE_ONLY | DR_ALLOC_COMMIT_ONLY, flags) ||
                      TESTALL(DR_ALLOC_NON_HEAP | DR_ALLOC_NON_DR, flags),
                  "dr_custom_alloc: reserve/commit-only are only for non-DR non-heap");
    CLIENT_ASSERT(!TEST(DR_ALLOC_RESERVE_ONLY, flags) ||
                      !TEST(DR_ALLOC_COMMIT_ONLY, flags),
                  "dr_custom_alloc: cannot combine reserve-only + commit-only");
#endif
    CLIENT_ASSERT(!TEST(DR_ALLOC_CACHE_REACHABLE, flags) ||
                      !DYNAMO_OPTION(satisfy_w_xor_x),
                  "dr_custom_alloc: DR_ALLOC_CACHE_REACHABLE memory is not "
                  "supported with -satisfy_w_xor_x");
    if (TEST(DR_ALLOC_NON_HEAP, flags)) {
        CLIENT_ASSERT(drcontext == NULL,
                      "dr_custom_alloc: drcontext must be NULL for non-heap");
        CLIENT_ASSERT(!TEST(DR_ALLOC_THREAD_PRIVATE, flags),
                      "dr_custom_alloc: non-heap cannot be thread-private");
        CLIENT_ASSERT(!TESTALL(DR_ALLOC_CACHE_REACHABLE | DR_ALLOC_LOW_2GB, flags),
                      "dr_custom_alloc: cannot combine low-2GB and cache-reachable");
#ifdef WINDOWS
        CLIENT_ASSERT(addr != NULL || !TEST(DR_ALLOC_COMMIT_ONLY, flags),
                      "dr_custom_alloc: commit-only requires non-NULL addr");
#endif
        if (TEST(DR_ALLOC_LOW_2GB, flags)) {
#ifdef WINDOWS
            CLIENT_ASSERT(!TEST(DR_ALLOC_COMMIT_ONLY, flags),
                          "dr_custom_alloc: cannot combine commit-only and low-2GB");
#endif
            CLIENT_ASSERT(!alloc || addr == NULL,
                          "dr_custom_alloc: cannot pass an addr with low-2GB");
            /* Even if not non-DR, easier to allocate via raw */
            if (alloc)
                return raw_mem_alloc(size, prot, addr, flags);
            else
                *free_res = raw_mem_free(addr, size, flags);
        } else if (TEST(DR_ALLOC_NON_DR, flags)) {
            /* ok for addr to be NULL */
            if (alloc)
                return raw_mem_alloc(size, prot, addr, flags);
            else
                *free_res = raw_mem_free(addr, size, flags);
        } else { /* including DR_ALLOC_CACHE_REACHABLE */
            CLIENT_ASSERT(!alloc || !TEST(DR_ALLOC_CACHE_REACHABLE, flags) ||
                              addr == NULL,
                          "dr_custom_alloc: cannot ask for addr and cache-reachable");
            /* This flag is here solely so we know which version of free to call */
            if (TEST(DR_ALLOC_FIXED_LOCATION, flags) ||
                !TEST(DR_ALLOC_CACHE_REACHABLE, flags)) {
                CLIENT_ASSERT(addr != NULL || !TEST(DR_ALLOC_FIXED_LOCATION, flags),
                              "dr_custom_alloc: fixed location requires an address");
                if (alloc)
                    return raw_mem_alloc(size, prot, addr, 0);
                else
                    *free_res = raw_mem_free(addr, size, 0);
            } else {
                if (alloc)
                    return dr_nonheap_alloc(size, prot);
                else {
                    *free_res = true;
                    dr_nonheap_free(addr, size);
                }
            }
        }
    } else {
        if (!alloc)
            *free_res = true;
        CLIENT_ASSERT(!alloc || addr == NULL,
                      "dr_custom_alloc: cannot pass an addr for heap memory");
        CLIENT_ASSERT(drcontext == NULL || TEST(DR_ALLOC_THREAD_PRIVATE, flags),
                      "dr_custom_alloc: drcontext must be NULL for global heap");
        CLIENT_ASSERT(!TEST(DR_ALLOC_LOW_2GB, flags),
                      "dr_custom_alloc: cannot ask for heap in low 2GB");
        CLIENT_ASSERT(!TEST(DR_ALLOC_NON_DR, flags),
                      "dr_custom_alloc: cannot ask for non-DR heap memory");
        if (TEST(DR_ALLOC_CACHE_REACHABLE, flags)) {
            if (TEST(DR_ALLOC_THREAD_PRIVATE, flags)) {
                if (alloc)
                    return dr_thread_alloc(drcontext, size);
                else
                    dr_thread_free(drcontext, addr, size);
            } else {
                if (alloc)
                    return dr_global_alloc(size);
                else
                    dr_global_free(addr, size);
            }
        } else {
            if (TEST(DR_ALLOC_THREAD_PRIVATE, flags)) {
                if (alloc)
                    return heap_alloc(dcontext, size HEAPACCT(ACCT_CLIENT));
                else
                    heap_free(dcontext, addr, size HEAPACCT(ACCT_CLIENT));
            } else {
                if (alloc)
                    return global_heap_alloc(size HEAPACCT(ACCT_CLIENT));
                else
                    global_heap_free(addr, size HEAPACCT(ACCT_CLIENT));
            }
        }
    }
    return NULL;
}

DR_API
void *
dr_custom_alloc(void *drcontext, dr_alloc_flags_t flags, size_t size, uint prot,
                void *addr)
{
    return custom_memory_shared(true, drcontext, flags, size, prot, addr, NULL);
}

DR_API
bool
dr_custom_free(void *drcontext, dr_alloc_flags_t flags, void *addr, size_t size)
{
    bool res;
    custom_memory_shared(false, drcontext, flags, size, 0, addr, &res);
    return res;
}

DR_API
/* With ld's -wrap option, we can supply a replacement for malloc.
 * This routine allocates memory from DR's global memory pool.  Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so __wrap_free() can retrieve it.
 */
void *
__wrap_malloc(size_t size)
{
    return redirect_malloc(size);
}

DR_API
/* With ld's -wrap option, we can supply a replacement for realloc.
 * This routine allocates memory from DR's global memory pool.  Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so __wrap_free() can retrieve it.
 */
void *
__wrap_realloc(void *mem, size_t size)
{
    return redirect_realloc(mem, size);
}

DR_API
/* With ld's -wrap option, we can supply a replacement for calloc.
 * This routine allocates memory from DR's global memory pool. Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so __wrap_free() can retrieve it.
 */
void *
__wrap_calloc(size_t nmemb, size_t size)
{
    return redirect_calloc(nmemb, size);
}

DR_API
/* With ld's -wrap option, we can supply a replacement for free.  This
 * routine frees memory allocated by __wrap_alloc and expects the
 * allocation size to be available in the few bytes before 'mem'.
 */
void
__wrap_free(void *mem)
{
    redirect_free(mem);
}

DR_API
char *
__wrap_strdup(const char *str)
{
    return redirect_strdup(str);
}

DR_API
bool
dr_memory_protect(void *base, size_t size, uint new_prot)
{
    /* We do allow the client to modify DR memory, for allocating a
     * region and later making it unwritable.  We should probably
     * allow modifying ntdll, since our general model is to trust the
     * client and let it shoot itself in the foot, but that would require
     * passing in extra args to app_memory_protection_change() to ignore
     * the patch_proof_list: and maybe it is safer to disallow client
     * from putting hooks in ntdll.
     */
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    if (!dynamo_vm_area_overlap(base, ((byte *)base) + size)) {
        uint mod_prot = new_prot;
        uint res =
            app_memory_protection_change(get_thread_private_dcontext(), base, size,
                                         new_prot, &mod_prot, NULL, false /*!image*/);
        if (res != DO_APP_MEM_PROT_CHANGE) {
            if (res == FAIL_APP_MEM_PROT_CHANGE || res == PRETEND_APP_MEM_PROT_CHANGE) {
                return false;
            } else {
                /* SUBSET_APP_MEM_PROT_CHANGE should only happen for
                 * PROGRAM_SHEPHERDING.  FIXME: not sure how common
                 * this will be: for now we just fail.
                 */
                return false;
            }
        }
        CLIENT_ASSERT(mod_prot == new_prot, "internal error on dr_memory_protect()");
    }
    return set_protection(base, size, new_prot);
}

DR_API
size_t
dr_page_size(void)
{
    return os_page_size();
}

DR_API
/* checks to see that all bytes with addresses from pc to pc+size-1
 * are readable and that reading from there won't generate an exception.
 */
bool
dr_memory_is_readable(const byte *pc, size_t size)
{
    return is_readable_without_exception(pc, size);
}

DR_API
/* OS neutral memory query for clients, just wrapper around our get_memory_info(). */
bool
dr_query_memory(const byte *pc, byte **base_pc, size_t *size, uint *prot)
{
    uint real_prot;
    bool res;
#if defined(UNIX) && defined(HAVE_MEMINFO)
    /* xref PR 246897 - the cached all memory list can have problems when
     * out-of-process entities change the mapings. For now we use the from
     * os version instead (even though it's slower, and only if we have
     * HAVE_MEMINFO_MAPS support). FIXME
     * XXX i#853: We could decide allmem vs os with the use_all_memory_areas
     * option.
     */
    res = get_memory_info_from_os(pc, base_pc, size, &real_prot);
#else
    res = get_memory_info(pc, base_pc, size, &real_prot);
#endif
    if (prot != NULL) {
        if (is_pretend_or_executable_writable((app_pc)pc)) {
            /* We can't assert there's no DR_MEMPROT_WRITE b/c we mark selfmod
             * as executable-but-writable and we'll come here.
             */
            real_prot |= DR_MEMPROT_WRITE | DR_MEMPROT_PRETEND_WRITE;
        }
        *prot = real_prot;
    }
    return res;
}

DR_API
bool
dr_query_memory_ex(const byte *pc, OUT dr_mem_info_t *info)
{
    bool res;
#if defined(UNIX) && defined(HAVE_MEMINFO)
    /* PR 246897: all_memory_areas not ready for prime time */
    res = query_memory_ex_from_os(pc, info);
#else
    res = query_memory_ex(pc, info);
#endif
    if (is_pretend_or_executable_writable((app_pc)pc)) {
        /* We can't assert there's no DR_MEMPROT_WRITE b/c we mark selfmod
         * as executable-but-writable and we'll come here.
         */
        info->prot |= DR_MEMPROT_WRITE | DR_MEMPROT_PRETEND_WRITE;
    }
    return res;
}

DR_API
/* Wrapper around our safe_read. Xref P4 198875, placeholder till we have try/except */
bool
dr_safe_read(const void *base, size_t size, void *out_buf, size_t *bytes_read)
{
    return safe_read_ex(base, size, out_buf, bytes_read);
}

DR_API
/* Wrapper around our safe_write. Xref P4 198875, placeholder till we have try/except */
bool
dr_safe_write(void *base, size_t size, const void *in_buf, size_t *bytes_written)
{
    return safe_write_ex(base, size, in_buf, bytes_written);
}

DR_API
void
dr_try_setup(void *drcontext, void **try_cxt)
{
    /* Yes we're duplicating the code from the TRY() macro but this
     * provides better abstraction and lets us change our impl later
     * vs exposing that macro
     */
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    try_except_context_t *try_state;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL && dcontext == get_thread_private_dcontext());
    ASSERT(try_cxt != NULL);
    /* We allocate on the heap to avoid having to expose the try_except_context_t
     * and dr_jmp_buf_t structs and be tied to their exact layouts.
     * The client is likely to allocate memory inside the try anyway
     * if doing a decode or something.
     */
    try_state = (try_except_context_t *)HEAP_TYPE_ALLOC(dcontext, try_except_context_t,
                                                        ACCT_CLIENT, PROTECTED);
    *try_cxt = try_state;
    try_state->prev_context = dcontext->try_except.try_except_state;
    dcontext->try_except.try_except_state = try_state;
}

/* dr_try_start() is in x86.asm since we can't have an extra frame that's
 * going to be torn down between the longjmp and the restore point
 */

DR_API
void
dr_try_stop(void *drcontext, void *try_cxt)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    try_except_context_t *try_state = (try_except_context_t *)try_cxt;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL && dcontext == get_thread_private_dcontext());
    ASSERT(try_state != NULL);
    POP_TRY_BLOCK(&dcontext->try_except, *try_state);
    HEAP_TYPE_FREE(dcontext, try_state, try_except_context_t, ACCT_CLIENT, PROTECTED);
}

DR_API
bool
dr_memory_is_dr_internal(const byte *pc)
{
    return is_dynamo_address((app_pc)pc);
}

DR_API
bool
dr_memory_is_in_client(const byte *pc)
{
    return is_in_client_lib((app_pc)pc);
}

void
instrument_client_lib_loaded(byte *start, byte *end)
{
    /* i#852: include Extensions as they are really part of the clients and
     * aren't like other private libs.
     * XXX: we only avoid having the client libs on here b/c they're specified via
     * full path and don't go through the loaders' locate routines.
     * Not a big deal if they do end up on here: if they always did we could
     * remove the linear walk in is_in_client_lib().
     */
    /* called prior to instrument_init() */
    init_client_aux_libs();
    vmvector_add(client_aux_libs, start, end, NULL /*not an auxlib*/);
}

void
instrument_client_lib_unloaded(byte *start, byte *end)
{
    /* called after instrument_exit() */
    if (client_aux_libs != NULL)
        vmvector_remove(client_aux_libs, start, end);
}

/**************************************************
 * CLIENT AUXILIARY LIBRARIES
 */

DR_API
dr_auxlib_handle_t
dr_load_aux_library(const char *name, byte **lib_start /*OPTIONAL OUT*/,
                    byte **lib_end /*OPTIONAL OUT*/)
{
    byte *start, *end;
    dr_auxlib_handle_t lib = load_shared_library(name, true /*reachable*/);
    if (shared_library_bounds(lib, NULL, name, &start, &end)) {
        /* be sure to replace b/c i#852 now adds during load w/ empty data */
        vmvector_add_replace(client_aux_libs, start, end, (void *)lib);
        if (lib_start != NULL)
            *lib_start = start;
        if (lib_end != NULL)
            *lib_end = end;
        all_memory_areas_lock();
        update_all_memory_areas(start, end,
                                /* XXX: see comment in instrument_init()
                                 * on walking the sections and what prot to use
                                 */
                                MEMPROT_READ, DR_MEMTYPE_IMAGE);
        all_memory_areas_unlock();
    } else {
        unload_shared_library(lib);
        lib = NULL;
    }
    return lib;
}

DR_API
dr_auxlib_routine_ptr_t
dr_lookup_aux_library_routine(dr_auxlib_handle_t lib, const char *name)
{
    if (lib == NULL)
        return NULL;
    return lookup_library_routine(lib, name);
}

DR_API
bool
dr_unload_aux_library(dr_auxlib_handle_t lib)
{
    byte *start = NULL, *end = NULL;
    /* unfortunately on linux w/ dlopen we cannot find the bounds w/o
     * either the path or an address so we iterate.
     * once we have our private loader we shouldn't need this:
     * XXX i#157
     */
    vmvector_iterator_t vmvi;
    dr_auxlib_handle_t found = NULL;
    if (lib == NULL)
        return false;
    vmvector_iterator_start(client_aux_libs, &vmvi);
    while (vmvector_iterator_hasnext(&vmvi)) {
        found = (dr_auxlib_handle_t)vmvector_iterator_next(&vmvi, &start, &end);
        if (found == lib)
            break;
    }
    vmvector_iterator_stop(&vmvi);
    if (found == lib) {
        CLIENT_ASSERT(start != NULL && start < end, "logic error");
        vmvector_remove(client_aux_libs, start, end);
        unload_shared_library(lib);
        all_memory_areas_lock();
        update_all_memory_areas(start, end, MEMPROT_NONE, DR_MEMTYPE_FREE);
        all_memory_areas_unlock();
        return true;
    } else {
        CLIENT_ASSERT(false, "invalid aux lib");
        return false;
    }
}

#if defined(WINDOWS) && !defined(X64)
/* XXX i#1633: these routines all have 64-bit handle and routine types for
 * handling win8's high ntdll64 in the future.  For now the implementation
 * treats them as 32-bit types and we do not support win8+.
 */

DR_API
dr_auxlib64_handle_t
dr_load_aux_x64_library(const char *name)
{
    HANDLE h;
    /* We use the x64 system loader.  We assume that x64 state is fine being
     * interrupted at arbitrary points during x86 execution, and that there
     * is little risk of transparency violations.
     */
    /* load_library_64() is racy.  We don't expect anyone else to load
     * x64 libs, but another thread in this client could, so we
     * serialize here.
     */
    d_r_mutex_lock(&client_aux_lib64_lock);
    /* XXX: if we switch to our private loader we'll need to add custom
     * search support to look in 64-bit system dir
     */
    /* XXX: I'd add to the client_aux_libs vector, but w/ the system loader
     * loading this I don't know all the dependent libs it might load.
     * Not bothering for now.
     */
    h = load_library_64(name);
    d_r_mutex_unlock(&client_aux_lib64_lock);
    return (dr_auxlib64_handle_t)h;
}

DR_API
dr_auxlib64_routine_ptr_t
dr_lookup_aux_x64_library_routine(dr_auxlib64_handle_t lib, const char *name)
{
    uint64 res = get_proc_address_64((uint64)lib, name);
    return (dr_auxlib64_routine_ptr_t)res;
}

DR_API
bool
dr_unload_aux_x64_library(dr_auxlib64_handle_t lib)
{
    bool res;
    d_r_mutex_lock(&client_aux_lib64_lock);
    res = free_library_64((HANDLE)(uint)lib); /* uint cast to avoid cl warning */
    d_r_mutex_unlock(&client_aux_lib64_lock);
    return res;
}
#endif

/***************************************************************************
 * LOCKS
 */

DR_API
/* Initializes a mutex
 */
void *
dr_mutex_create(void)
{
    void *mutex =
        (void *)HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, mutex_t, ACCT_CLIENT, UNPROTECTED);
    ASSIGN_INIT_LOCK_FREE(*((mutex_t *)mutex), dr_client_mutex);
    return mutex;
}

DR_API
/* Deletes mutex
 */
void
dr_mutex_destroy(void *mutex)
{
    /* Delete mutex so locks_not_closed()==0 test in dynamo.c passes */
    DELETE_LOCK(*((mutex_t *)mutex));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, (mutex_t *)mutex, mutex_t, ACCT_CLIENT, UNPROTECTED);
}

DR_API
/* Locks mutex
 */
void
dr_mutex_lock(void *mutex)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* set client_grab_mutex so that we know to set client_thread_safe_for_synch
     * around the actual wait for the lock */
    if (IS_CLIENT_THREAD(dcontext)) {
        dcontext->client_data->client_grab_mutex = mutex;
        /* We do this on the outside so that we're conservative wrt races
         * in the direction of not killing the thread while it has a lock
         */
        dcontext->client_data->mutex_count++;
    }
    d_r_mutex_lock((mutex_t *)mutex);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_grab_mutex = NULL;
}

DR_API
/* Unlocks mutex
 */
void
dr_mutex_unlock(void *mutex)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    d_r_mutex_unlock((mutex_t *)mutex);
    /* We do this on the outside so that we're conservative wrt races
     * in the direction of not killing the thread while it has a lock
     */
    if (IS_CLIENT_THREAD(dcontext)) {
        CLIENT_ASSERT(dcontext->client_data->mutex_count > 0,
                      "internal client mutex nesting error");
        dcontext->client_data->mutex_count--;
    }
}

DR_API
/* Tries once to grab the lock, returns whether or not successful
 */
bool
dr_mutex_trylock(void *mutex)
{
    bool success = false;
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* set client_grab_mutex so that we know to set client_thread_safe_for_synch
     * around the actual wait for the lock */
    if (IS_CLIENT_THREAD(dcontext)) {
        dcontext->client_data->client_grab_mutex = mutex;
        /* We do this on the outside so that we're conservative wrt races
         * in the direction of not killing the thread while it has a lock
         */
        dcontext->client_data->mutex_count++;
    }
    success = d_r_mutex_trylock((mutex_t *)mutex);
    if (IS_CLIENT_THREAD(dcontext)) {
        if (!success)
            dcontext->client_data->mutex_count--;
        dcontext->client_data->client_grab_mutex = NULL;
    }
    return success;
}

DR_API
bool
dr_mutex_self_owns(void *mutex)
{
    return IF_DEBUG_ELSE(OWN_MUTEX((mutex_t *)mutex), true);
}

DR_API
bool
dr_mutex_mark_as_app(void *mutex)
{
    mutex_t *lock = (mutex_t *)mutex;
    d_r_mutex_mark_as_app(lock);
    return true;
}

DR_API
void *
dr_rwlock_create(void)
{
    void *rwlock = (void *)HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, read_write_lock_t,
                                           ACCT_CLIENT, UNPROTECTED);
    ASSIGN_INIT_READWRITE_LOCK_FREE(*((read_write_lock_t *)rwlock), dr_client_mutex);
    return rwlock;
}

DR_API
void
dr_rwlock_destroy(void *rwlock)
{
    DELETE_READWRITE_LOCK(*((read_write_lock_t *)rwlock));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, (read_write_lock_t *)rwlock, read_write_lock_t,
                   ACCT_CLIENT, UNPROTECTED);
}

DR_API
void
dr_rwlock_read_lock(void *rwlock)
{
    d_r_read_lock((read_write_lock_t *)rwlock);
}

DR_API
void
dr_rwlock_read_unlock(void *rwlock)
{
    d_r_read_unlock((read_write_lock_t *)rwlock);
}

DR_API
void
dr_rwlock_write_lock(void *rwlock)
{
    d_r_write_lock((read_write_lock_t *)rwlock);
}

DR_API
void
dr_rwlock_write_unlock(void *rwlock)
{
    d_r_write_unlock((read_write_lock_t *)rwlock);
}

DR_API
bool
dr_rwlock_write_trylock(void *rwlock)
{
    return d_r_write_trylock((read_write_lock_t *)rwlock);
}

DR_API
bool
dr_rwlock_self_owns_write_lock(void *rwlock)
{
    return self_owns_write_lock((read_write_lock_t *)rwlock);
}

DR_API
bool
dr_rwlock_mark_as_app(void *rwlock)
{
    read_write_lock_t *lock = (read_write_lock_t *)rwlock;
    d_r_mutex_mark_as_app(&lock->lock);
    return true;
}

DR_API
void *
dr_recurlock_create(void)
{
    void *reclock = (void *)HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, recursive_lock_t,
                                            ACCT_CLIENT, UNPROTECTED);
    ASSIGN_INIT_RECURSIVE_LOCK_FREE(*((recursive_lock_t *)reclock), dr_client_mutex);
    return reclock;
}

DR_API
void
dr_recurlock_destroy(void *reclock)
{
    DELETE_RECURSIVE_LOCK(*((recursive_lock_t *)reclock));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, (recursive_lock_t *)reclock, recursive_lock_t,
                   ACCT_CLIENT, UNPROTECTED);
}

DR_API
void
dr_recurlock_lock(void *reclock)
{
    acquire_recursive_lock((recursive_lock_t *)reclock);
}

DR_API
void
dr_app_recurlock_lock(void *reclock, dr_mcontext_t *mc)
{
    CLIENT_ASSERT(mc->flags == DR_MC_ALL, "mcontext must be for DR_MC_ALL");

    acquire_recursive_app_lock((recursive_lock_t *)reclock,
                               dr_mcontext_as_priv_mcontext(mc));
}

DR_API
void
dr_recurlock_unlock(void *reclock)
{
    release_recursive_lock((recursive_lock_t *)reclock);
}

DR_API
bool
dr_recurlock_trylock(void *reclock)
{
    return try_recursive_lock((recursive_lock_t *)reclock);
}

DR_API
bool
dr_recurlock_self_owns(void *reclock)
{
    return self_owns_recursive_lock((recursive_lock_t *)reclock);
}

DR_API
bool
dr_recurlock_mark_as_app(void *reclock)
{
    recursive_lock_t *lock = (recursive_lock_t *)reclock;
    d_r_mutex_mark_as_app(&lock->lock);
    return true;
}

DR_API
void *
dr_event_create(void)
{
    return (void *)create_event();
}

DR_API
bool
dr_event_destroy(void *event)
{
    destroy_event((event_t)event);
    return true;
}

DR_API
bool
dr_event_wait(void *event)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    wait_for_event((event_t)event, 0);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
    return true;
}

DR_API
bool
dr_event_signal(void *event)
{
    signal_event((event_t)event);
    return true;
}

DR_API
bool
dr_event_reset(void *event)
{
    reset_event((event_t)event);
    return true;
}

DR_API
bool
dr_mark_safe_to_suspend(void *drcontext, bool enter)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    ASSERT_OWN_NO_LOCKS();
    /* We need to return so we can't call check_wait_at_safe_spot().
     * We don't set mcontext b/c noone should examine it.
     */
    if (enter)
        set_synch_state(dcontext, THREAD_SYNCH_NO_LOCKS_NO_XFER);
    else
        set_synch_state(dcontext, THREAD_SYNCH_NONE);
    return true;
}

DR_API
int
dr_atomic_add32_return_sum(volatile int *dest, int val)
{
    return atomic_add_exchange_int(dest, val);
}

#ifdef X64
DR_API
int64
dr_atomic_add64_return_sum(volatile int64 *dest, int64 val)
{
    return atomic_add_exchange_int64(dest, val);
}
#endif

DR_API
int
dr_atomic_load32(volatile int *src)
{
    return atomic_aligned_read_int(src);
}

DR_API
void
dr_atomic_store32(volatile int *dest, int val)
{
    ATOMIC_4BYTE_ALIGNED_WRITE(dest, val, false);
}

#ifdef X64
DR_API
int64
dr_atomic_load64(volatile int64 *src)
{
    return atomic_aligned_read_int64(src);
}

DR_API
void
dr_atomic_store64(volatile int64 *dest, int64 val)
{
    ATOMIC_8BYTE_ALIGNED_WRITE(dest, val, false);
}
#endif

byte *
dr_map_executable_file(const char *filename, dr_map_executable_flags_t flags,
                       size_t *size OUT)
{
#ifdef MACOS
    /* XXX i#1285: implement private loader on Mac */
    return NULL;
#else
    modload_flags_t mflags = MODLOAD_NOT_PRIVLIB;
    if (TEST(DR_MAPEXE_SKIP_WRITABLE, flags))
        mflags |= MODLOAD_SKIP_WRITABLE;
    if (filename == NULL)
        return NULL;
    return privload_map_and_relocate(filename, size, mflags);
#endif
}

bool
dr_unmap_executable_file(byte *base, size_t size)
{
    if (standalone_library)
        return os_unmap_file(base, size);
    else
        return d_r_unmap_file(base, size);
}

DR_API
/* Creates a new directory.  Fails if the directory already exists
 * or if it can't be created.
 */
bool
dr_create_dir(const char *fname)
{
    return os_create_dir(fname, CREATE_DIR_REQUIRE_NEW);
}

DR_API
bool
dr_delete_dir(const char *fname)
{
    return os_delete_dir(fname);
}

DR_API
bool
dr_get_current_directory(char *buf, size_t bufsz)
{
    return os_get_current_dir(buf, bufsz);
}

DR_API
/* Checks existence of a directory. */
bool
dr_directory_exists(const char *fname)
{
    return os_file_exists(fname, true);
}

DR_API
/* Checks for the existence of a file. */
bool
dr_file_exists(const char *fname)
{
    return os_file_exists(fname, false);
}

DR_API
/* Opens a file in the mode specified by mode_flags.
 * Returns INVALID_FILE if unsuccessful
 */
file_t
dr_open_file(const char *fname, uint mode_flags)
{
    uint flags = 0;

    if (TEST(DR_FILE_WRITE_REQUIRE_NEW, mode_flags)) {
        flags |= OS_OPEN_WRITE | OS_OPEN_REQUIRE_NEW;
    }
    if (TEST(DR_FILE_WRITE_APPEND, mode_flags)) {
        CLIENT_ASSERT((flags == 0), "dr_open_file: multiple write modes selected");
        flags |= OS_OPEN_WRITE | OS_OPEN_APPEND;
    }
    if (TEST(DR_FILE_WRITE_OVERWRITE, mode_flags)) {
        CLIENT_ASSERT((flags == 0), "dr_open_file: multiple write modes selected");
        flags |= OS_OPEN_WRITE;
    }
    if (TEST(DR_FILE_WRITE_ONLY, mode_flags)) {
        CLIENT_ASSERT((flags == 0), "dr_open_file: multiple write modes selected");
        flags |= OS_OPEN_WRITE_ONLY;
    }
    if (TEST(DR_FILE_READ, mode_flags))
        flags |= OS_OPEN_READ;
    CLIENT_ASSERT((flags != 0), "dr_open_file: no mode selected");

    if (TEST(DR_FILE_ALLOW_LARGE, mode_flags))
        flags |= OS_OPEN_ALLOW_LARGE;

    if (TEST(DR_FILE_CLOSE_ON_FORK, mode_flags))
        flags |= OS_OPEN_CLOSE_ON_FORK;

    /* all client-opened files are protected */
    return os_open_protected(fname, flags);
}

DR_API
/* Closes file f
 */
void
dr_close_file(file_t f)
{
    /* all client-opened files are protected */
    os_close_protected(f);
}

DR_API
/* Renames the file src to dst. */
bool
dr_rename_file(const char *src, const char *dst, bool replace)
{
    return os_rename_file(src, dst, replace);
}

DR_API
/* Deletes a file. */
bool
dr_delete_file(const char *filename)
{
    /* os_delete_mapped_file should be a superset of os_delete_file, so we use
     * it.
     */
    return os_delete_mapped_file(filename);
}

DR_API
/* Flushes any buffers for file f
 */
void
dr_flush_file(file_t f)
{
    os_flush(f);
}

DR_API
/* Writes count bytes from buf to f.
 * Returns the actual number written.
 */
ssize_t
dr_write_file(file_t f, const void *buf, size_t count)
{
#ifdef WINDOWS
    if ((f == STDOUT || f == STDERR) && print_to_console)
        return dr_write_to_console_varg(f == STDOUT, "%.*s", count, buf);
    else
#endif
        return os_write(f, buf, count);
}

DR_API
/* Reads up to count bytes from f into buf.
 * Returns the actual number read.
 */
ssize_t
dr_read_file(file_t f, void *buf, size_t count)
{
    return os_read(f, buf, count);
}

DR_API
/* sets the current file position for file f to offset bytes from the specified origin
 * returns true if successful */
bool
dr_file_seek(file_t f, int64 offset, int origin)
{
    CLIENT_ASSERT(origin == DR_SEEK_SET || origin == DR_SEEK_CUR || origin == DR_SEEK_END,
                  "dr_file_seek: invalid origin value");
    return os_seek(f, offset, origin);
}

DR_API
/* gets the current file position for file f in bytes from start of file */
int64
dr_file_tell(file_t f)
{
    return os_tell(f);
}

DR_API
file_t
dr_dup_file_handle(file_t f)
{
#ifdef UNIX
    /* returns -1 on failure == INVALID_FILE */
    return dup_syscall(f);
#else
    HANDLE ht = INVALID_HANDLE_VALUE;
    NTSTATUS res =
        duplicate_handle(NT_CURRENT_PROCESS, f, NT_CURRENT_PROCESS, &ht, SYNCHRONIZE, 0,
                         DUPLICATE_SAME_ACCESS | DUPLICATE_SAME_ATTRIBUTES);
    if (!NT_SUCCESS(res))
        return INVALID_FILE;
    else
        return ht;
#endif
}

DR_API
bool
dr_file_size(file_t fd, OUT uint64 *size)
{
    return os_get_file_size_by_handle(fd, size);
}

DR_API
void *
dr_map_file(file_t f, size_t *size INOUT, uint64 offs, app_pc addr, uint prot, uint flags)
{
    return (void *)d_r_map_file(
        f, size, offs, addr, prot,
        (TEST(DR_MAP_PRIVATE, flags) ? MAP_FILE_COPY_ON_WRITE : 0) |
            IF_WINDOWS((TEST(DR_MAP_IMAGE, flags) ? MAP_FILE_IMAGE : 0) |)
                IF_UNIX((TEST(DR_MAP_FIXED, flags) ? MAP_FILE_FIXED : 0) |)(
                    TEST(DR_MAP_CACHE_REACHABLE, flags) ? MAP_FILE_REACHABLE : 0));
}

DR_API
bool
dr_unmap_file(void *map, size_t size)
{
    dr_mem_info_t info;
    CLIENT_ASSERT(ALIGNED(map, PAGE_SIZE), "dr_unmap_file: map is not page aligned");
    if (!dr_query_memory_ex(map, &info) /* fail to query */ ||
        info.type == DR_MEMTYPE_FREE /* not mapped file */) {
        CLIENT_ASSERT(false, "dr_unmap_file: incorrect file map");
        return false;
    }
#ifdef WINDOWS
    /* On Windows, the whole file will be unmapped instead, so we adjust
     * the bound to make sure vm_areas are updated correctly.
     */
    map = info.base_pc;
    if (info.type == DR_MEMTYPE_IMAGE) {
        size = get_allocation_size(map, NULL);
    } else
        size = info.size;
#endif
    return d_r_unmap_file((byte *)map, size);
}

DR_API
void
dr_log(void *drcontext, uint mask, uint level, const char *fmt, ...)
{
#ifdef DEBUG
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    va_list ap;
    if (d_r_stats != NULL &&
        ((d_r_stats->logmask & mask) == 0 || d_r_stats->loglevel < level))
        return;
    va_start(ap, fmt);
    if (dcontext != NULL)
        do_file_write(dcontext->logfile, fmt, ap);
    else
        do_file_write(main_logfile, fmt, ap);
    va_end(ap);
#else
    return; /* no logging if not debug */
#endif
}

DR_API
/* Returns the log file for the drcontext thread.
 * If drcontext is NULL, returns the main log file.
 */
file_t
dr_get_logfile(void *drcontext)
{
#ifdef DEBUG
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (dcontext != NULL)
        return dcontext->logfile;
    else
        return main_logfile;
#else
    return INVALID_FILE;
#endif
}

DR_API
/* Returns true iff the -stderr_mask runtime option is non-zero, indicating
 * that the user wants notification messages printed to stderr.
 */
bool
dr_is_notify_on(void)
{
    return (dynamo_options.stderr_mask != 0);
}

#ifdef WINDOWS
DR_API file_t
dr_get_stdout_file(void)
{
    return get_stdout_handle();
}

DR_API file_t
dr_get_stderr_file(void)
{
    return get_stderr_handle();
}

DR_API file_t
dr_get_stdin_file(void)
{
    return get_stdin_handle();
}
#endif

#ifdef PROGRAM_SHEPHERDING

DR_API void
dr_write_forensics_report(void *dcontext, file_t file,
                          dr_security_violation_type_t violation,
                          dr_security_violation_action_t action,
                          const char *violation_name)
{
    security_violation_t sec_violation;
    action_type_t sec_action;

    switch (violation) {
    case DR_RCO_STACK_VIOLATION: sec_violation = STACK_EXECUTION_VIOLATION; break;
    case DR_RCO_HEAP_VIOLATION: sec_violation = HEAP_EXECUTION_VIOLATION; break;
    case DR_RCT_RETURN_VIOLATION: sec_violation = RETURN_TARGET_VIOLATION; break;
    case DR_RCT_INDIRECT_CALL_VIOLATION:
        sec_violation = INDIRECT_CALL_RCT_VIOLATION;
        break;
    case DR_RCT_INDIRECT_JUMP_VIOLATION:
        sec_violation = INDIRECT_JUMP_RCT_VIOLATION;
        break;
    default:
        CLIENT_ASSERT(false,
                      "dr_write_forensics_report does not support "
                      "DR_UNKNOWN_VIOLATION or invalid violation types");
        return;
    }

    switch (action) {
    case DR_VIOLATION_ACTION_KILL_PROCESS: sec_action = ACTION_TERMINATE_PROCESS; break;
    case DR_VIOLATION_ACTION_CONTINUE:
    case DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT:
        sec_action = ACTION_CONTINUE;
        break;
    case DR_VIOLATION_ACTION_KILL_THREAD: sec_action = ACTION_TERMINATE_THREAD; break;
    case DR_VIOLATION_ACTION_THROW_EXCEPTION: sec_action = ACTION_THROW_EXCEPTION; break;
    default:
        CLIENT_ASSERT(false, "dr_write_forensics_report invalid action selection");
        return;
    }

    /* FIXME - could use a better message. */
    append_diagnostics(file, action_message[sec_action], violation_name, sec_violation);
}

#endif /* PROGRAM_SHEPHERDING */

#ifdef WINDOWS
DR_API void
dr_messagebox(const char *fmt, ...)
{
    dcontext_t *dcontext = NULL;
    if (!standalone_library)
        dcontext = get_thread_private_dcontext();
    char msg[MAX_LOG_LENGTH];
    wchar_t wmsg[MAX_LOG_LENGTH];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, BUFFER_SIZE_ELEMENTS(msg), fmt, ap);
    NULL_TERMINATE_BUFFER(msg);
    snwprintf(wmsg, BUFFER_SIZE_ELEMENTS(wmsg), L"%S", msg);
    NULL_TERMINATE_BUFFER(wmsg);
    if (!standalone_library && IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    nt_messagebox(wmsg, debugbox_get_title());
    if (!standalone_library && IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
    va_end(ap);
}

static ssize_t
dr_write_to_console(bool to_stdout, const char *fmt, va_list ap)
{
    bool res = true;
    char msg[MAX_LOG_LENGTH];
    uint written = 0;
    int len;
    HANDLE std;
    CLIENT_ASSERT(dr_using_console(), "internal logic error");
    ASSERT(priv_kernel32 != NULL && kernel32_WriteFile != NULL);
    /* kernel32!GetStdHandle(STD_OUTPUT_HANDLE) == our PEB-based get_stdout_handle */
    std = (to_stdout ? get_stdout_handle() : get_stderr_handle());
    if (std == INVALID_HANDLE_VALUE)
        return false;
    len = vsnprintf(msg, BUFFER_SIZE_ELEMENTS(msg), fmt, ap);
    /* Let user know if message was truncated */
    if (len < 0 || len == BUFFER_SIZE_ELEMENTS(msg))
        res = false;
    NULL_TERMINATE_BUFFER(msg);
    /* Make this routine work in all kinds of windows by going through
     * kernel32!WriteFile, which will call WriteConsole for us.
     */
    res =
        res && kernel32_WriteFile(std, msg, (DWORD)strlen(msg), (LPDWORD)&written, NULL);
    return (res ? written : 0);
}

static ssize_t
dr_write_to_console_varg(bool to_stdout, const char *fmt, ...)
{
    va_list ap;
    ssize_t res;
    va_start(ap, fmt);
    res = dr_write_to_console(to_stdout, fmt, ap);
    va_end(ap);
    return res;
}

DR_API
bool
dr_using_console(void)
{
    bool res;
    if (get_os_version() >= WINDOWS_VERSION_8) {
        FILE_FS_DEVICE_INFORMATION device_info;
        HANDLE herr = get_stderr_handle();
        /* The handle is invalid iff it's a gui app and the parent is a console */
        if (herr == INVALID_HANDLE_VALUE) {
            module_data_t *app_kernel32 = dr_lookup_module_by_name("kernel32.dll");
            if (privload_attach_parent_console(app_kernel32->start) == false) {
                dr_free_module_data(app_kernel32);
                return false;
            }
            dr_free_module_data(app_kernel32);
            herr = get_stderr_handle();
        }
        if (nt_query_volume_info(herr, &device_info, sizeof(device_info),
                                 FileFsDeviceInformation) == STATUS_SUCCESS) {
            if (device_info.DeviceType == FILE_DEVICE_CONSOLE)
                return true;
        }
        return false;
    }
    /* We detect cmd window using what kernel32!WriteFile uses: a handle
     * having certain bits set.
     */
    res = (((ptr_int_t)get_stderr_handle() & 0x10000003) == 0x3);
    CLIENT_ASSERT(!res || get_os_version() < WINDOWS_VERSION_8,
                  "Please report this: Windows 8 does have old-style consoles!");
    return res;
}

DR_API
bool
dr_enable_console_printing(void)
{
    bool success = false;
    /* b/c private loader sets cxt sw code up front based on whether have windows
     * priv libs or not, this can only be called during client init()
     */
    if (dynamo_initialized) {
        CLIENT_ASSERT(false, "dr_enable_console_printing() must be called during init");
        return false;
    }
    /* Direct writes to std handles work on win8+ (xref i#911) but we don't need
     * a separate check as the handle is detected as a non-console handle.
     */
    if (!dr_using_console())
        return true;
    if (!INTERNAL_OPTION(private_loader))
        return false;
    if (!print_to_console) {
        if (priv_kernel32 == NULL) {
            /* Not using load_shared_library() b/c it won't search paths
             * for us.  XXX: should add os-shared interface for
             * locate-and-load.
             */
            priv_kernel32 = (shlib_handle_t)locate_and_load_private_library(
                "kernel32.dll", false /*!reachable*/);
        }
        if (priv_kernel32 != NULL && kernel32_WriteFile == NULL) {
            module_data_t *app_kernel32 = dr_lookup_module_by_name("kernel32.dll");
            kernel32_WriteFile =
                (kernel32_WriteFile_t)lookup_library_routine(priv_kernel32, "WriteFile");
            /* There is some problem in loading 32 bit kernel32.dll
             * when 64 bit kernel32.dll is already loaded. If kernel32 is
             * not loaded we can't call privload_console_share because it
             * assumes kernel32 is loaded
             */
            if (app_kernel32 == NULL) {
                success = false;
            } else {
                success = privload_console_share(priv_kernel32, app_kernel32->start);
                dr_free_module_data(app_kernel32);
            }
        }
        /* We go ahead and cache whether dr_using_console().  If app really
         * changes its console, client could call this routine again
         * as a workaround.  Seems unlikely: better to have better perf.
         */
        print_to_console =
            (priv_kernel32 != NULL && kernel32_WriteFile != NULL && success);
    }
    return print_to_console;
}
#endif /* WINDOWS */

DR_API void
dr_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#ifdef WINDOWS
    if (print_to_console)
        dr_write_to_console(true /*stdout*/, fmt, ap);
    else
#endif
        do_file_write(STDOUT, fmt, ap);
    va_end(ap);
}

DR_API ssize_t
dr_vfprintf(file_t f, const char *fmt, va_list ap)
{
    ssize_t written;
#ifdef WINDOWS
    if ((f == STDOUT || f == STDERR) && print_to_console) {
        written = dr_write_to_console(f == STDOUT, fmt, ap);
        if (written <= 0)
            written = -1;
    } else
#endif
        written = do_file_write(f, fmt, ap);
    return written;
}

DR_API ssize_t
dr_fprintf(file_t f, const char *fmt, ...)
{
    va_list ap;
    ssize_t res;
    va_start(ap, fmt);
    res = dr_vfprintf(f, fmt, ap);
    va_end(ap);
    return res;
}

DR_API int
dr_snprintf(char *buf, size_t max, const char *fmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, fmt);
    /* PR 219380: we use d_r_vsnprintf instead of ntdll._vsnprintf b/c the
     * latter does not support floating point.
     * Plus, d_r_vsnprintf returns -1 for > max chars (matching Windows
     * behavior, but which Linux libc version does not do).
     */
    res = d_r_vsnprintf(buf, max, fmt, ap);
    va_end(ap);
    return res;
}

DR_API int
dr_vsnprintf(char *buf, size_t max, const char *fmt, va_list ap)
{
    return d_r_vsnprintf(buf, max, fmt, ap);
}

DR_API int
dr_snwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, fmt);
    res = d_r_vsnprintf_wide(buf, max, fmt, ap);
    va_end(ap);
    return res;
}

DR_API int
dr_vsnwprintf(wchar_t *buf, size_t max, const wchar_t *fmt, va_list ap)
{
    return d_r_vsnprintf_wide(buf, max, fmt, ap);
}

DR_API int
dr_sscanf(const char *str, const char *fmt, ...)
{
    int res;
    va_list ap;
    va_start(ap, fmt);
    res = d_r_vsscanf(str, fmt, ap);
    va_end(ap);
    return res;
}

DR_API const char *
dr_get_token(const char *str, char *buf, size_t buflen)
{
    /* We don't indicate whether any truncation happened.  The
     * reasoning is that this is meant to be used on a string of known
     * size ahead of time, so the max size for any one token is known.
     */
    const char *pos = str;
    CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(buflen), "buflen too large");
    if (d_r_parse_word(str, &pos, buf, (uint)buflen) == NULL)
        return NULL;
    else
        return pos;
}

DR_API void
dr_print_instr(void *drcontext, file_t f, instr_t *instr, const char *msg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_print_instr: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT || standalone_library,
                  "dr_print_instr: drcontext is invalid");
    dr_fprintf(f, "%s " PFX " ", msg, instr_get_translation(instr));
    instr_disassemble(dcontext, instr, f);
    dr_fprintf(f, "\n");
}

DR_API void
dr_print_opnd(void *drcontext, file_t f, opnd_t opnd, const char *msg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_print_opnd: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT || standalone_library,
                  "dr_print_opnd: drcontext is invalid");
    dr_fprintf(f, "%s ", msg);
    opnd_disassemble(dcontext, opnd, f);
    dr_fprintf(f, "\n");
}

/***************************************************************************
 * Thread support
 */

DR_API
/* Returns the DR context of the current thread */
void *
dr_get_current_drcontext(void)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    return (void *)dcontext;
}

DR_API thread_id_t
dr_get_thread_id(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_get_thread_id: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_get_thread_id: drcontext is invalid");
    return dcontext->owning_thread;
}

#ifdef WINDOWS
/* Added for DrMem i#1254 */
DR_API HANDLE
dr_get_dr_thread_handle(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_get_thread_id: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_get_thread_id: drcontext is invalid");
    return dcontext->thread_record->handle;
}
#endif

DR_API void *
dr_get_tls_field(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_get_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_get_tls_field: drcontext is invalid");
    return dcontext->client_data->user_field;
}

DR_API void
dr_set_tls_field(void *drcontext, void *value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_set_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_set_tls_field: drcontext is invalid");
    dcontext->client_data->user_field = value;
}

DR_API void *
dr_get_dr_segment_base(IN reg_id_t seg)
{
#ifdef AARCHXX
    if (seg == dr_reg_stolen)
        return os_get_dr_tls_base(get_thread_private_dcontext());
    else
        return NULL;
#else
    return get_segment_base(seg);
#endif
}

DR_API
bool
dr_raw_tls_calloc(OUT reg_id_t *tls_register, OUT uint *offset, IN uint num_slots,
                  IN uint alignment)
{
    CLIENT_ASSERT(tls_register != NULL, "dr_raw_tls_calloc: tls_register cannot be NULL");
    CLIENT_ASSERT(offset != NULL, "dr_raw_tls_calloc: offset cannot be NULL");
    *tls_register = IF_X86_ELSE(SEG_TLS, IF_RISCV64_ELSE(DR_REG_TP, dr_reg_stolen));
    if (num_slots == 0)
        return true;
    return os_tls_calloc(offset, num_slots, alignment);
}

DR_API
bool
dr_raw_tls_cfree(uint offset, uint num_slots)
{
    if (num_slots == 0)
        return true;
    return os_tls_cfree(offset, num_slots);
}

DR_API
opnd_t
dr_raw_tls_opnd(void *drcontext, reg_id_t tls_register, uint tls_offs)
{
    CLIENT_ASSERT(drcontext != NULL, "dr_raw_tls_opnd: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_raw_tls_opnd: drcontext is invalid");
    IF_X86_ELSE(
        {
            return opnd_create_far_base_disp_ex(tls_register, DR_REG_NULL, DR_REG_NULL, 0,
                                                tls_offs, OPSZ_PTR,
                                                /* modern processors don't want addr16
                                                 * prefixes
                                                 */
                                                false, true, false);
        },
        { return OPND_CREATE_MEMPTR(tls_register, tls_offs); });
}

DR_API
void
dr_insert_read_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                       reg_id_t tls_register, uint tls_offs, reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_read_raw_tls: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    IF_X86_ELSE(
        {
            MINSERT(
                ilist, where,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(reg),
                                    dr_raw_tls_opnd(drcontext, tls_register, tls_offs)));
        },
        {
            MINSERT(
                ilist, where,
                XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                                  dr_raw_tls_opnd(drcontext, tls_register, tls_offs)));
        });
}

DR_API
void
dr_insert_write_raw_tls(void *drcontext, instrlist_t *ilist, instr_t *where,
                        reg_id_t tls_register, uint tls_offs, reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_write_raw_tls: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    IF_X86_ELSE(
        {
            MINSERT(ilist, where,
                    INSTR_CREATE_mov_st(
                        dcontext, dr_raw_tls_opnd(drcontext, tls_register, tls_offs),
                        opnd_create_reg(reg)));
        },
        {
            MINSERT(ilist, where,
                    XINST_CREATE_store(dcontext,
                                       dr_raw_tls_opnd(drcontext, tls_register, tls_offs),
                                       opnd_create_reg(reg)));
        });
}

DR_API
/* Current thread gives up its time quantum. */
void
dr_thread_yield(void)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    else
        dcontext->client_data->at_safe_to_terminate_syscall = true;
    os_thread_yield();
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
    else
        dcontext->client_data->at_safe_to_terminate_syscall = false;
}

DR_API
/* Current thread sleeps for time_ms milliseconds. */
void
dr_sleep(int time_ms)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    else
        dcontext->client_data->at_safe_to_terminate_syscall = true;
    os_thread_sleep(time_ms);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
    else
        dcontext->client_data->at_safe_to_terminate_syscall = false;
}

DR_API
bool
dr_client_thread_set_suspendable(bool suspendable)
{
    /* see notes in synch_with_all_threads() */
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    if (!IS_CLIENT_THREAD(dcontext))
        return false;
    dcontext->client_data->suspendable = suspendable;
    return true;
}

DR_API
bool
dr_suspend_all_other_threads_ex(OUT void ***drcontexts, OUT uint *num_suspended,
                                OUT uint *num_unsuspended, dr_suspend_flags_t flags)
{
    uint out_suspended = 0, out_unsuspended = 0;
    thread_record_t **threads;
    int num_threads;
    dcontext_t *my_dcontext = get_thread_private_dcontext();
    int i;

    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    CLIENT_ASSERT(OWN_NO_LOCKS(my_dcontext),
                  "dr_suspend_all_other_threads cannot be called while holding a lock");
    CLIENT_ASSERT(drcontexts != NULL && num_suspended != NULL,
                  "dr_suspend_all_other_threads invalid params");
    LOG(GLOBAL, LOG_FRAGMENT, 2,
        "\ndr_suspend_all_other_threads: thread " TIDFMT " suspending all threads\n",
        d_r_get_thread_id());

    /* suspend all DR-controlled threads at safe locations */
    if (!synch_with_all_threads(THREAD_SYNCH_SUSPENDED_VALID_MCONTEXT_OR_NO_XFER,
                                &threads, &num_threads, THREAD_SYNCH_NO_LOCKS_NO_XFER,
                                /* if we fail to suspend a thread (e.g., for
                                 * privilege reasons), ignore and continue
                                 */
                                THREAD_SYNCH_SUSPEND_FAILURE_IGNORE)) {
        LOG(GLOBAL, LOG_FRAGMENT, 2,
            "\ndr_suspend_all_other_threads: failed to suspend every thread\n");
        /* some threads may have been successfully suspended so we must return
         * their info so they'll be resumed.  I believe there is thus no
         * scenario under which we return false.
         */
    }
    /* now we own the thread_initexit_lock */
    CLIENT_ASSERT(OWN_MUTEX(&all_threads_synch_lock) && OWN_MUTEX(&thread_initexit_lock),
                  "internal locking error");

    /* To avoid two passes we allocate the array now.  It may be larger than
     * necessary if we had suspend failures but taht's ok.
     * We hide the threads num and array in extra slots.
     */
    *drcontexts = (void **)global_heap_alloc(
        (num_threads + 2) * sizeof(dcontext_t *) HEAPACCT(ACCT_THREAD_MGT));
    for (i = 0; i < num_threads; i++) {
        dcontext_t *dcontext = threads[i]->dcontext;
        if (dcontext != NULL) { /* include my_dcontext here */
            if (dcontext != my_dcontext) {
                /* must translate BEFORE freeing any memory! */
                if (!thread_synch_successful(threads[i])) {
                    out_unsuspended++;
                } else if (is_thread_currently_native(threads[i]) &&
                           !TEST(DR_SUSPEND_NATIVE, flags)) {
                    out_unsuspended++;
                } else if (thread_synch_state_no_xfer(dcontext)) {
                    /* FIXME: for all other synchall callers, the app
                     * context should be sitting in their mcontext, even
                     * though we can't safely get their native context and
                     * translate it.
                     */
                    (*drcontexts)[out_suspended] = (void *)dcontext;
                    out_suspended++;
                    CLIENT_ASSERT(!dcontext->client_data->mcontext_in_dcontext,
                                  "internal inconsistency in where mcontext is");
                    /* officially get_mcontext() doesn't always set pc: we do anyway */
                    get_mcontext(dcontext)->pc = dcontext->next_tag;
                    dcontext->client_data->mcontext_in_dcontext = true;
                } else {
                    (*drcontexts)[out_suspended] = (void *)dcontext;
                    out_suspended++;
                    /* It's not safe to clobber the thread's mcontext with
                     * its own translation b/c for shared_syscall we store
                     * the continuation pc in the esi slot.
                     * We could translate here into heap-allocated memory,
                     * but some clients may just want to stop
                     * the world but not examine the threads, so we lazily
                     * translate in dr_get_mcontext().
                     */
                    CLIENT_ASSERT(!dcontext->client_data->suspended,
                                  "inconsistent usage of dr_suspend_all_other_threads");
                    CLIENT_ASSERT(dcontext->client_data->cur_mc == NULL,
                                  "inconsistent usage of dr_suspend_all_other_threads");
                    dcontext->client_data->suspended = true;
                }
            }
        }
    }
    /* Hide the two extra vars we need the client to pass back to us */
    (*drcontexts)[out_suspended] = (void *)threads;
    (*drcontexts)[out_suspended + 1] = (void *)(ptr_uint_t)num_threads;
    *num_suspended = out_suspended;
    if (num_unsuspended != NULL)
        *num_unsuspended = out_unsuspended;
    return true;
}

DR_API
bool
dr_suspend_all_other_threads(OUT void ***drcontexts, OUT uint *num_suspended,
                             OUT uint *num_unsuspended)
{
    return dr_suspend_all_other_threads_ex(drcontexts, num_suspended, num_unsuspended, 0);
}

bool
dr_resume_all_other_threads(IN void **drcontexts, IN uint num_suspended)
{
    thread_record_t **threads;
    int num_threads;
    uint i;
    CLIENT_ASSERT(drcontexts != NULL, "dr_suspend_all_other_threads invalid params");
    LOG(GLOBAL, LOG_FRAGMENT, 2, "dr_resume_all_other_threads\n");
    threads = (thread_record_t **)drcontexts[num_suspended];
    num_threads = (int)(ptr_int_t)drcontexts[num_suspended + 1];
    for (i = 0; i < num_suspended; i++) {
        dcontext_t *dcontext = (dcontext_t *)drcontexts[i];
        if (dcontext->client_data->cur_mc != NULL) {
            /* clear any cached mc from dr_get_mcontext_priv() */
            heap_free(dcontext, dcontext->client_data->cur_mc,
                      sizeof(*dcontext->client_data->cur_mc) HEAPACCT(ACCT_CLIENT));
            dcontext->client_data->cur_mc = NULL;
        }
        dcontext->client_data->suspended = false;
    }
    global_heap_free(drcontexts,
                     (num_threads + 2) * sizeof(dcontext_t *) HEAPACCT(ACCT_THREAD_MGT));
    end_synch_with_all_threads(threads, num_threads, true /*resume*/);
    return true;
}

DR_API
bool
dr_is_thread_native(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "invalid param");
    return is_thread_currently_native(dcontext->thread_record);
}

DR_API
bool
dr_retakeover_suspended_native_thread(void *drcontext)
{
    bool res;
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "invalid param");
    /* XXX: I don't quite see why I need to pop these 2 when I'm doing
     * what a regular retakeover would do
     */
    KSTOP_NOT_MATCHING_DC(dcontext, fcache_default);
    KSTOP_NOT_MATCHING_DC(dcontext, dispatch_num_exits);
    res = os_thread_take_over_suspended_native(dcontext);
    return res;
}

#ifdef UNIX
DR_API
bool
dr_set_itimer(int which, uint millisec,
              void (*func)(void *drcontext, dr_mcontext_t *mcontext))
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    if (func == NULL && millisec != 0)
        return false;
    return set_itimer_callback(dcontext, which, millisec, NULL,
                               (void (*)(dcontext_t *, dr_mcontext_t *))func);
}

DR_API
uint
dr_get_itimer(int which)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    return get_itimer_frequency(dcontext, which);
}
#endif /* UNIX */

DR_API
void
dr_track_where_am_i(void)
{
    track_where_am_i = true;
}

bool
should_track_where_am_i(void)
{
    return track_where_am_i || DYNAMO_OPTION(profile_pcs);
}

DR_API
bool
dr_is_tracking_where_am_i(void)
{
    return should_track_where_am_i();
}

DR_API
dr_where_am_i_t
dr_where_am_i(void *drcontext, app_pc pc, OUT void **tag_out)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "invalid param");
    void *tag = NULL;
    dr_where_am_i_t whereami = dcontext->whereami;
    /* Further refine if pc is in the cache. */
    if (whereami == DR_WHERE_FCACHE) {
        fragment_t *fragment;
        whereami = fcache_refine_whereami(dcontext, whereami, pc, &fragment);
        if (fragment != NULL)
            tag = fragment->tag;
    }
    if (tag_out != NULL)
        *tag_out = tag;
    return whereami;
}

DR_API
void
instrlist_meta_fault_preinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_meta_may_fault(inst, true);
    instrlist_preinsert(ilist, where, inst);
}

DR_API
void
instrlist_meta_fault_postinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_meta_may_fault(inst, true);
    instrlist_postinsert(ilist, where, inst);
}

DR_API
void
instrlist_meta_fault_append(instrlist_t *ilist, instr_t *inst)
{
    instr_set_meta_may_fault(inst, true);
    instrlist_append(ilist, inst);
}

static void
convert_va_list_to_opnd(dcontext_t *dcontext, opnd_t **args, uint num_args, va_list ap)
{
    uint i;
    ASSERT(num_args > 0);
    /* allocate at least one argument opnd */
    /* we don't check for GLOBAL_DCONTEXT since DR internally calls this */
    *args = HEAP_ARRAY_ALLOC(dcontext, opnd_t, num_args, ACCT_CLEANCALL, UNPROTECTED);
    for (i = 0; i < num_args; i++) {
        (*args)[i] = va_arg(ap, opnd_t);
        CLIENT_ASSERT(opnd_is_valid((*args)[i]),
                      "Call argument: bad operand. Did you create a valid opnd_t?");
    }
}

static void
free_va_opnd_list(dcontext_t *dcontext, uint num_args, opnd_t *args)
{
    if (num_args != 0) {
        HEAP_ARRAY_FREE(dcontext, args, opnd_t, num_args, ACCT_CLEANCALL, UNPROTECTED);
    }
}

/* dr_insert_* are used by general DR */
/* Inserts a complete call to callee with the passed-in arguments */
void
dr_insert_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
               uint num_args, ...)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    opnd_t *args = NULL;
    instr_t *label = INSTR_CREATE_label(drcontext);
    dr_pred_type_t auto_pred = instrlist_get_auto_predicate(ilist);
    va_list ap;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_call: drcontext cannot be NULL");
    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
#ifdef ARM
    if (instr_predicate_is_cond(auto_pred)) {
        /* auto_predicate is set, though we handle the clean call with a cbr
         * because we require inserting instrumentation which modifies cpsr.
         */
        MINSERT(ilist, where,
                XINST_CREATE_jump_cond(drcontext, instr_invert_predicate(auto_pred),
                                       opnd_create_instr(label)));
    }
#endif
    if (num_args != 0) {
        va_start(ap, num_args);
        convert_va_list_to_opnd(dcontext, &args, num_args, ap);
        va_end(ap);
    }
    insert_meta_call_vargs(dcontext, ilist, where, META_CALL_RETURNS, vmcode_get_start(),
                           callee, num_args, args);
    if (num_args != 0)
        free_va_opnd_list(dcontext, num_args, args);
    MINSERT(ilist, where, label);
    instrlist_set_auto_predicate(ilist, auto_pred);
}

bool
dr_insert_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, byte *encode_pc,
                  void *callee, uint num_args, ...)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    opnd_t *args = NULL;
    bool direct;
    va_list ap;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_call: drcontext cannot be NULL");
    if (num_args != 0) {
        va_start(ap, num_args);
        convert_va_list_to_opnd(drcontext, &args, num_args, ap);
        va_end(ap);
    }
    direct = insert_meta_call_vargs(dcontext, ilist, where, META_CALL_RETURNS, encode_pc,
                                    callee, num_args, args);
    if (num_args != 0)
        free_va_opnd_list(dcontext, num_args, args);
    return direct;
}

/* Not exported.  Currently used for ARM to avoid storing to %lr. */
void
dr_insert_call_noreturn(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        uint num_args, ...)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    opnd_t *args = NULL;
    va_list ap;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_call_noreturn: drcontext cannot be NULL");
    CLIENT_ASSERT(instrlist_get_auto_predicate(ilist) == DR_PRED_NONE,
                  "Does not support auto-predication");
    if (num_args != 0) {
        va_start(ap, num_args);
        convert_va_list_to_opnd(dcontext, &args, num_args, ap);
        va_end(ap);
    }
    insert_meta_call_vargs(dcontext, ilist, where, 0, vmcode_get_start(), callee,
                           num_args, args);
    if (num_args != 0)
        free_va_opnd_list(dcontext, num_args, args);
}

/* Internal utility routine for inserting context save for a clean call.
 * Returns the size of the data stored on the DR stack
 * (in case the caller needs to align the stack pointer).
 * XSP and XAX are modified by this call.
 */
static uint
prepare_for_call_ex(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                    instr_t *where, byte *encode_pc)
{
    instr_t *in;
    uint dstack_offs;
    in = (where == NULL) ? instrlist_last(ilist) : instr_get_prev(where);
    dstack_offs = prepare_for_clean_call(dcontext, cci, ilist, where, encode_pc);
    /* now go through and mark inserted instrs as meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != where) {
        instr_set_meta(in);
        in = instr_get_next(in);
    }
    return dstack_offs;
}

/* Internal utility routine for inserting context restore for a clean call. */
static void
cleanup_after_call_ex(dcontext_t *dcontext, clean_call_info_t *cci, instrlist_t *ilist,
                      instr_t *where, uint sizeof_param_area, byte *encode_pc)
{
    instr_t *in;
    in = (where == NULL) ? instrlist_last(ilist) : instr_get_prev(where);
    if (sizeof_param_area > 0) {
        /* clean up the parameter area */
        CLIENT_ASSERT(sizeof_param_area <= 127,
                      "cleanup_after_call_ex: sizeof_param_area must be <= 127");
        /* mark it meta down below */
        instrlist_preinsert(ilist, where,
                            XINST_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                                             OPND_CREATE_INT8(sizeof_param_area)));
    }
    cleanup_after_clean_call(dcontext, cci, ilist, where, encode_pc);
    /* now go through and mark inserted instrs as meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != where) {
        instr_set_meta(in);
        in = instr_get_next(in);
    }
}

/* Inserts a complete call to callee with the passed-in arguments, wrapped
 * by an app save and restore.
 *
 * If "save_flags" includes DR_CLEANCALL_SAVE_FLOAT, saves the fp/mmx/sse state.
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_prepare_for_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 *
 * NOTE : dr_insert_cbr_instrumentation has assumption about the clean call
 * instrumentation layout, changes to the clean call instrumentation may break
 * dr_insert_cbr_instrumentation.
 */
void
dr_insert_clean_call_ex_varg(void *drcontext, instrlist_t *ilist, instr_t *where,
                             void *callee, dr_cleancall_save_t save_flags, uint num_args,
                             opnd_t *args)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    uint dstack_offs, pad = 0;
    size_t buf_sz = 0;
    clean_call_info_t cci; /* information for clean call insertion. */
    bool save_fpstate = TEST(DR_CLEANCALL_SAVE_FLOAT, save_flags);
    meta_call_flags_t call_flags = META_CALL_CLEAN | META_CALL_RETURNS;
    byte *encode_pc;
    instr_t *label = INSTR_CREATE_label(drcontext);
    dr_pred_type_t auto_pred = instrlist_get_auto_predicate(ilist);
    instr_t *insert_at = where;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_clean_call: drcontext cannot be NULL");
    STATS_INC(cleancall_inserted);
    LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: insert clean call to " PFX "\n", callee);

    if (clean_call_insertion_callbacks.num > 0) {
        /* Some libraries need to save and restore around the call, for which we want
         * a single instr to focus on that will put the post-call additions in the
         * right place instead of after 'where'.
         * This assumes the code below inserts all instructions prior to 'insert_at'
         * and none are appended.
         */
        instr_t *mark = INSTR_CREATE_label(drcontext);
        instr_set_note(mark, (void *)DR_NOTE_CLEAN_CALL_END);
        MINSERT(ilist, where, mark);
        insert_at = mark;
        call_all(clean_call_insertion_callbacks,
                 int (*)(void *, instrlist_t *, instr_t *, dr_cleancall_save_t),
                 (void *)dcontext, ilist, mark, save_flags);
    }

    instrlist_set_auto_predicate(ilist, DR_PRED_NONE);
#ifdef ARM
    if (instr_predicate_is_cond(auto_pred)) {
        /* auto_predicate is set, though we handle the clean call with a cbr
         * because we require inserting instrumentation which modifies cpsr.
         * We don't need to set DR_CLEANCALL_MULTIPATH because this jump is
         * *after* the register restores just above here.
         */
        MINSERT(ilist, insert_at,
                XINST_CREATE_jump_cond(drcontext, instr_invert_predicate(auto_pred),
                                       opnd_create_instr(label)));
    }
#endif
    /* analyze the clean call, return true if clean call can be inlined. */
    if (analyze_clean_call(dcontext, &cci, insert_at, callee, save_fpstate,
                           TEST(DR_CLEANCALL_ALWAYS_OUT_OF_LINE, save_flags), num_args,
                           args) &&
        !TEST(DR_CLEANCALL_ALWAYS_OUT_OF_LINE, save_flags)) {
        /* we can perform the inline optimization and return. */
        STATS_INC(cleancall_inlined);
        LOG(THREAD, LOG_CLEANCALL, 2, "CLEANCALL: inlined callee " PFX "\n", callee);
        insert_inline_clean_call(dcontext, &cci, ilist, insert_at, args);
        MINSERT(ilist, insert_at, label);
        instrlist_set_auto_predicate(ilist, auto_pred);
        return;
    }
    /* honor requests from caller */
    if (TEST(DR_CLEANCALL_NOSAVE_FLAGS, save_flags)) {
        /* even if we remove flag saves we want to keep mcontext shape */
        cci.preserve_mcontext = true;
        cci.skip_save_flags = true;
        /* we assume this implies DF should be 0 already */
        cci.skip_clear_flags = true;
        /* XXX: should also provide DR_CLEANCALL_NOSAVE_NONAFLAGS to
         * preserve just arith flags on return from a call
         */
    }
    if (TESTANY(DR_CLEANCALL_NOSAVE_XMM | DR_CLEANCALL_NOSAVE_XMM_NONPARAM |
                    DR_CLEANCALL_NOSAVE_XMM_NONRET,
                save_flags)) {
        int i;
        /* even if we remove xmm saves we want to keep mcontext shape */
        cci.preserve_mcontext = true;
        /* start w/ all */
#if defined(X64) && defined(WINDOWS)
        cci.num_simd_skip = 6;
#else
        /* all 8, 16 or 32 are scratch */
        cci.num_simd_skip = proc_num_simd_registers();
#endif
        for (i = 0; i < cci.num_simd_skip; i++)
            cci.simd_skip[i] = true;
#ifdef X86
        cci.num_opmask_skip = proc_num_opmask_registers();
        for (i = 0; i < cci.num_opmask_skip; i++)
            cci.opmask_skip[i] = true;
#endif
            /* now remove those used for param/retval */
#ifdef X64
        if (TEST(DR_CLEANCALL_NOSAVE_XMM_NONPARAM, save_flags)) {
            /* xmm0-3 (-7 for linux) are used for params */
            for (i = 0; i < IF_UNIX_ELSE(7, 3); i++)
                cci.simd_skip[i] = false;
            cci.num_simd_skip -= i;
        }
        if (TEST(DR_CLEANCALL_NOSAVE_XMM_NONRET, save_flags)) {
            /* xmm0 (and xmm1 for linux) are used for retvals */
            cci.simd_skip[0] = false;
            cci.num_simd_skip--;
#    ifdef UNIX
            cci.simd_skip[1] = false;
            cci.num_simd_skip--;
#    endif
        }
#endif
    }
    if (TEST(DR_CLEANCALL_INDIRECT, save_flags))
        encode_pc = vmcode_unreachable_pc();
    else
        encode_pc = vmcode_get_start();
    dstack_offs = prepare_for_call_ex(dcontext, &cci, ilist, insert_at, encode_pc);
    /* PR 218790: we assume that dr_prepare_for_call() leaves stack 16-byte
     * aligned, which is what insert_meta_call_vargs requires.
     */
    if (cci.should_align) {
        CLIENT_ASSERT(ALIGNED(dstack_offs, get_ABI_stack_alignment()),
                      "internal error: bad stack alignment");
    }
    if (save_fpstate) {
        /* save on the stack: xref PR 202669 on clients using more stack */
        buf_sz = proc_fpstate_save_size();
        /* we need 16-byte-alignment */
        pad = ALIGN_FORWARD_UINT(dstack_offs, 16) - dstack_offs;
        IF_X64(CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_int(buf_sz + pad),
                             "dr_insert_clean_call: internal truncation error"));
        MINSERT(ilist, insert_at,
                XINST_CREATE_sub(dcontext, opnd_create_reg(REG_XSP),
                                 OPND_CREATE_INT32((int)(buf_sz + pad))));
        dr_insert_save_fpstate(drcontext, ilist, insert_at,
                               opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0, OPSZ_512));
    }

    /* PR 302951: restore state if clean call args reference app memory.
     * We use a hack here: this is the only instance insert_at we mark as our-mangling
     * but do not have a translation target set, which indicates to the restore
     * routines that this is a clean call.  If the client adds instrs in the middle
     * translation will fail; if the client modifies any instr, the our-mangling
     * flag will disappear and translation will fail.
     */
    instrlist_set_our_mangling(ilist, true);
    if (TEST(DR_CLEANCALL_RETURNS_TO_NATIVE, save_flags))
        call_flags |= META_CALL_RETURNS_TO_NATIVE;
    insert_meta_call_vargs(dcontext, ilist, insert_at, call_flags, encode_pc, callee,
                           num_args, args);
    instrlist_set_our_mangling(ilist, false);

    if (save_fpstate) {
        dr_insert_restore_fpstate(
            drcontext, ilist, insert_at,
            opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0, OPSZ_512));
        MINSERT(ilist, insert_at,
                XINST_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                                 OPND_CREATE_INT32(buf_sz + pad)));
    }
    cleanup_after_call_ex(dcontext, &cci, ilist, insert_at, 0, encode_pc);
    MINSERT(ilist, insert_at, label);
    instrlist_set_auto_predicate(ilist, auto_pred);
}

void
dr_insert_clean_call_ex(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                        dr_cleancall_save_t save_flags, uint num_args, ...)
{
    opnd_t *args = NULL;
    if (num_args != 0) {
        va_list ap;
        va_start(ap, num_args);
        convert_va_list_to_opnd(drcontext, &args, num_args, ap);
        va_end(ap);
    }
    dr_insert_clean_call_ex_varg(drcontext, ilist, where, callee, save_flags, num_args,
                                 args);
    if (num_args != 0)
        free_va_opnd_list(drcontext, num_args, args);
}

DR_API
void
dr_insert_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where, void *callee,
                     bool save_fpstate, uint num_args, ...)
{
    dr_cleancall_save_t flags = (save_fpstate ? DR_CLEANCALL_SAVE_FLOAT : 0);
    opnd_t *args = NULL;
    if (num_args != 0) {
        va_list ap;
        va_start(ap, num_args);
        convert_va_list_to_opnd(drcontext, &args, num_args, ap);
        va_end(ap);
    }
    dr_insert_clean_call_ex_varg(drcontext, ilist, where, callee, flags, num_args, args);
    if (num_args != 0)
        free_va_opnd_list(drcontext, num_args, args);
}

/* Utility routine for inserting a clean call to an instrumentation routine
 * Returns the size of the data stored on the DR stack (in case the caller
 * needs to align the stack pointer).  XSP and XAX are modified by this call.
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * prepare_for_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 */
DR_API uint
dr_prepare_for_call(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    CLIENT_ASSERT(drcontext != NULL, "dr_prepare_for_call: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_prepare_for_call: drcontext is invalid");
    return prepare_for_call_ex((dcontext_t *)drcontext, NULL, ilist, where,
                               vmcode_get_start());
}

DR_API void
dr_cleanup_after_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                      uint sizeof_param_area)
{
    CLIENT_ASSERT(drcontext != NULL, "dr_cleanup_after_call: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_cleanup_after_call: drcontext is invalid");
    cleanup_after_call_ex((dcontext_t *)drcontext, NULL, ilist, where, sizeof_param_area,
                          vmcode_get_start());
}

DR_API void
dr_swap_to_clean_stack(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_swap_to_clean_stack: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_swap_to_clean_stack: drcontext is invalid");

    /* PR 219620: For thread-shared, we need to get the dcontext
     * dynamically rather than use the constant passed in here.
     */
    if (SCRATCH_ALWAYS_TLS()) {
        MINSERT(ilist, where,
                instr_create_save_to_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
        insert_get_mcontext_base(dcontext, ilist, where, SCRATCH_REG0);
        /* save app xsp, and then bring in dstack to xsp */
        MINSERT(
            ilist, where,
            instr_create_save_to_dc_via_reg(dcontext, SCRATCH_REG0, REG_XSP, XSP_OFFSET));
        /* DSTACK_OFFSET isn't within the upcontext so if it's separate this won't
         * work right.  FIXME - the dcontext accessing routines are a mess of shared
         * vs. no shared support, separate context vs. no separate context support etc. */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
        MINSERT(ilist, where,
                instr_create_restore_from_dc_via_reg(dcontext, SCRATCH_REG0, REG_XSP,
                                                     DSTACK_OFFSET));
        MINSERT(ilist, where,
                instr_create_restore_from_tls(dcontext, SCRATCH_REG0, TLS_REG0_SLOT));
    } else {
        MINSERT(ilist, where,
                instr_create_save_to_dcontext(dcontext, REG_XSP, XSP_OFFSET));
        MINSERT(ilist, where, instr_create_restore_dynamo_stack(dcontext));
    }
}

DR_API void
dr_restore_app_stack(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_app_stack: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_app_stack: drcontext is invalid");
    /* restore stack */
    if (SCRATCH_ALWAYS_TLS()) {
        /* use the register we're about to clobber as scratch space */
        insert_get_mcontext_base(dcontext, ilist, where, REG_XSP);
        MINSERT(
            ilist, where,
            instr_create_restore_from_dc_via_reg(dcontext, REG_XSP, REG_XSP, XSP_OFFSET));
    } else {
        MINSERT(ilist, where,
                instr_create_restore_from_dcontext(dcontext, REG_XSP, XSP_OFFSET));
    }
}

#define SPILL_SLOT_TLS_MAX 2
#define NUM_TLS_SPILL_SLOTS (SPILL_SLOT_TLS_MAX + 1)
#define NUM_SPILL_SLOTS (SPILL_SLOT_MAX + 1)
/* The three tls slots we make available to clients.  We reserve TLS_REG0_SLOT for our
 * own use in dr convenience routines. Note the +1 is because the max is an array index
 * (so zero based) while array size is number of slots.  We don't need to +1 in
 * SPILL_SLOT_MC_REG because subtracting SPILL_SLOT_TLS_MAX already accounts for it. */
static const ushort SPILL_SLOT_TLS_OFFS[NUM_TLS_SPILL_SLOTS] = { TLS_REG3_SLOT,
                                                                 TLS_REG2_SLOT,
                                                                 TLS_REG1_SLOT };
static const reg_id_t SPILL_SLOT_MC_REG[NUM_SPILL_SLOTS - NUM_TLS_SPILL_SLOTS] = {
#ifdef X86
/* The dcontext reg slots we make available to clients.  We reserve XAX and XSP
 * for our own use in dr convenience routines. */
#    ifdef X64
    REG_R15, REG_R14, REG_R13, REG_R12, REG_R11, REG_R10, REG_R9, REG_R8,
#    endif
    REG_XDI, REG_XSI, REG_XBP, REG_XDX, REG_XCX, REG_XBX
#elif defined(AARCHXX)
    /* DR_REG_R0 is not used here. See prepare_for_clean_call. */
    DR_REG_R6, DR_REG_R5, DR_REG_R4, DR_REG_R3, DR_REG_R2, DR_REG_R1
#elif defined(RISCV64)
    DR_REG_A6, DR_REG_A5, DR_REG_A4, DR_REG_A3, DR_REG_A2, DR_REG_A1
#endif /* X86/ARM */
};

DR_API void
dr_save_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
            dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_save_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_save_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX, "dr_save_reg: invalid spill slot selection");

    CLIENT_ASSERT(reg_is_pointer_sized(reg), "dr_save_reg requires pointer-sized gpr");

#ifdef AARCH64
    CLIENT_ASSERT(reg != DR_REG_XSP, "dr_save_reg: store from XSP is not supported");
#endif

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        MINSERT(ilist, where,
                XINST_CREATE_store(dcontext, opnd_create_tls_slot(offs),
                                   opnd_create_reg(reg)));
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        if (SCRATCH_ALWAYS_TLS()) {
            /* PR 219620: For thread-shared, we need to get the dcontext
             * dynamically rather than use the constant passed in here.
             */
            reg_id_t tmp = (reg == SCRATCH_REG0) ? SCRATCH_REG1 : SCRATCH_REG0;

            MINSERT(ilist, where, instr_create_save_to_tls(dcontext, tmp, TLS_REG0_SLOT));

            insert_get_mcontext_base(dcontext, ilist, where, tmp);

            MINSERT(ilist, where,
                    instr_create_save_to_dc_via_reg(dcontext, tmp, reg, offs));

            MINSERT(ilist, where,
                    instr_create_restore_from_tls(dcontext, tmp, TLS_REG0_SLOT));
        } else {
            MINSERT(ilist, where, instr_create_save_to_dcontext(dcontext, reg, offs));
        }
    }
}

/* if want to save 8 or 16-bit reg, must pass in containing ptr-sized reg! */
DR_API void
dr_restore_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
               dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_restore_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX, "dr_restore_reg: invalid spill slot selection");

    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_restore_reg requires a pointer-sized gpr");

#ifdef AARCH64
    CLIENT_ASSERT(reg != DR_REG_XSP, "dr_restore_reg: load into XSP is not supported");
#endif

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        MINSERT(ilist, where,
                XINST_CREATE_load(dcontext, opnd_create_reg(reg),
                                  opnd_create_tls_slot(offs)));
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        if (SCRATCH_ALWAYS_TLS()) {
            /* PR 219620: For thread-shared, we need to get the dcontext
             * dynamically rather than use the constant passed in here.
             */
            /* use the register we're about to clobber as scratch space */
            insert_get_mcontext_base(dcontext, ilist, where, reg);

            MINSERT(ilist, where,
                    instr_create_restore_from_dc_via_reg(dcontext, reg, reg, offs));
        } else {
            MINSERT(ilist, where,
                    instr_create_restore_from_dcontext(dcontext, reg, offs));
        }
    }
}

DR_API dr_spill_slot_t
dr_max_opnd_accessible_spill_slot()
{
    if (SCRATCH_ALWAYS_TLS())
        return SPILL_SLOT_TLS_MAX;
    else
        return SPILL_SLOT_MAX;
}

/* creates an opnd to access spill slot slot, slot must be <=
 * dr_max_opnd_accessible_spill_slot() */
opnd_t
reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        return opnd_create_tls_slot(offs);
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        ASSERT(!SCRATCH_ALWAYS_TLS()); /* client assert above should catch */
        return opnd_create_dcontext_field(dcontext, offs);
    }
}

DR_API
opnd_t
dr_reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_reg_spill_slot_opnd: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_reg_spill_slot_opnd: drcontext is invalid");
    CLIENT_ASSERT(slot <= dr_max_opnd_accessible_spill_slot(),
                  "dr_reg_spill_slot_opnd: slot must be less than "
                  "dr_max_opnd_accessible_spill_slot()");
    return reg_spill_slot_opnd(dcontext, slot);
}

DR_API
/* used to read a saved register spill slot from a clean call or a restore_state_event */
reg_t
dr_read_saved_reg(void *drcontext, dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    CLIENT_ASSERT(drcontext != NULL, "dr_read_saved_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_read_saved_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_read_saved_reg: invalid spill slot selection");
    /* We do allow drcontext to not belong to the current thread, for state restoration
     * during synchall and other scenarios.
     */

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = SPILL_SLOT_TLS_OFFS[slot];
        return *(reg_t *)(((byte *)&dcontext->local_state->spill_space) + offs);
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        return reg_get_value_priv(reg_slot, get_mcontext(dcontext));
    }
}

DR_API
/* used to write a saved register spill slot from a clean call */
void
dr_write_saved_reg(void *drcontext, dr_spill_slot_t slot, reg_t value)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    CLIENT_ASSERT(drcontext != NULL, "dr_write_saved_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_write_saved_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_write_saved_reg: invalid spill slot selection");
    /* We do allow drcontext to not belong to the current thread, for state restoration
     * during synchall and other scenarios.
     */

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = SPILL_SLOT_TLS_OFFS[slot];
        *(reg_t *)(((byte *)&dcontext->local_state->spill_space) + offs) = value;
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        reg_set_value_priv(reg_slot, get_mcontext(dcontext), value);
    }
}

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to read into the
 * general-purpose full-size register reg from the user-controlled drcontext
 * field for this thread.
 */
void
dr_insert_read_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                         reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_read_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    if (SCRATCH_ALWAYS_TLS()) {
        /* For thread-shared, since reg must be general-purpose we can
         * use it as a base pointer (repeatedly).  Plus it's already dead.
         */
        MINSERT(ilist, where,
                instr_create_restore_from_tls(dcontext, reg, TLS_DCONTEXT_SLOT));
        MINSERT(
            ilist, where,
            instr_create_restore_from_dc_via_reg(dcontext, reg, reg, CLIENT_DATA_OFFSET));
        MINSERT(ilist, where,
                XINST_CREATE_load(
                    dcontext, opnd_create_reg(reg),
                    OPND_CREATE_MEMPTR(reg, offsetof(client_data_t, user_field))));
    } else {
        MINSERT(ilist, where,
                XINST_CREATE_load(
                    dcontext, opnd_create_reg(reg),
                    OPND_CREATE_ABSMEM(&dcontext->client_data->user_field, OPSZ_PTR)));
    }
}

DR_API
/**
 * Inserts into ilist prior to "where" instruction(s) to write the
 * general-purpose full-size register reg to the user-controlled drcontext field
 * for this thread.
 */
void
dr_insert_write_tls_field(void *drcontext, instrlist_t *ilist, instr_t *where,
                          reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_write_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    if (SCRATCH_ALWAYS_TLS()) {
        reg_id_t spill = SCRATCH_REG0;
        if (reg == spill) /* don't need sub-reg test b/c we know it's pointer-sized */
            spill = SCRATCH_REG1;
        MINSERT(ilist, where, instr_create_save_to_tls(dcontext, spill, TLS_REG0_SLOT));
        MINSERT(ilist, where,
                instr_create_restore_from_tls(dcontext, spill, TLS_DCONTEXT_SLOT));
        MINSERT(ilist, where,
                instr_create_restore_from_dc_via_reg(dcontext, spill, spill,
                                                     CLIENT_DATA_OFFSET));
        MINSERT(ilist, where,
                XINST_CREATE_store(
                    dcontext,
                    OPND_CREATE_MEMPTR(spill, offsetof(client_data_t, user_field)),
                    opnd_create_reg(reg)));
        MINSERT(ilist, where,
                instr_create_restore_from_tls(dcontext, spill, TLS_REG0_SLOT));
    } else {
        MINSERT(ilist, where,
                XINST_CREATE_store(
                    dcontext,
                    OPND_CREATE_ABSMEM(&dcontext->client_data->user_field, OPSZ_PTR),
                    opnd_create_reg(reg)));
    }
}

DR_API void
dr_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                    dr_spill_slot_t slot)
{
    reg_id_t reg = IF_X86_ELSE(DR_REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0));
    CLIENT_ASSERT(IF_X86_ELSE(true, false), "X86-only");
    CLIENT_ASSERT(drcontext != NULL, "dr_save_arith_flags: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_save_arith_flags: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_save_arith_flags: invalid spill slot selection");
    dr_save_reg(drcontext, ilist, where, reg, slot);
    dr_save_arith_flags_to_reg(drcontext, ilist, where, reg);
}

DR_API void
dr_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                       dr_spill_slot_t slot)
{
    reg_id_t reg = IF_X86_ELSE(DR_REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0));
    CLIENT_ASSERT(IF_X86_ELSE(true, false), "X86-only");
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_arith_flags: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_arith_flags: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_restore_arith_flags: invalid spill slot selection");
    dr_restore_arith_flags_from_reg(drcontext, ilist, where, reg);
    dr_restore_reg(drcontext, ilist, where, reg, slot);
}

DR_API void
dr_save_arith_flags_to_xax(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    reg_id_t reg = IF_X86_ELSE(DR_REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0));
    CLIENT_ASSERT(IF_X86_ELSE(true, false), "X86-only");
    dr_save_arith_flags_to_reg(drcontext, ilist, where, reg);
}

DR_API void
dr_restore_arith_flags_from_xax(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    reg_id_t reg = IF_X86_ELSE(DR_REG_XAX, IF_RISCV64_ELSE(DR_REG_A0, DR_REG_R0));
    CLIENT_ASSERT(IF_X86_ELSE(true, false), "X86-only");
    dr_restore_arith_flags_from_reg(drcontext, ilist, where, reg);
}

DR_API void
dr_save_arith_flags_to_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                           reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_save_arith_flags_to_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_save_arith_flags_to_reg: drcontext is invalid");
#ifdef X86
    CLIENT_ASSERT(reg == DR_REG_XAX,
                  "only xax should be used for save arith flags in X86");
    /* flag saving code:
     *   lahf
     *   seto al
     */
    MINSERT(ilist, where, INSTR_CREATE_lahf(dcontext));
    MINSERT(ilist, where, INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
#elif defined(ARM)
    /* flag saving code: mrs reg, cpsr */
    MINSERT(
        ilist, where,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(reg), opnd_create_reg(DR_REG_CPSR)));
#elif defined(AARCH64)
    /* flag saving code: mrs reg, nzcv */
    MINSERT(
        ilist, where,
        INSTR_CREATE_mrs(dcontext, opnd_create_reg(reg), opnd_create_reg(DR_REG_NZCV)));
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented. Perhaps float flags should be saved here? */
    ASSERT_NOT_IMPLEMENTED(false);
    /* Marking as unused to silence -Wunused-variable. */
    (void)dcontext;
#endif /* X86/ARM/AARCH64/RISCV64 */
}

DR_API void
dr_restore_arith_flags_from_reg(void *drcontext, instrlist_t *ilist, instr_t *where,
                                reg_id_t reg)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_restore_arith_flags_from_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_arith_flags_from_reg: drcontext is invalid");
#ifdef X86
    CLIENT_ASSERT(reg == DR_REG_XAX,
                  "only xax should be used for save arith flags in X86");
    /* flag restoring code:
     *   add 0x7f,%al
     *   sahf
     */
    /* do an add such that OF will be set only if seto set
     * the LSB of AL to 1
     */
    MINSERT(ilist, where,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    MINSERT(ilist, where, INSTR_CREATE_sahf(dcontext));
#elif defined(ARM)
    /* flag restoring code: mrs reg, apsr_nzcvqg */
    MINSERT(ilist, where,
            INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_CPSR),
                             OPND_CREATE_INT_MSR_NZCVQG(), opnd_create_reg(reg)));
#elif defined(AARCH64)
    /* flag restoring code: mrs reg, nzcv */
    MINSERT(
        ilist, where,
        INSTR_CREATE_msr(dcontext, opnd_create_reg(DR_REG_NZCV), opnd_create_reg(reg)));
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented. Perhaps float flags should be restored here? */
    ASSERT_NOT_IMPLEMENTED(false);
    /* Marking as unused to silence -Wunused-variable. */
    (void)dcontext;
#endif /* X86/ARM/AARCH64/RISCV64 */
}

DR_API reg_t
dr_merge_arith_flags(reg_t cur_xflags, reg_t saved_xflag)
{
#ifdef AARCHXX
    cur_xflags &= ~(EFLAGS_ARITH);
    cur_xflags |= saved_xflag;
#elif defined(X86)
    uint sahf = (saved_xflag & 0xff00) >> 8;
    cur_xflags &= ~(EFLAGS_ARITH);
    cur_xflags |= sahf;
    if (TEST(1, saved_xflag)) /* seto */
        cur_xflags |= EFLAGS_OF;
#endif

    return cur_xflags;
}

/* providing functionality of old -instr_calls and -instr_branches flags
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_insert_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 */
DR_API void
dr_insert_call_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               void *callee)
{
    ptr_uint_t target, address;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_call_instrumentation: drcontext cannot be NULL");
    address = (ptr_uint_t)instr_get_translation(instr);
    /* dr_insert_ubr_instrumentation() uses this function */
    CLIENT_ASSERT(instr_is_call(instr) || instr_is_ubr(instr),
                  "dr_insert_{ubr,call}_instrumentation must be applied to a ubr");
    CLIENT_ASSERT(address != 0,
                  "dr_insert_{ubr,call}_instrumentation: can't determine app address");
    if (opnd_is_pc(instr_get_target(instr))) {
        if (opnd_is_far_pc(instr_get_target(instr))) {
            /* FIXME: handle far pc */
            CLIENT_ASSERT(false,
                          "dr_insert_{ubr,call}_instrumentation: far pc not supported");
        }
        /* In release build for far pc keep going assuming 0 base */
        target = (ptr_uint_t)opnd_get_pc(instr_get_target(instr));
    } else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        target = (ptr_uint_t)instr_get_translation(tgt);
        CLIENT_ASSERT(target != 0,
                      "dr_insert_{ubr,call}_instrumentation: unknown target");
        if (opnd_is_far_instr(instr_get_target(instr))) {
            /* FIXME: handle far instr */
            CLIENT_ASSERT(false,
                          "dr_insert_{ubr,call}_instrumentation: far instr "
                          "not supported");
        }
    } else {
        CLIENT_ASSERT(false, "dr_insert_{ubr,call}_instrumentation: unknown target");
        target = 0;
    }

    dr_insert_clean_call_ex(
        drcontext, ilist, instr, callee,
        /* Many users will ask for mcontexts; some will set; it doesn't seem worth
         * asking the user to pass in a flag: if they're using this they are not
         * super concerned about overhead.
         */
        DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 2,
        /* address of call is 1st parameter */
        OPND_CREATE_INTPTR(address),
        /* call target is 2nd parameter */
        OPND_CREATE_INTPTR(target));
}

/* NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_insert_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched. Since we need another
 * tls spill slot in this routine we require the caller to give us one. */
DR_API void
dr_insert_mbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee, dr_spill_slot_t scratch_slot)
{
#ifdef X86
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    ptr_uint_t address = (ptr_uint_t)instr_get_translation(instr);
    opnd_t tls_opnd;
    instr_t *newinst;
    reg_id_t reg_target;

    /* PR 214051: dr_insert_mbr_instrumentation() broken with -indcall2direct */
    CLIENT_ASSERT(!DYNAMO_OPTION(indcall2direct),
                  "dr_insert_mbr_instrumentation not supported with -opt_speed");
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_mbr_instrumentation: drcontext cannot be NULL");
    CLIENT_ASSERT(address != 0,
                  "dr_insert_mbr_instrumentation: can't determine app address");
    CLIENT_ASSERT(instr_is_mbr(instr),
                  "dr_insert_mbr_instrumentation must be applied to an mbr");

    /* We need a TLS spill slot to use.  We can use any tls slot that is opnd
     * accessible. */
    CLIENT_ASSERT(scratch_slot <= dr_max_opnd_accessible_spill_slot(),
                  "dr_insert_mbr_instrumentation: scratch_slot must be less than "
                  "dr_max_opnd_accessible_spill_slot()");

    /* It is possible for mbr instruction to use XCX register, so we have
     * to use an unsed register.
     */
    for (reg_target = REG_XAX; reg_target <= REG_XBX; reg_target++) {
        if (!instr_uses_reg(instr, reg_target))
            break;
    }

    /* PR 240265: we disallow clients to add post-mbr instrumentation, so we
     * avoid doing that here even though it's a little less efficient since
     * our mbr mangling will re-grab the target.
     * We could keep it post-mbr and mark it w/ a special flag so we allow
     * our own but not clients' instrumentation post-mbr: but then we
     * hit post-syscall issues for wow64 where post-mbr equals post-syscall
     * (PR 240258: though we might solve that some other way).
     */

    /* Note that since we're using a client exposed slot we know it will be
     * preserved across the clean call. */
    tls_opnd = dr_reg_spill_slot_opnd(drcontext, scratch_slot);
    newinst = XINST_CREATE_store(dcontext, tls_opnd, opnd_create_reg(reg_target));

    /* PR 214962: ensure we'll properly translate the de-ref of app
     * memory by marking the spill and de-ref as INSTR_OUR_MANGLING.
     */
    instr_set_our_mangling(newinst, true);
    MINSERT(ilist, instr, newinst);

    if (instr_is_return(instr)) {
        /* the retaddr operand is always the final source for all OP_ret* instrs */
        opnd_t retaddr = instr_get_src(instr, instr_num_srcs(instr) - 1);
        opnd_size_t sz = opnd_get_size(retaddr);
        /* Even for far ret and iret, retaddr is at TOS
         * but operand size needs to be set to stack size
         * since iret pops more than return address.
         */
        opnd_set_size(&retaddr, OPSZ_STACK);
        newinst = instr_create_1dst_1src(dcontext, sz == OPSZ_2 ? OP_movzx : OP_mov_ld,
                                         opnd_create_reg(reg_target), retaddr);
    } else {
        /* call* or jmp* */
        opnd_t src = instr_get_src(instr, 0);
        opnd_size_t sz = opnd_get_size(src);
        /* if a far cti, we can't fit it into a register: asserted above.
         * in release build we'll get just the address here.
         */
        if (instr_is_far_cti(instr)) {
            if (sz == OPSZ_10) {
                sz = OPSZ_8;
            } else if (sz == OPSZ_6) {
                sz = OPSZ_4;
#    ifdef X64
                reg_target = reg_64_to_32(reg_target);
#    endif
            } else /* target has OPSZ_4 */ {
                sz = OPSZ_2;
            }
            opnd_set_size(&src, sz);
        }
#    ifdef UNIX
        /* xref i#1834 the problem with fs and gs segment is a general problem
         * on linux, this fix is specific for mbr_instrumentation, but a general
         * solution is needed.
         */
        if (INTERNAL_OPTION(mangle_app_seg) && opnd_is_far_base_disp(src)) {
            src = mangle_seg_ref_opnd(dcontext, ilist, instr, src, reg_target);
        }
#    endif

        newinst = instr_create_1dst_1src(dcontext, sz == OPSZ_2 ? OP_movzx : OP_mov_ld,
                                         opnd_create_reg(reg_target), src);
    }
    instr_set_our_mangling(newinst, true);
    MINSERT(ilist, instr, newinst);
    /* Now we want the true app state saved, for dr_get_mcontext().
     * We specially recognize our OP_xchg as a restore in
     * instr_is_reg_spill_or_restore().
     */
    MINSERT(ilist, instr,
            INSTR_CREATE_xchg(dcontext, tls_opnd, opnd_create_reg(reg_target)));

    dr_insert_clean_call_ex(
        drcontext, ilist, instr, callee,
        /* Many users will ask for mcontexts; some will set; it doesn't seem worth
         * asking the user to pass in a flag: if they're using this they are not
         * super concerned about overhead.
         */
        DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 2,
        /* address of mbr is 1st param */
        OPND_CREATE_INTPTR(address),
        /* indirect target (in tls, xchg-d from ecx) is 2nd param */
        tls_opnd);
#elif defined(ARM)
    /* i#1551: NYI on ARM.
     * Also, we may want to split these out into arch/{x86,arm}/ files
     */
    ASSERT_NOT_IMPLEMENTED(false);
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM/RISCV64 */
}

/* NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_insert_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 *
 * NOTE : this routine has assumption about the layout of the clean call,
 * so any change to clean call instrumentation layout may break this routine.
 */
static void
dr_insert_cbr_instrumentation_help(void *drcontext, instrlist_t *ilist, instr_t *instr,
                                   void *callee, bool has_fallthrough, opnd_t user_data)
{
#ifdef X86
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    ptr_uint_t address, target;
    int opc;
    instr_t *app_flags_ok;
    bool out_of_line_switch = false;
    ;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_cbr_instrumentation: drcontext cannot be NULL");
    address = (ptr_uint_t)instr_get_translation(instr);
    CLIENT_ASSERT(address != 0,
                  "dr_insert_cbr_instrumentation: can't determine app address");
    CLIENT_ASSERT(instr_is_cbr(instr),
                  "dr_insert_cbr_instrumentation must be applied to a cbr");
    CLIENT_ASSERT(opnd_is_near_pc(instr_get_target(instr)) ||
                      opnd_is_near_instr(instr_get_target(instr)),
                  "dr_insert_cbr_instrumentation: target opnd must be a near pc or "
                  "near instr");
    if (opnd_is_near_pc(instr_get_target(instr)))
        target = (ptr_uint_t)opnd_get_pc(instr_get_target(instr));
    else if (opnd_is_near_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        target = (ptr_uint_t)instr_get_translation(tgt);
        CLIENT_ASSERT(target != 0, "dr_insert_cbr_instrumentation: unknown target");
    } else {
        CLIENT_ASSERT(false, "dr_insert_cbr_instrumentation: unknown target");
        target = 0;
    }

    app_flags_ok = instr_get_prev(instr);
    if (has_fallthrough) {
        ptr_uint_t fallthrough = address + instr_length(drcontext, instr);
        CLIENT_ASSERT(!opnd_uses_reg(user_data, DR_REG_XBX),
                      "register ebx should not be used");
        CLIENT_ASSERT(fallthrough > address, "wrong fallthrough address");
        dr_insert_clean_call_ex(
            drcontext, ilist, instr, callee,
            /* Many users will ask for mcontexts; some will set; it doesn't seem worth
             * asking the user to pass in a flag: if they're using this they are not
             * super concerned about overhead.
             */
            DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 5,
            /* push address of mbr onto stack as 1st parameter */
            OPND_CREATE_INTPTR(address),
            /* target is 2nd parameter */
            OPND_CREATE_INTPTR(target),
            /* fall-throug is 3rd parameter */
            OPND_CREATE_INTPTR(fallthrough),
            /* branch direction (put in ebx below) is 4th parameter */
            opnd_create_reg(REG_XBX),
            /* user defined data is 5th parameter */
            opnd_is_null(user_data) ? OPND_CREATE_INT32(0) : user_data);
    } else {
        dr_insert_clean_call_ex(
            drcontext, ilist, instr, callee,
            /* Many users will ask for mcontexts; some will set; it doesn't seem worth
             * asking the user to pass in a flag: if they're using this they are not
             * super concerned about overhead.
             */
            DR_CLEANCALL_READS_APP_CONTEXT | DR_CLEANCALL_WRITES_APP_CONTEXT, 3,
            /* push address of mbr onto stack as 1st parameter */
            OPND_CREATE_INTPTR(address),
            /* target is 2nd parameter */
            OPND_CREATE_INTPTR(target),
            /* branch direction (put in ebx below) is 3rd parameter */
            opnd_create_reg(REG_XBX));
    }

    /* calculate whether branch taken or not
     * since the clean call mechanism clobbers eflags, we
     * must insert our checks prior to that clobbering.
     * since we do it AFTER the pusha, we don't have to save; but, we
     * can't use a param that's part of any calling convention b/c w/
     * PR 250976 our clean call will get it from the pusha.
     * ebx is a good choice.
     */
    /* We expect:
       mov    0x400e5e34 -> %esp
       pusha  %esp %eax %ebx %ecx %edx %ebp %esi %edi -> %esp (%esp)
       pushf  %esp -> %esp (%esp)
       push   $0x00000000 %esp -> %esp (%esp)
       popf   %esp (%esp) -> %esp
       mov    0x400e5e40 -> %eax
       push   %eax %esp -> %esp (%esp)
     * We also assume all clean call instrs are expanded.
     */
    /* Because the clean call might be optimized, we cannot assume the sequence.
     * We assume that the clean call will not be inlined for having more than one
     * arguments, so we scan to find either a call instr or a popf.
     * if a popf, do as before.
     * if a call, move back to right before push xbx or mov rbx => r3.
     */
    if (app_flags_ok == NULL)
        app_flags_ok = instrlist_first(ilist);
    /* r2065 added out-of-line clean call context switch, so we need to check
     * how the context switch code is inserted.
     */
    while (!instr_opcode_valid(app_flags_ok) ||
           instr_get_opcode(app_flags_ok) != OP_call) {
        app_flags_ok = instr_get_next(app_flags_ok);
        CLIENT_ASSERT(app_flags_ok != NULL,
                      "dr_insert_cbr_instrumentation: cannot find call instr");
        if (instr_get_opcode(app_flags_ok) == OP_popf)
            break;
    }
    if (instr_get_opcode(app_flags_ok) == OP_call) {
        if (opnd_get_pc(instr_get_target(app_flags_ok)) == (app_pc)callee) {
            /* call to clean callee
             * move a few instrs back till right before push xbx, or mov rbx => r3
             */
            while (app_flags_ok != NULL) {
                if (instr_reg_in_src(app_flags_ok, DR_REG_XBX))
                    break;
                app_flags_ok = instr_get_prev(app_flags_ok);
            }
        } else {
            /* call to clean call context save */
            ASSERT(opnd_get_pc(instr_get_target(app_flags_ok)) ==
                   get_clean_call_save(dcontext _IF_X64(GENCODE_X64)));
            out_of_line_switch = true;
        }
        ASSERT(app_flags_ok != NULL);
    }
    /* i#1155: for out-of-line context switch
     * we insert two parts of code to setup "taken" arg for clean call:
     * - compute "taken" and put it onto the stack right before call to context
     *   save, where DR already swapped stack and adjusted xsp to point beyond
     *   mcontext plus temp stack size.
     *   It is 2 slots away b/c 1st is retaddr.
     * - move the "taken" from stack to ebx to compatible with existing code
     *   right after context save returns and before arg setup, where xsp
     *   points beyond mcontext (xref emit_clean_call_save).
     *   It is 2 slots + temp stack size away.
     * XXX: we could optimize the code by computing "taken" after clean call
     * save if the eflags are not cleared.
     */
    /* put our code before the popf or use of xbx */
    opc = instr_get_opcode(instr);
    if (opc == OP_jecxz || opc == OP_loop || opc == OP_loope || opc == OP_loopne) {
        /* for 8-bit cbrs w/ multiple conditions and state, simpler to
         * simply execute them -- they're rare so shouldn't be a perf hit.
         * after all, ecx is saved, can clobber it.
         * we do:
         *               loop/jecxz taken
         *    not_taken: mov 0, ebx
         *               jmp done
         *    taken:     mov 1, ebx
         *    done:
         */
        opnd_t opnd_taken = out_of_line_switch
            ?
            /* 2 slots away from xsp, xref comment above for i#1155 */
            OPND_CREATE_MEM32(REG_XSP, -2 * (int)XSP_SZ /* ret+taken */)
            : opnd_create_reg(REG_EBX);
        instr_t *branch = instr_clone(dcontext, instr);
        instr_t *not_taken =
            INSTR_CREATE_mov_imm(dcontext, opnd_taken, OPND_CREATE_INT32(0));
        instr_t *taken = INSTR_CREATE_mov_imm(dcontext, opnd_taken, OPND_CREATE_INT32(1));
        instr_t *done = INSTR_CREATE_label(dcontext);
        instr_set_target(branch, opnd_create_instr(taken));
        /* client-added meta instrs should not have translation set */
        instr_set_translation(branch, NULL);
        MINSERT(ilist, app_flags_ok, branch);
        MINSERT(ilist, app_flags_ok, not_taken);
        MINSERT(ilist, app_flags_ok,
                INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(done)));
        MINSERT(ilist, app_flags_ok, taken);
        MINSERT(ilist, app_flags_ok, done);
        if (out_of_line_switch) {
            if (opc == OP_loop || opc == OP_loope || opc == OP_loopne) {
                /* We executed OP_loop* before we saved xcx, so we must restore
                 * it.  We should be able to use OP_lea b/c OP_loop* uses
                 * addr prefix to shrink pointer-sized xcx, not data prefix.
                 */
                reg_id_t xcx = opnd_get_reg(instr_get_dst(instr, 0));
                MINSERT(ilist, app_flags_ok,
                        INSTR_CREATE_lea(
                            dcontext, opnd_create_reg(xcx),
                            opnd_create_base_disp(xcx, DR_REG_NULL, 0, 1, OPSZ_lea)));
            }
            ASSERT(instr_get_opcode(app_flags_ok) == OP_call);
            /* 2 slots + temp_stack_size away from xsp,
             * xref comment above for i#1155
             */
            opnd_taken = OPND_CREATE_MEM32(
                REG_XSP, -2 * (int)XSP_SZ - get_clean_call_temp_stack_size());
            MINSERT(ilist, instr_get_next(app_flags_ok),
                    XINST_CREATE_load(dcontext, opnd_create_reg(REG_EBX), opnd_taken));
        }
    } else {
        /* build a setcc equivalent of instr's jcc operation
         * WARNING: this relies on order of OP_ enum!
         */
        opnd_t opnd_taken = out_of_line_switch
            ?
            /* 2 slots away from xsp, xref comment above for i#1155 */
            OPND_CREATE_MEM8(REG_XSP, -2 * (int)XSP_SZ /* ret+taken */)
            : opnd_create_reg(REG_BL);
        opc = instr_get_opcode(instr);
        if (opc <= OP_jnle_short)
            opc += (OP_jo - OP_jo_short);
        CLIENT_ASSERT(opc >= OP_jo && opc <= OP_jnle,
                      "dr_insert_cbr_instrumentation: unknown opcode");
        opc = opc - OP_jo + OP_seto;
        MINSERT(ilist, app_flags_ok, INSTR_CREATE_setcc(dcontext, opc, opnd_taken));
        if (out_of_line_switch) {
            app_flags_ok = instr_get_next(app_flags_ok);
            /* 2 slots + temp_stack_size away from xsp,
             * xref comment above for i#1155
             */
            opnd_taken = OPND_CREATE_MEM8(
                REG_XSP, -2 * (int)XSP_SZ - get_clean_call_temp_stack_size());
        }
        /* movzx ebx <- bl */
        MINSERT(ilist, app_flags_ok,
                INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX), opnd_taken));
    }
    /* now branch dir is in ebx and will be passed to clean call */
#elif defined(ARM)
    /* i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#elif defined(RISCV64)
    /* FIXME i#3544: Not implemented */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM/RISCV64 */
}

DR_API void
dr_insert_cbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee)
{
    dr_insert_cbr_instrumentation_help(drcontext, ilist, instr, callee,
                                       false /* no fallthrough */, opnd_create_null());
}

DR_API void
dr_insert_cbr_instrumentation_ex(void *drcontext, instrlist_t *ilist, instr_t *instr,
                                 void *callee, opnd_t user_data)
{
    dr_insert_cbr_instrumentation_help(drcontext, ilist, instr, callee,
                                       true /* has fallthrough */, user_data);
}

DR_API void
dr_insert_ubr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee)
{
    /* same as call */
    dr_insert_call_instrumentation(drcontext, ilist, instr, callee);
}

/* This may seem like a pretty targeted API function, but there's no
 * clean way for a client to do this on its own due to DR's
 * restrictions on bb instrumentation (i#782).
 */
DR_API
bool
dr_clobber_retaddr_after_read(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              ptr_uint_t value)
{
    /* the client could be using note fields so we use a label and xfer to
     * a note field during the mangling pass
     */
    if (instr_is_return(instr)) {
        instr_t *label = INSTR_CREATE_label(drcontext);
        dr_instr_label_data_t *data = instr_get_label_data_area(label);
        /* we could coordinate w/ drmgr and use some reserved note label value
         * but only if we run out of instr flags.  so we set to 0 to not
         * overlap w/ any client uses (DRMGR_NOTE_NONE == 0).
         */
        label->note = 0;
        /* these values are read back in d_r_mangle() */
        data->data[0] = (ptr_uint_t)instr;
        data->data[1] = value;
        label->flags |= INSTR_CLOBBER_RETADDR;
        instr->flags |= INSTR_CLOBBER_RETADDR;
        instrlist_meta_preinsert(ilist, instr, label);
        return true;
    }
    return false;
}

DR_API bool
dr_mcontext_xmm_fields_valid(void)
{
#ifdef X86
    return preserve_xmm_caller_saved();
#else
    return false;
#endif
}

DR_API bool
dr_mcontext_zmm_fields_valid(void)
{
#ifdef X86
    return d_r_is_avx512_code_in_use();
#else
    return false;
#endif
}

/* dr_get_mcontext() needed for translating clean call arg errors */

/* Fills in whichever of dmc or mc is non-NULL */
bool
dr_get_mcontext_priv(dcontext_t *dcontext, dr_mcontext_t *dmc, priv_mcontext_t *mc)
{
    priv_mcontext_t *state;
    CLIENT_ASSERT(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)),
                  "DR context protection NYI");
    if (mc == NULL) {
        CLIENT_ASSERT(dmc != NULL, "invalid context");
        CLIENT_ASSERT(dmc->flags != 0 && (dmc->flags & ~(DR_MC_ALL)) == 0,
                      "dr_mcontext_t.flags field not set properly");
    } else
        CLIENT_ASSERT(dmc == NULL, "invalid internal params");

    /* i#117/PR 395156: support getting mcontext from events where mcontext is
     * stable.  It would be nice to support it from init and 1st thread init,
     * but the mcontext is not available at those points.
     *
     * Since DR calls this routine when recreating state and wants the
     * clean call version, can't distinguish by whereami=DR_WHERE_FCACHE,
     * so we set a flag in the supported events. If client routine
     * crashes and we recreate then we want clean call version anyway
     * so should be ok.  Note that we want in_pre_syscall for other
     * reasons (dr_syscall_set_param() for Windows) so we keep it a
     * separate flag.
     */
    /* no support for init or initial thread init */
    if (!dynamo_initialized)
        return false;

    if (dcontext->client_data->cur_mc != NULL) {
        if (mc != NULL)
            *mc = *dcontext->client_data->cur_mc;
        else if (!priv_mcontext_to_dr_mcontext(dmc, dcontext->client_data->cur_mc))
            return false;
        return true;
    }

    if (!is_os_cxt_ptr_null(dcontext->client_data->os_cxt)) {
        return os_context_to_mcontext(dmc, mc, dcontext->client_data->os_cxt);
    }

    if (dcontext->client_data->suspended) {
        /* A thread suspended by dr_suspend_all_other_threads() has its
         * context translated lazily here.
         * We cache the result in cur_mc to avoid a translation cost next time.
         */
        DEBUG_DECLARE(bool res;)
        priv_mcontext_t *mc_xl8;
        if (mc != NULL)
            mc_xl8 = mc;
        else {
            dcontext->client_data->cur_mc = (priv_mcontext_t *)heap_alloc(
                dcontext, sizeof(*dcontext->client_data->cur_mc) HEAPACCT(ACCT_CLIENT));
            /* We'll clear this cache in dr_resume_all_other_threads() */
            mc_xl8 = dcontext->client_data->cur_mc;
        }
        DEBUG_DECLARE(res =) thread_get_mcontext(dcontext->thread_record, mc_xl8);
        CLIENT_ASSERT(res, "failed to get mcontext of suspended thread");
        DEBUG_DECLARE(res =)
        translate_mcontext(dcontext->thread_record, mc_xl8,
                           false /*do not restore memory*/, NULL);
        CLIENT_ASSERT(res, "failed to xl8 mcontext of suspended thread");
        if (mc == NULL && !priv_mcontext_to_dr_mcontext(dmc, mc_xl8))
            return false;
        return true;
    }

    /* PR 207947: support mcontext access from syscall events */
    if (dcontext->client_data->mcontext_in_dcontext ||
        dcontext->client_data->in_pre_syscall || dcontext->client_data->in_post_syscall) {
        if (mc != NULL)
            *mc = *get_mcontext(dcontext);
        else if (!priv_mcontext_to_dr_mcontext(dmc, get_mcontext(dcontext)))
            return false;
        return true;
    }

    /* dr_prepare_for_call() puts the machine context on the dstack
     * with pusha and pushf, but only fills in xmm values for
     * preserve_xmm_caller_saved(): however, we tell the client that the xmm
     * fields are not valid otherwise.  so, we just have to copy the
     * state from the dstack.
     */
    state = get_priv_mcontext_from_dstack(dcontext);
    if (mc != NULL)
        *mc = *state;
    else if (!priv_mcontext_to_dr_mcontext(dmc, state))
        return false;

    /* esp is a dstack value -- get the app stack's esp from the dcontext */
    if (mc != NULL)
        mc->xsp = get_mcontext(dcontext)->xsp;
    else if (TEST(DR_MC_CONTROL, dmc->flags))
        dmc->xsp = get_mcontext(dcontext)->xsp;

#ifdef AARCHXX
    if (mc != NULL || TEST(DR_MC_INTEGER, dmc->flags)) {
        /* get the stolen register's app value */
        if (mc != NULL) {
            set_stolen_reg_val(mc,
                               (reg_t)d_r_get_tls(os_tls_offset(TLS_REG_STOLEN_SLOT)));
        } else {
            set_stolen_reg_val(dr_mcontext_as_priv_mcontext(dmc),
                               (reg_t)d_r_get_tls(os_tls_offset(TLS_REG_STOLEN_SLOT)));
        }
    }
#endif

    /* XXX: should we set the pc field?
     * If we do we'll have to adopt a different solution for i#1685 in our Windows
     * hooks where today we use the pc slot for temp storage.
     */

    return true;
}

DR_API bool
dr_get_mcontext(void *drcontext, dr_mcontext_t *dmc)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return dr_get_mcontext_priv(dcontext, dmc, NULL);
}

DR_API bool
dr_set_mcontext(void *drcontext, dr_mcontext_t *context)
{
    priv_mcontext_t *state;
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    IF_AARCHXX(reg_t reg_val = 0 /* silence the compiler warning */;)
    CLIENT_ASSERT(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)),
                  "DR context protection NYI");
    CLIENT_ASSERT(context != NULL, "invalid context");
    CLIENT_ASSERT(context->flags != 0 && (context->flags & ~(DR_MC_ALL)) == 0,
                  "dr_mcontext_t.flags field not set properly");

    /* i#117/PR 395156: allow dr_[gs]et_mcontext where accurate */
    /* PR 207947: support mcontext access from syscall events */
    if (dcontext->client_data->mcontext_in_dcontext ||
        dcontext->client_data->in_pre_syscall || dcontext->client_data->in_post_syscall) {
        if (!dr_mcontext_to_priv_mcontext(get_mcontext(dcontext), context))
            return false;
        return true;
    }
    if (dcontext->client_data->cur_mc != NULL) {
        return dr_mcontext_to_priv_mcontext(dcontext->client_data->cur_mc, context);
    }
    if (!is_os_cxt_ptr_null(dcontext->client_data->os_cxt)) {
        /* It would be nice to fail for #DR_XFER_CALLBACK_RETURN but we'd need to
         * store yet more state to do so.  The pc will be ignored, and xsi
         * changes will likely cause crashes.
         */
        return mcontext_to_os_context(dcontext->client_data->os_cxt, context, NULL);
    }

    /* copy the machine context to the dstack area created with
     * dr_prepare_for_call().  note that xmm0-5 copied there
     * will override any save_fpstate xmm values, as desired.
     */
    state = get_priv_mcontext_from_dstack(dcontext);
#ifdef AARCHXX
    if (TEST(DR_MC_INTEGER, context->flags)) {
        /* Set the stolen register's app value in TLS, not on stack (we rely
         * on our stolen reg retaining its value on the stack)
         */
        priv_mcontext_t *mc = dr_mcontext_as_priv_mcontext(context);
        d_r_set_tls(os_tls_offset(TLS_REG_STOLEN_SLOT), (void *)get_stolen_reg_val(mc));
        /* save the reg val on the stack to be clobbered by the the copy below */
        reg_val = get_stolen_reg_val(state);
    }
#endif
    if (!dr_mcontext_to_priv_mcontext(state, context))
        return false;
#ifdef AARCHXX
    if (TEST(DR_MC_INTEGER, context->flags)) {
        /* restore the reg val on the stack clobbered by the copy above */
        set_stolen_reg_val(state, reg_val);
    }
#endif

    if (TEST(DR_MC_CONTROL, context->flags)) {
        /* esp will be restored from a field in the dcontext */
        get_mcontext(dcontext)->xsp = context->xsp;
    }

    /* XXX: should we support setting the pc field? */

    return true;
}

DR_API
bool
dr_redirect_execution(dr_mcontext_t *mcontext)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL);
    CLIENT_ASSERT(mcontext->size == sizeof(dr_mcontext_t),
                  "dr_mcontext_t.size field not set properly");
    CLIENT_ASSERT(mcontext->flags == DR_MC_ALL, "dr_mcontext_t.flags must be DR_MC_ALL");

    /* PR 352429: squash current trace.
     * FIXME: will clients use this so much that this will be a perf issue?
     * samples/cbr doesn't hit this even at -trace_threshold 1
     */
    if (is_building_trace(dcontext)) {
        LOG(THREAD, LOG_INTERP, 1, "squashing trace-in-progress\n");
        trace_abort(dcontext);
    }

    dcontext->next_tag = canonicalize_pc_target(dcontext, mcontext->pc);
    dcontext->whereami = DR_WHERE_FCACHE;
    set_last_exit(dcontext, (linkstub_t *)get_client_linkstub());
    if (kernel_xfer_callbacks.num > 0) {
        /* This can only be called from a clean call or an exception event.
         * For both of those we can get the current mcontext via dr_get_mcontext()
         * (the latter b/c we explicitly store to cur_mc just for this use case).
         */
        dr_mcontext_t src_dmc;
        src_dmc.size = sizeof(src_dmc);
        src_dmc.flags = DR_MC_CONTROL | DR_MC_INTEGER;
        dr_get_mcontext(dcontext, &src_dmc);
        if (instrument_kernel_xfer(dcontext, DR_XFER_CLIENT_REDIRECT, osc_empty, &src_dmc,
                                   NULL, dcontext->next_tag, mcontext->xsp, osc_empty,
                                   dr_mcontext_as_priv_mcontext(mcontext), 0))
            dcontext->next_tag = canonicalize_pc_target(dcontext, mcontext->pc);
    }
    transfer_to_dispatch(dcontext, dr_mcontext_as_priv_mcontext(mcontext),
                         true /*full_DR_state*/);
    /* on success we won't get here */
    return false;
}

DR_API
byte *
dr_redirect_native_target(void *drcontext)
{
#ifdef PROGRAM_SHEPHERDING
    /* This feature is unavail for prog shep b/c of the cross-ib-type pollution,
     * as well as the lack of source tag info when exiting the ibl (i#1150).
     */
    return NULL;
#else
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_redirect_native_target(): drcontext cannot be NULL");
    /* The client has no way to know the mode of our gencode so we set LSB here */
    return PC_AS_JMP_TGT(DEFAULT_ISA_MODE, get_client_ibl_xfer_entry(dcontext));
#endif
}

/***************************************************************************
 * ADAPTIVE OPTIMIZATION SUPPORT
 * *Note for non owning thread support (i.e. sideline) all methods assume
 * the dcontext valid, the client will have to insure this with a lock
 * on thread_exit!!
 *
 * *need way for side thread to get a dcontext to use for logging and mem
 * alloc, before do that should think more about mem alloc in/for adaptive
 * routines
 *
 * *made local mem alloc by side thread safe (see heap.c)
 *
 * *loging not safe if not owning thread?
 */

DR_API
/* Schedules the fragment to be deleted.  Once this call is completed,
 * an existing executing fragment is allowed to complete, but control
 * will not enter the fragment again before it is deleted.
 *
 * NOTE: this comment used to say, "after deletion, control may still
 * reach the fragment by indirect branch.".  We believe this is now only
 * true for shared fragments, which are not currently supported.
 */
bool
dr_delete_fragment(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    bool deletable = false, waslinking;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    CLIENT_ASSERT(!SHARED_FRAGMENTS_ENABLED(),
                  "dr_delete_fragment() only valid with -thread_private");
    CLIENT_ASSERT(drcontext != NULL, "dr_delete_fragment(): drcontext cannot be NULL");
    /* i#1989: there's no easy way to get a translation without a proper dcontext */
    CLIENT_ASSERT(!fragment_thread_exited(dcontext),
                  "dr_delete_fragment not supported from the thread exit event");
    if (fragment_thread_exited(dcontext))
        return false;
    waslinking = is_couldbelinking(dcontext);
    if (!waslinking)
        enter_couldbelinking(dcontext, NULL, false);
    d_r_mutex_lock(&(dcontext->client_data->sideline_mutex));
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    if (f != NULL && (f->flags & FRAG_CANNOT_DELETE) == 0) {
        /* We avoid "local unprotected" as it is global, complicating thread exit. */
        client_todo_list_t *todo =
            HEAP_TYPE_ALLOC(dcontext, client_todo_list_t, ACCT_CLIENT, PROTECTED);
        client_todo_list_t *iter = dcontext->client_data->to_do;
        todo->next = NULL;
        todo->ilist = NULL;
        todo->tag = tag;
        if (iter == NULL)
            dcontext->client_data->to_do = todo;
        else {
            while (iter->next != NULL)
                iter = iter->next;
            iter->next = todo;
        }
        deletable = true;
        /* unlink fragment so will return to dynamo and delete.
         * Do not remove the fragment from the hashtable --
         * we need to be able to look up the fragment when
         * inspecting the to_do list in d_r_dispatch.
         */
        if ((f->flags & FRAG_LINKED_INCOMING) != 0)
            unlink_fragment_incoming(dcontext, f);
        fragment_remove_from_ibt_tables(dcontext, f, false);
    }
    fragment_release_fragment_delete_mutex(dcontext);
    d_r_mutex_unlock(&(dcontext->client_data->sideline_mutex));
    if (!waslinking)
        enter_nolinking(dcontext, NULL, false);
    return deletable;
}

DR_API
/* Schedules the fragment at 'tag' for replacement.  Once this call is
 * completed, an existing executing fragment is allowed to complete,
 * but control will not enter the fragment again before it is replaced.
 *
 * NOTE: this comment used to say, "after replacement, control may still
 * reach the fragment by indirect branch.".  We believe this is now only
 * true for shared fragments, which are not currently supported.
 *
 * Takes control of the ilist and all responsibility for deleting it and the
 * instrs inside of it.  The client should not keep, use, reference, etc. the
 * instrlist or any of the instrs it contains after they are passed in.
 */
bool
dr_replace_fragment(void *drcontext, void *tag, instrlist_t *ilist)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    bool frag_found, waslinking;
    fragment_t *f;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    CLIENT_ASSERT(!SHARED_FRAGMENTS_ENABLED(),
                  "dr_replace_fragment() only valid with -thread_private");
    CLIENT_ASSERT(drcontext != NULL, "dr_replace_fragment(): drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_replace_fragment: drcontext is invalid");
    /* i#1989: there's no easy way to get a translation without a proper dcontext */
    CLIENT_ASSERT(!fragment_thread_exited(dcontext),
                  "dr_replace_fragment not supported from the thread exit event");
    if (fragment_thread_exited(dcontext))
        return false;
    waslinking = is_couldbelinking(dcontext);
    if (!waslinking)
        enter_couldbelinking(dcontext, NULL, false);
    d_r_mutex_lock(&(dcontext->client_data->sideline_mutex));
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    frag_found = (f != NULL);
    if (frag_found) {
        client_todo_list_t *iter = dcontext->client_data->to_do;
        /* We avoid "local unprotected" as it is global, complicating thread exit. */
        client_todo_list_t *todo =
            HEAP_TYPE_ALLOC(dcontext, client_todo_list_t, ACCT_CLIENT, PROTECTED);
        todo->next = NULL;
        todo->ilist = ilist;
        todo->tag = tag;
        if (iter == NULL)
            dcontext->client_data->to_do = todo;
        else {
            while (iter->next != NULL)
                iter = iter->next;
            iter->next = todo;
        }
        /* unlink fragment so will return to dynamo and replace for next time
         * its executed
         */
        if ((f->flags & FRAG_LINKED_INCOMING) != 0)
            unlink_fragment_incoming(dcontext, f);
        fragment_remove_from_ibt_tables(dcontext, f, false);
    }
    fragment_release_fragment_delete_mutex(dcontext);
    d_r_mutex_unlock(&(dcontext->client_data->sideline_mutex));
    if (!waslinking)
        enter_nolinking(dcontext, NULL, false);
    return frag_found;
}

#ifdef UNSUPPORTED_API
/* FIXME - doesn't work with shared fragments.  Consider removing since dr_flush_region
 * and dr_delay_flush_region give us most of this functionality. */
DR_API
/* Flushes all fragments containing 'flush_tag', or the entire code
 * cache if flush_tag is NULL.  'curr_tag' must specify the tag of the
 * currently-executing fragment.  If curr_tag is NULL, flushing can be
 * delayed indefinitely.  Note that flushing is performed across all
 * threads, but other threads may continue to execute fragments
 * containing 'curr_tag' until those fragments finish.
 */
void
dr_flush_fragments(void *drcontext, void *curr_tag, void *flush_tag)
{
    client_flush_req_t *iter, *flush;
    dcontext_t *dcontext = (dcontext_t *)drcontext;

    /* We want to unlink the currently executing fragment so we'll
     * force a context switch to DR.  That way, we'll perform the
     * flush as soon as possible.  Unfortunately, the client may not
     * know the tag of the current trace.  Therefore, we unlink all
     * fragments in the region.
     *
     * Note that we aren't unlinking or ibl-invalidating (i.e., making
     * unreachable) any fragments in other threads containing curr_tag
     * until the delayed flush happens in enter_nolinking().
     */
    if (curr_tag != NULL)
        vm_area_unlink_incoming(dcontext, (app_pc)curr_tag);

    /* We avoid "local unprotected" as it is global, complicating thread exit. */
    flush = HEAP_TYPE_ALLOC(dcontext, client_flush_req_t, ACCT_CLIENT, PROTECTED);
    flush->flush_callback = NULL;
    if (flush_tag == NULL) {
        flush->start = UNIVERSAL_REGION_BASE;
        flush->size = UNIVERSAL_REGION_SIZE;
    } else {
        flush->start = (app_pc)flush_tag;
        flush->size = 1;
    }
    flush->next = NULL;

    iter = dcontext->client_data->flush_list;
    if (iter == NULL) {
        dcontext->client_data->flush_list = flush;
    } else {
        while (iter->next != NULL)
            iter = iter->next;
        iter->next = flush;
    }
}
#endif /* UNSUPPORTED_API */

DR_API
/* Flush all fragments that contain code from the region [start, start+size).
 * Uses a synchall flush to guarantee that no execution occurs out of the fragments
 * flushed once this returns. Requires caller to be holding no locks (dr or client) and
 * to be !couldbelinking (xref PR 199115, 227619). Invokes the given callback after the
 * flush completes and before threads are resumed. Caller must use
 * dr_redirect_execution() to return to the cache. */
bool
dr_flush_region_ex(app_pc start, size_t size,
                   void (*flush_completion_callback)(void *user_data), void *user_data)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL);

    LOG(THREAD, LOG_FRAGMENT, 2, "%s: " PFX "-" PFX "\n", __FUNCTION__, start,
        start + size);

    /* Flush requires !couldbelinking. FIXME - not all event callbacks to the client are
     * !couldbelinking (see PR 227619) restricting where this routine can be used. */
    CLIENT_ASSERT(!is_couldbelinking(dcontext),
                  "dr_flush_region: called from an event "
                  "callback that doesn't support calling this routine; see header file "
                  "for restrictions.");
    /* Flush requires caller to hold no locks that might block a couldbelinking thread
     * (which includes almost all dr locks).  FIXME - some event callbacks are holding
     * dr locks (see PR 227619) so can't call this routine.  Since we are going to use
     * a synchall flush, holding client locks is disallowed too (could block a thread
     * at an unsafe spot for synch). */
    CLIENT_ASSERT(OWN_NO_LOCKS(dcontext),
                  "dr_flush_region: caller owns a client "
                  "lock or was called from an event callback that doesn't support "
                  "calling this routine; see header file for restrictions.");
    CLIENT_ASSERT(size != 0, "dr_flush_region_ex: 0 is invalid size for flush");

    /* release build check of requirements, as many as possible at least */
    if (size == 0 || is_couldbelinking(dcontext)) {
        (*flush_completion_callback)(user_data);
        return false;
    }

    if (!executable_vm_area_executed_from(start, start + size)) {
        (*flush_completion_callback)(user_data);
        return true;
    }

    flush_fragments_from_region(dcontext, start, size, true /*force synchall*/,
                                flush_completion_callback, user_data);

    return true;
}

DR_API
/* Equivalent to dr_flush_region_ex, without the callback. */
bool
dr_flush_region(app_pc start, size_t size)
{
    return dr_flush_region_ex(start, size, NULL /*flush_completion_callback*/,
                              NULL /*user_data*/);
}

DR_API
/* Flush all fragments that contain code from the region [start, start+size).
 * Uses an unlink flush which guarantees that no thread will enter a fragment that was
 * flushed once this returns (threads already in a flushed fragment will continue).
 * Requires caller to be holding no locks (dr or client) and to be !couldbelinking
 * (xref PR 199115, 227619). */
bool
dr_unlink_flush_region(app_pc start, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL);

    LOG(THREAD, LOG_FRAGMENT, 2, "%s: " PFX "-" PFX "\n", __FUNCTION__, start,
        start + size);

    /* This routine won't work with coarse_units */
    CLIENT_ASSERT(!DYNAMO_OPTION(coarse_units),
                  /* as of now, coarse_units are always disabled with -thread_private. */
                  "dr_unlink_flush_region is not supported with -opt_memory unless "
                  "-thread_private or -enable_full_api is also specified");

    /* Flush requires !couldbelinking. FIXME - not all event callbacks to the client are
     * !couldbelinking (see PR 227619) restricting where this routine can be used. */
    CLIENT_ASSERT(!is_couldbelinking(dcontext),
                  "dr_flush_region: called from an event "
                  "callback that doesn't support calling this routine, see header file "
                  "for restrictions.");
    /* Flush requires caller to hold no locks that might block a couldbelinking thread
     * (which includes almost all dr locks).  FIXME - some event callbacks are holding
     * dr locks (see PR 227619) so can't call this routine.  FIXME - some event callbacks
     * are couldbelinking (see PR 227619) so can't allow the caller to hold any client
     * locks that could block threads in one of those events (otherwise we don't need
     * to care about client locks) */
    CLIENT_ASSERT(OWN_NO_LOCKS(dcontext),
                  "dr_flush_region: caller owns a client "
                  "lock or was called from an event callback that doesn't support "
                  "calling this routine, see header file for restrictions.");
    CLIENT_ASSERT(size != 0, "dr_unlink_flush_region: 0 is invalid size for flush");

    /* release build check of requirements, as many as possible at least */
    if (size == 0 || is_couldbelinking(dcontext))
        return false;

    if (!executable_vm_area_executed_from(start, start + size))
        return true;

    flush_fragments_from_region(dcontext, start, size, false /*don't force synchall*/,
                                NULL /*flush_completion_callback*/, NULL /*user_data*/);

    return true;
}

DR_API
/* Flush all fragments that contain code from the region [start, start+size) at the next
 * convenient time.  Unlike dr_flush_region() this routine has no restrictions on lock
 * or couldbelinking status; the downside is that the delay till the flush actually
 * occurs is unbounded (FIXME - we could do something safely here to try to speed it
 * up like unlinking shared_syscall etc.), but should occur before any new code is
 * executed or any nudges are processed. */
bool
dr_delay_flush_region(app_pc start, size_t size, uint flush_id,
                      void (*flush_completion_callback)(int flush_id))
{
    client_flush_req_t *flush;

    LOG(THREAD_GET, LOG_FRAGMENT, 2, "%s: " PFX "-" PFX "\n", __FUNCTION__, start,
        start + size);

    if (size == 0) {
        CLIENT_ASSERT(false, "dr_delay_flush_region: 0 is invalid size for flush");
        return false;
    }

    /* With the new module load event at 1st execution (i#884), we get a lot of
     * flush requests during creation of a bb from things like drwrap_replace().
     * To avoid them flushing from a new module we check overlap up front here.
     */
    if (!executable_vm_area_executed_from(start, start + size)) {
        return true;
    }

    /* FIXME - would be nice if we could check the requirements and call
     * dr_unlink_flush_region() here if it's safe. Is difficult to detect non-dr locks
     * that could block a couldbelinking thread though. */

    flush =
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, client_flush_req_t, ACCT_CLIENT, UNPROTECTED);
    memset(flush, 0x0, sizeof(client_flush_req_t));
    flush->start = (app_pc)start;
    flush->size = size;
    flush->flush_id = flush_id;
    flush->flush_callback = flush_completion_callback;

    d_r_mutex_lock(&client_flush_request_lock);
    flush->next = client_flush_requests;
    client_flush_requests = flush;
    d_r_mutex_unlock(&client_flush_request_lock);

    return true;
}

DR_API
/* returns whether or not there is a fragment in the drcontext fcache at tag
 */
bool
dr_fragment_exists_at(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    fragment_release_fragment_delete_mutex(dcontext);
    return f != NULL;
}

DR_API
bool
dr_bb_exists_at(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f = fragment_lookup(dcontext, tag);
    if (f != NULL && !TEST(FRAG_IS_TRACE, f->flags)) {
        return true;
    }

    return false;
}

DR_API
/* Looks up the fragment associated with the application pc tag.
 * If not found, returns 0.
 * If found, returns the total size occupied in the cache by the fragment.
 */
uint
dr_fragment_size(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    int size = 0;
    CLIENT_ASSERT(drcontext != NULL, "dr_fragment_size: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT, "dr_fragment_size: drcontext is invalid");
    /* used to check to see if owning thread, if so don't need lock */
    /* but the check for owning thread more expensive then just getting lock */
    /* to check if owner d_r_get_thread_id() == dcontext->owning_thread */
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    if (f == NULL)
        size = 0;
    else
        size = f->size;
    fragment_release_fragment_delete_mutex(dcontext);
    return size;
}

DR_API
/* Retrieves the application PC of a fragment */
app_pc
dr_fragment_app_pc(void *tag)
{
#ifdef WINDOWS
    tag = get_app_pc_from_intercept_pc_if_necessary((app_pc)tag);
    CLIENT_ASSERT(tag != NULL, "dr_fragment_app_pc shouldn't be NULL");

    DODEBUG({
        /* Without -hide our DllMain routine ends up in the cache (xref PR 223120).
         * On Linux fini() ends up in the cache.
         */
        if (DYNAMO_OPTION(hide) && is_dynamo_address(tag) &&
            /* support client interpreting code out of its library */
            !is_in_client_lib(tag)) {
            /* downgraded from assert for client interpreting its own generated code */
            SYSLOG_INTERNAL_WARNING_ONCE("dr_fragment_app_pc is a DR/client pc");
        }
    });
#elif defined(LINUX) && defined(X86_32)
    /* Point back at our hook, undoing the bb shift for SA_RESTART (i#2659). */
    if ((app_pc)tag == vsyscall_sysenter_displaced_pc)
        tag = vsyscall_sysenter_return_pc;
#endif
    return tag;
}

DR_API
/* i#268: opposite of dr_fragment_app_pc() */
app_pc
dr_app_pc_for_decoding(app_pc pc)
{
#ifdef WINDOWS
    app_pc displaced;
    if (is_intercepted_app_pc(pc, &displaced))
        return displaced;
#endif
    return pc;
}

DR_API
app_pc
dr_app_pc_from_cache_pc(byte *cache_pc)
{
    app_pc res = NULL;
    dcontext_t *dcontext = get_thread_private_dcontext();
    bool waslinking;
    CLIENT_ASSERT(!standalone_library, "API not supported in standalone mode");
    ASSERT(dcontext != NULL);
    /* i#1989: there's no easy way to get a translation without a proper dcontext */
    CLIENT_ASSERT(!fragment_thread_exited(dcontext),
                  "dr_app_pc_from_cache_pc not supported from the thread exit event");
    if (fragment_thread_exited(dcontext))
        return NULL;
    waslinking = is_couldbelinking(dcontext);
    if (!waslinking)
        enter_couldbelinking(dcontext, NULL, false);
    /* suppress asserts about faults in meta instrs */
    DODEBUG({ dcontext->client_data->is_translating = true; });
    res = recreate_app_pc(dcontext, cache_pc, NULL);
    DODEBUG({ dcontext->client_data->is_translating = false; });
    if (!waslinking)
        enter_nolinking(dcontext, NULL, false);
    return res;
}

DR_API
bool
dr_using_app_state(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    return os_using_app_state(dcontext);
}

DR_API
void
dr_switch_to_app_state(void *drcontext)
{
    dr_switch_to_app_state_ex(drcontext, DR_STATE_ALL);
}

DR_API
void
dr_switch_to_app_state_ex(void *drcontext, dr_state_flags_t flags)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    os_swap_context(dcontext, true /*to app*/, flags);
}

DR_API
void
dr_switch_to_dr_state(void *drcontext)
{
    dr_switch_to_dr_state_ex(drcontext, DR_STATE_ALL);
}

DR_API
void
dr_switch_to_dr_state_ex(void *drcontext, dr_state_flags_t flags)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    os_swap_context(dcontext, false /*to dr*/, flags);
}

/***************************************************************************
 * CUSTOM TRACES SUPPORT
 * *could use a method to unmark a trace head, would be nice if DR
 * notified the client when it marked a trace head and gave the client a
 * chance to override its decision
 */

DR_API
/* Marks the fragment associated with the application pc tag as
 * a trace head.  The fragment need not exist yet -- once it is
 * created it will be marked as a trace head.
 *
 * DR associates a counter with a trace head and once it
 * passes the -hot_threshold parameter, DR begins building
 * a trace.  Before each fragment is added to the trace, DR
 * calls the client routine dr_end_trace to determine whether
 * to end the trace.  (dr_end_trace will be called both for
 * standard DR traces and for client-defined traces.)
 *
 * Note, some fragments are unsuitable for trace heads. DR will
 * ignore attempts to mark such fragments as trace heads and will return
 * false. If the client marks a fragment that doesn't exist yet as a trace
 * head and DR later determines that the fragment is unsuitable for
 * a trace head it will unmark the fragment as a trace head without
 * notifying the client.
 *
 * Returns true if the target fragment is marked as a trace head.
 *
 * If coarse, headness depends on path: currently this will only have
 * links from tag's coarse unit unlinked.
 */
bool /* FIXME: dynamorio_app_init returns an int! */
dr_mark_trace_head(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    fragment_t coarse_f;
    bool success = true;
    CLIENT_ASSERT(drcontext != NULL, "dr_mark_trace_head: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_mark_trace_head: drcontext is invalid");
    /* Required to make the future-fragment lookup and add atomic and for
     * mark_trace_head.  We have to grab before fragment_delete_mutex so
     * we pay the cost of acquiring up front even when f->flags doesn't
     * require it.
     */
    SHARED_FLAGS_RECURSIVE_LOCK(FRAG_SHARED, acquire, change_linking_lock);
    /* used to check to see if owning thread, if so don't need lock */
    /* but the check for owning thread more expensive then just getting lock */
    /* to check if owner d_r_get_thread_id() == dcontext->owning_thread */
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup_fine_and_coarse(dcontext, tag, &coarse_f, NULL);
    if (f == NULL) {
        future_fragment_t *fut;
        fut = fragment_lookup_future(dcontext, tag);
        if (fut == NULL) {
            /* need to create a future fragment */
            fut = fragment_create_and_add_future(dcontext, tag, FRAG_IS_TRACE_HEAD);
        } else {
            /* don't call mark_trace_head, it will try to do some linking */
            fut->flags |= FRAG_IS_TRACE_HEAD;
        }
    } else {
        /* check precluding conditions */
        if (TEST(FRAG_IS_TRACE, f->flags)) {
            success = false;
        } else if (TEST(FRAG_CANNOT_BE_TRACE, f->flags)) {
            success = false;
        } else if (TEST(FRAG_IS_TRACE_HEAD, f->flags)) {
            success = true;
        } else {
            mark_trace_head(dcontext, f, NULL, NULL);
        }
    }
    fragment_release_fragment_delete_mutex(dcontext);
    SHARED_FLAGS_RECURSIVE_LOCK(FRAG_SHARED, release, change_linking_lock);
    return success;
}

DR_API
/* Checks to see if the fragment (or future fragment) in the drcontext
 * fcache at tag is marked as a trace head
 */
bool
dr_trace_head_at(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    bool trace_head;
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    if (f != NULL)
        trace_head = (f->flags & FRAG_IS_TRACE_HEAD) != 0;
    else {
        future_fragment_t *fut = fragment_lookup_future(dcontext, tag);
        if (fut != NULL)
            trace_head = (fut->flags & FRAG_IS_TRACE_HEAD) != 0;
        else
            trace_head = false;
    }
    fragment_release_fragment_delete_mutex(dcontext);
    return trace_head;
}

DR_API
/* checks to see that if there is a trace in the drcontext fcache at tag
 */
bool
dr_trace_exists_at(void *drcontext, void *tag)
{
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    fragment_t *f;
    bool trace;
    fragment_get_fragment_delete_mutex(dcontext);
    f = fragment_lookup(dcontext, tag);
    if (f != NULL)
        trace = (f->flags & FRAG_IS_TRACE) != 0;
    else
        trace = false;
    fragment_release_fragment_delete_mutex(dcontext);
    return trace;
}

DR_API
/* Insert code to get the segment base address pointed at by seg into
 * register reg. In Linux, it is only supported with -mangle_app_seg option.
 * In Windows, it only supports getting base address of the TLS segment.
 */
bool
dr_insert_get_seg_base(void *drcontext, instrlist_t *ilist, instr_t *instr, reg_id_t seg,
                       reg_id_t reg)
{
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_insert_get_seg_base: reg has wrong size\n");
#ifdef X86
    CLIENT_ASSERT(reg_is_segment(seg),
                  "dr_insert_get_seg_base: seg is not a segment register");
#    ifdef UNIX
#        ifndef MACOS64
    CLIENT_ASSERT(INTERNAL_OPTION(mangle_app_seg),
                  "dr_insert_get_seg_base is supported with -mangle_app_seg only");
    /* FIXME: we should remove the constraint below by always mangling SEG_TLS,
     * 1. Getting TLS base could be a common request by clients.
     * 2. The TLS descriptor setup and selector setup can be separated,
     * so we must intercept all descriptor setup. It will not be large
     * runtime overhead for keeping track of the app's TLS segment base.
     */
    CLIENT_ASSERT(INTERNAL_OPTION(private_loader) || seg != SEG_TLS,
                  "dr_insert_get_seg_base supports TLS seg only with -private_loader");
    if (!INTERNAL_OPTION(mangle_app_seg) ||
        !(INTERNAL_OPTION(private_loader) || seg != SEG_TLS))
        return false;
#        endif
    if (seg == SEG_FS || seg == SEG_GS) {
        instrlist_meta_preinsert(ilist, instr,
                                 instr_create_restore_from_tls(
                                     drcontext, reg, os_get_app_tls_base_offset(seg)));
    } else {
        instrlist_meta_preinsert(
            ilist, instr,
            INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(reg), OPND_CREATE_INTPTR(0)));
    }
#    else  /* Windows */
    if (seg == SEG_TLS) {
        instrlist_meta_preinsert(
            ilist, instr,
            XINST_CREATE_load(drcontext, opnd_create_reg(reg),
                              opnd_create_far_base_disp(SEG_TLS, REG_NULL, REG_NULL, 0,
                                                        SELF_TIB_OFFSET, OPSZ_PTR)));
    } else if (seg == SEG_CS || seg == SEG_DS || seg == SEG_ES || seg == SEG_SS) {
        /* XXX: we assume flat address space */
        instrlist_meta_preinsert(
            ilist, instr,
            INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(reg), OPND_CREATE_INTPTR(0)));
    } else
        return false;
#    endif /* UNIX/Windows */
#elif defined(ARM)
    /* i#1551: NYI on ARM */
    ASSERT_NOT_IMPLEMENTED(false);
#endif /* X86/ARM */
    return true;
}

DR_API
reg_id_t
dr_get_stolen_reg()
{
    return IF_AARCHXX_ELSE(dr_reg_stolen, REG_NULL);
}

DR_API
bool
dr_insert_get_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg)
{
    IF_X86(CLIENT_ASSERT(false, "dr_insert_get_stolen_reg: should not be reached\n"));
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_insert_get_stolen_reg: reg has wrong size\n");
    CLIENT_ASSERT(!reg_is_stolen(reg),
                  "dr_insert_get_stolen_reg: reg is used by DynamoRIO\n");
#ifdef AARCHXX
    instrlist_meta_preinsert(
        ilist, instr, instr_create_restore_from_tls(drcontext, reg, TLS_REG_STOLEN_SLOT));
#endif
    return true;
}

DR_API
bool
dr_insert_set_stolen_reg_value(void *drcontext, instrlist_t *ilist, instr_t *instr,
                               reg_id_t reg)
{
    IF_X86(CLIENT_ASSERT(false, "dr_insert_set_stolen_reg: should not be reached\n"));
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_insert_set_stolen_reg: reg has wrong size\n");
    CLIENT_ASSERT(!reg_is_stolen(reg),
                  "dr_insert_set_stolen_reg: reg is used by DynamoRIO\n");
#ifdef AARCHXX
    instrlist_meta_preinsert(
        ilist, instr, instr_create_save_to_tls(drcontext, reg, TLS_REG_STOLEN_SLOT));
#endif
    return true;
}

DR_API
int
dr_remove_it_instrs(void *drcontext, instrlist_t *ilist)
{
#if !defined(ARM)
    return 0;
#else
    int res = 0;
    instr_t *inst, *next;
    for (inst = instrlist_first(ilist); inst != NULL; inst = next) {
        next = instr_get_next(inst);
        if (instr_get_opcode(inst) == OP_it) {
            res++;
            instrlist_remove(ilist, inst);
            instr_destroy(drcontext, inst);
        }
    }
    return res;
#endif
}

DR_API
int
dr_insert_it_instrs(void *drcontext, instrlist_t *ilist)
{
#if !defined(ARM)
    return 0;
#else
    instr_t *first = instrlist_first(ilist);
    if (first == NULL || instr_get_isa_mode(first) != DR_ISA_ARM_THUMB)
        return 0;
    return reinstate_it_blocks((dcontext_t *)drcontext, ilist, instrlist_first(ilist),
                               NULL);
#endif
}

DR_API
bool
dr_prepopulate_cache(app_pc *tags, size_t tags_count)
{
    /* We expect get_thread_private_dcontext() to return NULL b/c we're between
     * dr_app_setup() and dr_app_start() and are considered a "native" thread
     * with disabled TLS.  We do set up TLS as too many routines fail (e.g.,
     * clean call analysis) with NULL from TLS, but we do not set up signal
     * handling: the caller has to handle decode faults, as we do not
     * want to enable our signal handlers, which might disrupt the app running
     * natively in parallel with us.
     */
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    dcontext_t *dcontext = tr->dcontext;
    uint i;
    if (dcontext == NULL)
        return false;
    SHARED_BB_LOCK();
    SYSLOG_INTERNAL_INFO("pre-building code cache from %d tags", tags_count);
#ifdef UNIX
    os_swap_context(dcontext, false /*to dr*/, DR_STATE_GO_NATIVE);
#endif
    for (i = 0; i < tags_count; i++) {
        /* There could be duplicates if sthg was deleted and re-added during profiling */
        fragment_t coarse_f;
        fragment_t *f;
#ifdef UNIX
        /* We silently skip DR-segment-reading addresses to help out a caller
         * who sampled and couldn't avoid self-sampling for decoding.
         */
        if (is_DR_segment_reader_entry(tags[i]))
            continue;
#endif
        f = fragment_lookup_fine_and_coarse(dcontext, tags[i], &coarse_f, NULL);
        if (f == NULL) {
            /* For coarse-grain we won't link as that's done during execution,
             * but for fine-grained this should produce a fully warmed cache.
             */
            f = build_basic_block_fragment(dcontext, tags[i], 0, true /*link*/,
                                           true /*visible*/, false /*!for_trace*/, NULL);
        }
        ASSERT(f != NULL);
        /* We're ok making a thread-private fragment: might be a waste if this
         * thread never runs it, but simpler than trying to skip them or sthg.
         */
    }
#ifdef UNIX
    os_swap_context(dcontext, true /*to app*/, DR_STATE_GO_NATIVE);
#endif
    SHARED_BB_UNLOCK();
    return true;
}

DR_API
bool
dr_prepopulate_indirect_targets(dr_indirect_branch_type_t branch_type, app_pc *tags,
                                size_t tags_count)
{
    /* We do the same setup as for dr_prepopulate_cache(). */
    thread_record_t *tr = thread_lookup(d_r_get_thread_id());
    dcontext_t *dcontext = tr->dcontext;
    ibl_branch_type_t ibl_type;
    uint i;
    if (dcontext == NULL)
        return false;
    /* Initially I took in an opcode and used extract_branchtype(instr_branch_type())
     * but every use case had to make a fake instr to get the opcode and had no
     * good cross-platform method so I switched to an enum.  We're unlikely to
     * change our ibt split and we can add new enums in any case.
     */
    switch (branch_type) {
    case DR_INDIRECT_RETURN: ibl_type = IBL_RETURN; break;
    case DR_INDIRECT_CALL: ibl_type = IBL_INDCALL; break;
    case DR_INDIRECT_JUMP: ibl_type = IBL_INDJMP; break;
    default: return false;
    }
    SYSLOG_INTERNAL_INFO("pre-populating ibt[%d] table for %d tags", ibl_type,
                         tags_count);
#ifdef UNIX
    os_swap_context(dcontext, false /*to dr*/, DR_STATE_GO_NATIVE);
#endif
    for (i = 0; i < tags_count; i++) {
        fragment_add_ibl_target(dcontext, tags[i], ibl_type);
    }
#ifdef UNIX
    os_swap_context(dcontext, true /*to app*/, DR_STATE_GO_NATIVE);
#endif
    return true;
}

DR_API
bool
dr_get_stats(dr_stats_t *drstats)
{
    return stats_get_snapshot(drstats);
}

/***************************************************************************
 * PERSISTENCE
 */

/* Up to caller to synchronize. */
uint
instrument_persist_ro_size(dcontext_t *dcontext, void *perscxt, size_t file_offs)
{
    size_t sz = 0;
    size_t i;

    /* Store the set of clients in use as we require the same set in order
     * to validate the pcache on use.  Note that we can't just have -client_lib
     * be OP_PCACHE_GLOBAL b/c it contains client options too.
     * We have no unique guids for clients so we store the full path.
     * We ignore ids.  We do care about priority order: clients must
     * be in the same order in addition to having the same path.
     *
     * XXX: we could go further and store client library checksum, etc. hashes,
     * but that precludes clients from doing their own proper versioning.
     *
     * XXX: we could also put the set of clients into the pcache namespace to allow
     * simultaneous use of pcaches with different sets of clients (empty set
     * vs under tool, in particular): but doesn't really seem useful enough
     * for the trouble
     */
    for (i = 0; i < num_client_libs; i++) {
        sz += strlen(client_libs[i].path) + 1 /*NULL*/;
    }
    sz++; /* double NULL ends it */

    /* Now for clients' own data.
     * For user_data, we assume each sequence of <size, patch, persist> is
     * atomic: caller holds a mutex across the sequence.  Thus, we can use
     * global storage.
     */
    if (persist_ro_size_callbacks.num > 0) {
        call_all_ret(sz, +=, , persist_ro_size_callbacks,
                     size_t(*)(void *, void *, size_t, void **), (void *)dcontext,
                     perscxt, file_offs + sz, &persist_user_data[idx]);
    }
    /* using size_t for API w/ clients in case we want to widen in future */
    CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(sz), "persisted cache size too large");
    return (uint)sz;
}

/* Up to caller to synchronize.
 * Returns true iff all writes succeeded.
 */
bool
instrument_persist_ro(dcontext_t *dcontext, void *perscxt, file_t fd)
{
    bool res = true;
    size_t i;
    char nul = '\0';
    ASSERT(fd != INVALID_FILE);

    for (i = 0; i < num_client_libs; i++) {
        size_t sz = strlen(client_libs[i].path) + 1 /*NULL*/;
        if (os_write(fd, client_libs[i].path, sz) != (ssize_t)sz)
            return false;
    }
    /* double NULL ends it */
    if (os_write(fd, &nul, sizeof(nul)) != (ssize_t)sizeof(nul))
        return false;

    /* Now for clients' own data */
    if (persist_ro_size_callbacks.num > 0) {
        call_all_ret(res, = res &&, , persist_ro_callbacks,
                     bool (*)(void *, void *, file_t, void *), (void *)dcontext, perscxt,
                     fd, persist_user_data[idx]);
    }
    return res;
}

/* Returns true if successfully validated and de-serialized */
bool
instrument_resurrect_ro(dcontext_t *dcontext, void *perscxt, byte *map)
{
    bool res = true;
    size_t i;
    const char *c;
    ASSERT(map != NULL);

    /* Ensure we have the same set of tools (see comments above) */
    i = 0;
    c = (const char *)map;
    while (*c != '\0') {
        if (i >= num_client_libs)
            return false; /* too many clients */
        if (strcmp(client_libs[i].path, c) != 0)
            return false; /* client path mismatch */
        c += strlen(c) + 1;
        i++;
    }
    if (i < num_client_libs)
        return false; /* too few clients */
    c++;

    /* Now for clients' own data */
    if (resurrect_ro_callbacks.num > 0) {
        call_all_ret(res, = res &&, , resurrect_ro_callbacks,
                     bool (*)(void *, void *, byte **), (void *)dcontext, perscxt,
                     (byte **)&c);
    }
    return res;
}

/* Up to caller to synchronize. */
uint
instrument_persist_rx_size(dcontext_t *dcontext, void *perscxt, size_t file_offs)
{
    size_t sz = 0;
    if (persist_rx_size_callbacks.num == 0)
        return 0;
    call_all_ret(sz, +=, , persist_rx_size_callbacks,
                 size_t(*)(void *, void *, size_t, void **), (void *)dcontext, perscxt,
                 file_offs + sz, &persist_user_data[idx]);
    /* using size_t for API w/ clients in case we want to widen in future */
    CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(sz), "persisted cache size too large");
    return (uint)sz;
}

/* Up to caller to synchronize.
 * Returns true iff all writes succeeded.
 */
bool
instrument_persist_rx(dcontext_t *dcontext, void *perscxt, file_t fd)
{
    bool res = true;
    ASSERT(fd != INVALID_FILE);
    if (persist_rx_callbacks.num == 0)
        return true;
    call_all_ret(res, = res &&, , persist_rx_callbacks,
                 bool (*)(void *, void *, file_t, void *), (void *)dcontext, perscxt, fd,
                 persist_user_data[idx]);
    return res;
}

/* Returns true if successfully validated and de-serialized */
bool
instrument_resurrect_rx(dcontext_t *dcontext, void *perscxt, byte *map)
{
    bool res = true;
    ASSERT(map != NULL);
    if (resurrect_rx_callbacks.num == 0)
        return true;
    call_all_ret(res, = res &&, , resurrect_rx_callbacks,
                 bool (*)(void *, void *, byte **), (void *)dcontext, perscxt, &map);
    return res;
}

/* Up to caller to synchronize. */
uint
instrument_persist_rw_size(dcontext_t *dcontext, void *perscxt, size_t file_offs)
{
    size_t sz = 0;
    if (persist_rw_size_callbacks.num == 0)
        return 0;
    call_all_ret(sz, +=, , persist_rw_size_callbacks,
                 size_t(*)(void *, void *, size_t, void **), (void *)dcontext, perscxt,
                 file_offs + sz, &persist_user_data[idx]);
    /* using size_t for API w/ clients in case we want to widen in future */
    CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_uint(sz), "persisted cache size too large");
    return (uint)sz;
}

/* Up to caller to synchronize.
 * Returns true iff all writes succeeded.
 */
bool
instrument_persist_rw(dcontext_t *dcontext, void *perscxt, file_t fd)
{
    bool res = true;
    ASSERT(fd != INVALID_FILE);
    if (persist_rw_callbacks.num == 0)
        return true;
    call_all_ret(res, = res &&, , persist_rw_callbacks,
                 bool (*)(void *, void *, file_t, void *), (void *)dcontext, perscxt, fd,
                 persist_user_data[idx]);
    return res;
}

/* Returns true if successfully validated and de-serialized */
bool
instrument_resurrect_rw(dcontext_t *dcontext, void *perscxt, byte *map)
{
    bool res = true;
    ASSERT(map != NULL);
    if (resurrect_rw_callbacks.num == 0)
        return true;
    call_all_ret(res, = res &&, , resurrect_rx_callbacks,
                 bool (*)(void *, void *, byte **), (void *)dcontext, perscxt, &map);
    return res;
}

bool
instrument_persist_patch(dcontext_t *dcontext, void *perscxt, byte *bb_start,
                         size_t bb_size)
{
    bool res = true;
    if (persist_patch_callbacks.num == 0)
        return true;
    call_all_ret(res, = res &&, , persist_patch_callbacks,
                 bool (*)(void *, void *, byte *, size_t, void *), (void *)dcontext,
                 perscxt, bb_start, bb_size, persist_user_data[idx]);
    return res;
}

DR_API
bool
dr_register_persist_ro(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT))
{
    if (func_size == NULL || func_persist == NULL || func_resurrect == NULL)
        return false;
    add_callback(&persist_ro_size_callbacks, (void (*)(void))func_size, true);
    add_callback(&persist_ro_callbacks, (void (*)(void))func_persist, true);
    add_callback(&resurrect_ro_callbacks, (void (*)(void))func_resurrect, true);
    return true;
}

DR_API
bool
dr_unregister_persist_ro(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT))
{
    bool res = true;
    if (func_size != NULL) {
        res = remove_callback(&persist_ro_size_callbacks, (void (*)(void))func_size,
                              true) &&
            res;
    } else
        res = false;
    if (func_persist != NULL) {
        res =
            remove_callback(&persist_ro_callbacks, (void (*)(void))func_persist, true) &&
            res;
    } else
        res = false;
    if (func_resurrect != NULL) {
        res = remove_callback(&resurrect_ro_callbacks, (void (*)(void))func_resurrect,
                              true) &&
            res;
    } else
        res = false;
    return res;
}

DR_API
bool
dr_register_persist_rx(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT))
{
    if (func_size == NULL || func_persist == NULL || func_resurrect == NULL)
        return false;
    add_callback(&persist_rx_size_callbacks, (void (*)(void))func_size, true);
    add_callback(&persist_rx_callbacks, (void (*)(void))func_persist, true);
    add_callback(&resurrect_rx_callbacks, (void (*)(void))func_resurrect, true);
    return true;
}

DR_API
bool
dr_unregister_persist_rx(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT))
{
    bool res = true;
    if (func_size != NULL) {
        res = remove_callback(&persist_rx_size_callbacks, (void (*)(void))func_size,
                              true) &&
            res;
    } else
        res = false;
    if (func_persist != NULL) {
        res =
            remove_callback(&persist_rx_callbacks, (void (*)(void))func_persist, true) &&
            res;
    } else
        res = false;
    if (func_resurrect != NULL) {
        res = remove_callback(&resurrect_rx_callbacks, (void (*)(void))func_resurrect,
                              true) &&
            res;
    } else
        res = false;
    return res;
}

DR_API
bool
dr_register_persist_rw(size_t (*func_size)(void *drcontext, void *perscxt,
                                           size_t file_offs, void **user_data OUT),
                       bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                            void *user_data),
                       bool (*func_resurrect)(void *drcontext, void *perscxt,
                                              byte **map INOUT))
{
    if (func_size == NULL || func_persist == NULL || func_resurrect == NULL)
        return false;
    add_callback(&persist_rw_size_callbacks, (void (*)(void))func_size, true);
    add_callback(&persist_rw_callbacks, (void (*)(void))func_persist, true);
    add_callback(&resurrect_rw_callbacks, (void (*)(void))func_resurrect, true);
    return true;
}

DR_API
bool
dr_unregister_persist_rw(size_t (*func_size)(void *drcontext, void *perscxt,
                                             size_t file_offs, void **user_data OUT),
                         bool (*func_persist)(void *drcontext, void *perscxt, file_t fd,
                                              void *user_data),
                         bool (*func_resurrect)(void *drcontext, void *perscxt,
                                                byte **map INOUT))
{
    bool res = true;
    if (func_size != NULL) {
        res = remove_callback(&persist_rw_size_callbacks, (void (*)(void))func_size,
                              true) &&
            res;
    } else
        res = false;
    if (func_persist != NULL) {
        res =
            remove_callback(&persist_rw_callbacks, (void (*)(void))func_persist, true) &&
            res;
    } else
        res = false;
    if (func_resurrect != NULL) {
        res = remove_callback(&resurrect_rw_callbacks, (void (*)(void))func_resurrect,
                              true) &&
            res;
    } else
        res = false;
    return res;
}

DR_API
bool
dr_register_persist_patch(bool (*func_patch)(void *drcontext, void *perscxt,
                                             byte *bb_start, size_t bb_size,
                                             void *user_data))
{
    if (func_patch == NULL)
        return false;
    add_callback(&persist_patch_callbacks, (void (*)(void))func_patch, true);
    return true;
}

DR_API
bool
dr_unregister_persist_patch(bool (*func_patch)(void *drcontext, void *perscxt,
                                               byte *bb_start, size_t bb_size,
                                               void *user_data))
{
    return remove_callback(&persist_patch_callbacks, (void (*)(void))func_patch, true);
}

DR_API
bool
dr_is_detaching(void)
{
    return doing_detach;
}

/**************************************************
 * OPEN-ADDRESS HASHTABLE
 *
 * Some uses cases need an open-address hashtable that does not use 3rd-party
 * libraries.  Rather than add something to drcontainers, we simply export the
 * hashtablex.h-based table directly from DR.
 */

DR_API
void *
dr_hashtable_create(void *drcontext, uint bits, uint load_factor_percent, bool synch,
                    void (*free_payload_func)(void * /*drcontext*/, void *))
{
    uint flags = HASHTABLE_PERSISTENT;
    if (synch)
        flags |= HASHTABLE_SHARED | HASHTABLE_ENTRY_SHARED;
    else
        flags |= HASHTABLE_LOCKLESS_ACCESS;
    return generic_hash_create(
        (dcontext_t *)drcontext, bits, load_factor_percent, flags,
        (void (*)(dcontext_t *, void *))free_payload_func _IF_DEBUG("client"));
}

DR_API
void
dr_hashtable_destroy(void *drcontext, void *htable)
{
    generic_hash_destroy((dcontext_t *)drcontext, (generic_table_t *)htable);
}

DR_API
void
dr_hashtable_clear(void *drcontext, void *htable)
{
    generic_hash_clear((dcontext_t *)drcontext, (generic_table_t *)htable);
}

DR_API
void *
dr_hashtable_lookup(void *drcontext, void *htable, ptr_uint_t key)
{
    return generic_hash_lookup((dcontext_t *)drcontext, (generic_table_t *)htable, key);
}

DR_API
void
dr_hashtable_add(void *drcontext, void *htable, ptr_uint_t key, void *payload)
{
    generic_hash_add((dcontext_t *)drcontext, (generic_table_t *)htable, key, payload);
}

DR_API
bool
dr_hashtable_remove(void *drcontext, void *htable, ptr_uint_t key)
{
    return generic_hash_remove((dcontext_t *)drcontext, (generic_table_t *)htable, key);
}
