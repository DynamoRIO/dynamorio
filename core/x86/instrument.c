/* **********************************************************
 * Copyright (c) 2002-2009 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

/*
 * instrument.c - interface for instrumentation
 */

#include "../globals.h"   /* just to disable warning C4206 about an empty file */


#include "instrument.h"
#include "../instrlist.h"
#include "arch.h"
#include "instr.h"
#include "instr_create.h"
#include "decode.h"
#include "disassemble.h"
#include "../fragment.h"
#include "../emit.h"
#include "../link.h"
#include "../monitor.h" /* for mark_trace_head */
#include <string.h> /* for strstr */
#include <stdarg.h> /* for varargs */
#ifdef WINDOWS
# include "../nudge.h" /* for nudge_internal() */
#endif

#ifdef CLIENT_INTERFACE
/* in utils.c, not exported to everyone */
extern void do_file_write(file_t f, const char *fmt, va_list ap);

#ifdef DEBUG
/* case 10450: give messages to clients */
/* we can't undef ASSERT b/c of DYNAMO_OPTION */
# undef ASSERT_TRUNCATE
# undef ASSERT_BITFIELD_TRUNCATE
# undef ASSERT_NOT_REACHED
# define ASSERT_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_BITFIELD_TRUNCATE DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
# define ASSERT_NOT_REACHED DO_NOT_USE_ASSERT_USE_CLIENT_ASSERT_INSTEAD
#endif

/* PR 200065: User passes us the shared library, we look up "dr_init",
 * and call it.  From there, the client can register which events it
 * wishes to receive.
 */
#define INSTRUMENT_INIT_NAME "dr_init"

/* PR 250952: version check 
 * If changing this, don't forget to update:
 * - lib/dr_defines.h _USES_DR_VERSION_
 * - api/docs/footer.html
 */
#define USES_DR_VERSION_NAME "_USES_DR_VERSION_"
/* should we expose this for use in samples/tracedump.c? */
#define OLDEST_COMPATIBLE_VERSION  96 /* 0.9.6 == 1.0.0 through 1.2.0 */
/* The 3rd version number, the bugfix/patch number, should not affect
 * compatibility, so our version check number simply uses:
 *   major*100 + minor
 * Which gives us room for 100 minor versions per major.
 */
#define NEWEST_COMPATIBLE_VERSION 103 /* 1.3.x */

/* Store the unique not-part-of-version build number (the version
 * BUILD_NUMBER is limited to 64K and is not guaranteed to be unique)
 * somewhere accessible at a customer site.  We could alternatively
 * pull it out of our DYNAMORIO_DEFINES string.
 */
DR_API const char *unique_build_number = STRINGIFY(UNIQUE_BUILD_NUMBER);

/* Acquire when registering or unregistering event callbacks */
DECLARE_CXTSWPROT_VAR(static mutex_t callback_registration_lock,
                      INIT_LOCK_FREE(callback_registration_lock));

/* Structures for maintaining lists of event callbacks */
typedef void (*callback_t)(void);
typedef struct _callback_list_t {
    callback_t *callbacks;      /* array of callback functions */
    size_t num;                 /* number of callbacks registered */
    size_t size;                /* allocated space (may be larger than num) */
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
 * We consider the first registered callback to have the highest
 * priority and call it last.  If we gave the last registered callback
 * the highest priority, a client could re-register a routine to
 * increase its priority.  That seems a little weird.
 */
#define FAST_COPY_SIZE 5
#define call_all_ret(ret, retop, postop, vec, type, ...)                \
    do {                                                                \
        size_t idx, num;                                                \
        mutex_lock(&callback_registration_lock);                        \
        num = (vec).num;                                                \
        if (num == 0) {                                                 \
            mutex_unlock(&callback_registration_lock);                  \
        }                                                               \
        else if (num <= FAST_COPY_SIZE) {                               \
            callback_t tmp[FAST_COPY_SIZE];                             \
            memcpy(tmp, (vec).callbacks, num * sizeof(callback_t));     \
            mutex_unlock(&callback_registration_lock);                  \
            for (idx=0; idx<num; idx++) {                               \
                ret retop (((type)tmp[num-idx-1])(__VA_ARGS__)) postop; \
            }                                                           \
        }                                                               \
        else {                                                          \
            callback_t *tmp = HEAP_ARRAY_ALLOC(GLOBAL_DCONTEXT, callback_t, num, \
                                               ACCT_OTHER, UNPROTECTED); \
            memcpy(tmp, (vec).callbacks, num * sizeof(callback_t));     \
            mutex_unlock(&callback_registration_lock);                  \
            for (idx=0; idx<num; idx++) {                               \
                ret retop ((type)tmp[num-idx-1])(__VA_ARGS__) postop;   \
            }                                                           \
            HEAP_ARRAY_FREE(GLOBAL_DCONTEXT, tmp, callback_t, num,      \
                            ACCT_OTHER, UNPROTECTED);                    \
        }                                                               \
    } while (0)

/* It's less error-prone if we just have one call_all macro.  We'll
 * reuse call_all_ret above for callbacks that don't have a return
 * value by assigning to a dummy var.  Note that this means we'll
 * have to pass an int-returning type to call_all()
 */
#define call_all(vec, type, ...)                        \
    do {                                                \
        int dummy;                                      \
        call_all_ret(dummy, =, , vec, type, __VA_ARGS__); \
    } while (0)

/* Lists of callbacks for each event type.  Note that init and nudge
 * callback lists are kept in the client_lib_t data structure below.
 * We could store all lists on a per-client basis, but we can iterate
 * over these lists slightly more efficiently if we store all
 * callbacks for a specific event in a single list.
 */
static callback_list_t exit_callbacks = {0,};
static callback_list_t thread_init_callbacks = {0,};
static callback_list_t thread_exit_callbacks = {0,};
#ifdef LINUX
static callback_list_t fork_init_callbacks = {0,};
#endif
static callback_list_t bb_callbacks = {0,};
static callback_list_t trace_callbacks = {0,};
#ifdef CUSTOM_TRACES
static callback_list_t end_trace_callbacks = {0,};
#endif
static callback_list_t fragdel_callbacks = {0,};
static callback_list_t restore_state_callbacks = {0,};
static callback_list_t module_load_callbacks = {0,};
static callback_list_t module_unload_callbacks = {0,};
static callback_list_t filter_syscall_callbacks = {0,};
static callback_list_t pre_syscall_callbacks = {0,};
static callback_list_t post_syscall_callbacks = {0,};
#ifdef WINDOWS
static callback_list_t exception_callbacks = {0,};
#else
static callback_list_t signal_callbacks = {0,};
#endif
#ifdef PROGRAM_SHEPHERDING
static callback_list_t security_violation_callbacks = {0,};
#endif

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
    char options[MAX_OPTION_LENGTH];
    /* We need to associate nudge events with a specific client so we
     * store that list here in the client_lib_t instead of using a
     * single global list.
     */
    callback_list_t nudge_callbacks;
} client_lib_t;

/* these should only be modified prior to instrument_init(), since no
 * readers of the client_libs array (event handlers, etc.) use synch
 */
static client_lib_t client_libs[MAX_CLIENT_LIBS] = {{0,}};
static size_t num_client_libs = 0;

#ifdef WINDOWS
/* used for nudge support */
static bool block_client_owned_threads = false;
DECLARE_CXTSWPROT_VAR(static int num_client_owned_threads, 0);
/* protects block_client_owned_threads and incrementing num_client_owned_threads */
DECLARE_CXTSWPROT_VAR(static mutex_t client_nudge_count_lock,
                      INIT_LOCK_FREE(client_nudge_count_lock));
#endif

/****************************************************************************/
/* INTERNAL ROUTINES */

static void
add_callback(callback_list_t *vec, void (*func)(void), bool unprotect)
{
    if (func == NULL) {
        CLIENT_ASSERT(false, "trying to register a NULL callback");
        return;
    }

    mutex_lock(&callback_registration_lock);
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
        callback_t *tmp = HEAP_ARRAY_ALLOC
            (GLOBAL_DCONTEXT, callback_t, vec->size + 2, /* Let's allocate 2 */
             ACCT_OTHER, UNPROTECTED);

        if (tmp == NULL) {
            CLIENT_ASSERT(false, "out of memory: can't register callback");
            mutex_unlock(&callback_registration_lock);        
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
    mutex_unlock(&callback_registration_lock);
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

    mutex_lock(&callback_registration_lock);
    /* Although we're receiving a pointer to a callback_list_t, we're
     * usually modifying a static var.
     */
    if (unprotect) {
        SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    }

    for (i=0; i<vec->num; i++) {
        if (vec->callbacks[i] == func) {
            size_t j;
            /* shift down the entries on the tail */
            for (j=i; j<vec->num-1; j++) {
                vec->callbacks[j] = vec->callbacks[j+1];
            }

            vec->num -= 1;
            found = true;
            break;
        }
    }

    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
    if (unprotect) {
        mutex_unlock(&callback_registration_lock);
    }

    return found;
}

/* This should only be called prior to instrument_init(),
 * since no readers of the client_libs array use synch
 * and since this routine assumes .data is writable.
 */
static void
add_client_lib(char *path, char *id_str, char *options)
{
    client_id_t id;
    shlib_handle_t client_lib;
    DEBUG_DECLARE(size_t i);

    ASSERT(!dynamo_initialized);

    /* if ID not specified, we'll default to 0 */
    id = (id_str == NULL) ? 0 : strtoul(id_str, NULL, 16);

#ifdef DEBUG
    /* Check for conflicting IDs */
    for (i=0; i<num_client_libs; i++) {
        CLIENT_ASSERT(client_libs[i].id != id, "Clients have the same ID");
    }
#endif

    if (num_client_libs == MAX_CLIENT_LIBS) {
        CLIENT_ASSERT(false, "Max number of clients reached");
        return;
    }

    client_lib = load_shared_library(path);
    if (client_lib == NULL) {
        char msg[MAXIMUM_PATH*4];
        char err[MAXIMUM_PATH*2];
        shared_library_error(err, BUFFER_SIZE_ELEMENTS(err));
        snprintf(msg, BUFFER_SIZE_ELEMENTS(msg),
                 "\n\tError opening instrumentation library %s:\n\t%s",
                 path, err);
        NULL_TERMINATE_BUFFER(msg);

        /* PR 232490 - malformed library names or incorrect
         * permissions shouldn't blow up an app in release builds as
         * they may happen at customer sites with a third party
         * client.
         */
        CLIENT_ASSERT(false, msg);
    }
    else {
        /* PR 250952: version check */
        int *uses_dr_version = (int *)
            lookup_library_routine(client_lib, USES_DR_VERSION_NAME);
        if (uses_dr_version == NULL ||
            *uses_dr_version < OLDEST_COMPATIBLE_VERSION ||
            *uses_dr_version > NEWEST_COMPATIBLE_VERSION) {
            /* not a fatal usage error since we want release build to continue */
            CLIENT_ASSERT(false, 
                          "client library is incompatible with this version of DR");
            SYSLOG(SYSLOG_WARNING, CLIENT_VERSION_INCOMPATIBLE, 2, 
                   get_application_name(), get_application_pid());
        }
        else {
            size_t idx = num_client_libs++;
            DEBUG_DECLARE(bool ok;)
            client_libs[idx].id = id;
            client_libs[idx].lib = client_lib;
            DEBUG_DECLARE(ok =)
                shared_library_bounds(client_lib, (byte *) uses_dr_version,
                                      &client_libs[idx].start, &client_libs[idx].end);
            ASSERT(ok);

            LOG(GLOBAL, LOG_INTERP, 1, "loaded %s at "PFX"-"PFX"\n",
                path, client_libs[idx].start, client_libs[idx].end);
#ifdef X64
            request_region_be_heap_reachable(client_libs[idx].start,
                                             client_libs[idx].end -
                                             client_libs[idx].start);
#endif
            strncpy(client_libs[idx].path, path,
                    BUFFER_SIZE_ELEMENTS(client_libs[idx].path));
            NULL_TERMINATE_BUFFER(client_libs[idx].path);

            if (options != NULL) {
                strncpy(client_libs[idx].options, options,
                        BUFFER_SIZE_ELEMENTS(client_libs[idx].options));
                NULL_TERMINATE_BUFFER(client_libs[idx].options);
            }

            /* We'll look up dr_init and call it in instrument_init */
        }
    }
}

void
instrument_load_client_libs(void)
{
    if (!IS_INTERNAL_STRING_OPTION_EMPTY(client_lib)) {
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

            add_client_lib(path, id, options);
            path = next_path;
        } while (path != NULL);
    }
}

void
instrument_init(void)
{
    /* Iterate over the client libs and call each dr_init */
    size_t i;
    for (i=0; i<num_client_libs; i++) {
        void (*init)(client_id_t) = (void (*)(client_id_t))
            (lookup_library_routine(client_libs[i].lib, INSTRUMENT_INIT_NAME));
        /* Since the user has to register all other events, it
         * doesn't make sense to provide the -client_lib
         * option for a module that doesn't export dr_init.
         */
        CLIENT_ASSERT(init != NULL,
                      "client library does not export a dr_init routine");
        (*init)(client_libs[i].id);
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
            dr_free_module_data(data);
        }
        dr_module_iterator_stop(mi);
    }

    /* We now initialize the 1st thread before coming here, so we can
     * hand the client a dcontext; so we need to specially generate
     * the thread init event now.  An alternative is to have
     * dr_get_global_drcontext(), but that's extra complexity for no
     * real reason.
     */
    if (thread_init_callbacks.num > 0) {
        instrument_thread_init(get_thread_private_dcontext());
    }
}

#ifdef DEBUG
void
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

void free_all_callback_lists()
{
    free_callback_list(&exit_callbacks);
    free_callback_list(&thread_init_callbacks);
    free_callback_list(&thread_exit_callbacks);
#ifdef LINUX
    free_callback_list(&fork_init_callbacks);
#endif
    free_callback_list(&bb_callbacks);
    free_callback_list(&trace_callbacks);
#ifdef CUSTOM_TRACES
    free_callback_list(&end_trace_callbacks);
#endif
    free_callback_list(&fragdel_callbacks);
    free_callback_list(&restore_state_callbacks);
    free_callback_list(&module_load_callbacks);
    free_callback_list(&module_unload_callbacks);
    free_callback_list(&filter_syscall_callbacks);
    free_callback_list(&pre_syscall_callbacks);
    free_callback_list(&post_syscall_callbacks);
#ifdef WINDOWS
    free_callback_list(&exception_callbacks);
#else
    free_callback_list(&signal_callbacks);
#endif
#ifdef PROGRAM_SHEPHERDING
    free_callback_list(&security_violation_callbacks);
#endif
}
#endif /* DEBUG */

void
instrument_exit(void)
{
    DEBUG_DECLARE(size_t i);
    /* Note - currently own initexit lock when this is called (see PR 227619). */

    call_all(exit_callbacks, int (*)(),
             /* It seems the compiler is confused if we pass no var args
              * to the call_all macro.  Bogus NULL arg */
             NULL);

#ifdef DEBUG
    /* Unload all client libs and free any allocated storage */
    for (i=0; i<num_client_libs; i++) {
        free_callback_list(&client_libs[i].nudge_callbacks);
        unload_shared_library(client_libs[i].lib);
    }

    free_all_callback_lists();
#endif

#ifdef WINDOWS
    DELETE_LOCK(client_nudge_count_lock);
#endif
    DELETE_LOCK(callback_registration_lock);
}

bool
is_in_client_lib(app_pc addr)
{
    /* NOTE: we use this routine for detecting exceptions in
     * clients. If we add a callback on that event we'll have to be
     * sure to deliver it only to the right client.
     */
    size_t i;
    for (i=0; i<num_client_libs; i++) {
        if ((addr >= (app_pc)client_libs[i].start) &&
            (addr < client_libs[i].end)) {
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

void
dr_register_bb_event(dr_emit_flags_t (*func)
                     (void *drcontext, void *tag, instrlist_t *bb,
                      bool for_trace, bool translating))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for bb event when code_api is disabled");
        return;
    }

    add_callback(&bb_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_bb_event(dr_emit_flags_t (*func)
                       (void *drcontext, void *tag, instrlist_t *bb,
                        bool for_trace, bool translating))
{
    return remove_callback(&bb_callbacks, (void (*)(void))func, true);
}

void
dr_register_trace_event(dr_emit_flags_t (*func)
                        (void *drcontext, void *tag, instrlist_t *trace,
                         bool translating))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for trace event when code_api is disabled");
        return;
    }

    add_callback(&trace_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_trace_event(dr_emit_flags_t (*func)
                          (void *drcontext, void *tag, instrlist_t *trace,
                           bool translating))
{
    return remove_callback(&trace_callbacks, (void (*)(void))func, true);
}

#ifdef CUSTOM_TRACES
void
dr_register_end_trace_event(dr_custom_trace_action_t (*func)
                            (void *drcontext, void *tag, void *next_tag))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for end-trace event when code_api is disabled");
        return;
    }

    add_callback(&end_trace_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_end_trace_event(dr_custom_trace_action_t
                              (*func)(void *drcontext, void *tag, void *next_tag))
{
    return remove_callback(&end_trace_callbacks, (void (*)(void))func, true);
}
#endif

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
dr_register_restore_state_event(void (*func)
                                (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                 bool restore_memory, bool app_code_consistent))
{
    if (!INTERNAL_OPTION(code_api)) {
        CLIENT_ASSERT(false, "asking for restore state event when code_api is disabled");
        return;
    }

    add_callback(&restore_state_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_restore_state_event(void (*func)
                                  (void *drcontext, void *tag, dr_mcontext_t *mcontext,
                                   bool restore_memory, bool app_code_consistent))
{
    return remove_callback(&restore_state_callbacks, (void (*)(void))func, true);
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

#ifdef LINUX
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
dr_register_module_unload_event(void (*func)(void *drcontext,
                                             const module_data_t *info))
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
dr_register_exception_event(void (*func)(void *drcontext, dr_exception_t *excpt))
{
    add_callback(&exception_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_exception_event(void (*func)(void *drcontext, dr_exception_t *excpt))
{
    return remove_callback(&exception_callbacks, (void (*)(void))func, true);
}
#else
void
dr_register_signal_event(dr_signal_action_t (*func)
                         (void *drcontext, dr_siginfo_t *siginfo))
{
    add_callback(&signal_callbacks, (void (*)(void))func, true);
}

bool
dr_unregister_signal_event(dr_signal_action_t (*func)
                           (void *drcontext, dr_siginfo_t *siginfo))
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

#ifdef WINDOWS
void
dr_register_nudge_event(void (*func)(void *drcontext, uint64 argument), client_id_t id)
{
    size_t i;
    for (i=0; i<num_client_libs; i++) {
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
    for (i=0; i<num_client_libs; i++) {
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

bool
dr_nudge_client(client_id_t client_id, uint64 argument)
{
    size_t i;

    for (i=0; i<num_client_libs; i++) {
        if (client_libs[i].id == client_id) {
            if (client_libs[i].nudge_callbacks.num == 0) {
                CLIENT_ASSERT(false, "dr_nudge_client: no nudge handler registered");
                return false;
            }
            return nudge_internal(NUDGE_GENERIC(client), argument, client_id);
        }
    }

    return false;
}
#endif /* WINDOWS */

void
instrument_thread_init(dcontext_t *dcontext)
{
    /* Note that we're called twice: once prior to instrument_init()
     * (PR 216936) to set up the dcontext client field, and once
     * after instrument_init() to call the client event.
     */
    if (dcontext->client_data == NULL) {
        dcontext->client_data = HEAP_TYPE_ALLOC(dcontext, client_data_t,
                                                ACCT_OTHER, UNPROTECTED);
        memset(dcontext->client_data, 0x0, sizeof(client_data_t));

#ifdef CLIENT_SIDELINE
        ASSIGN_INIT_LOCK_FREE(dcontext->client_data->sideline_mutex, sideline_mutex);
        ASSIGN_INIT_LOCK_FREE(dcontext->client_data->sideline_heap_lock,
                              sideline_heap_lock);
#endif
    }

    call_all(thread_init_callbacks, int (*)(void *), (void *)dcontext);
}

#ifdef LINUX
void
instrument_fork_init(dcontext_t *dcontext)
{
    call_all(fork_init_callbacks, int (*)(void *), (void *)dcontext);
}
#endif

void
instrument_thread_exit(dcontext_t *dcontext)
{
    client_todo_list_t *todo;
    client_flush_req_t *flush;

    /* Note - currently own initexit lock when this is called (see PR 227619). */
    call_all(thread_exit_callbacks, int (*)(void *), (void *)dcontext);

#ifdef CLIENT_SIDELINE
    DELETE_LOCK(dcontext->client_data->sideline_mutex);
    DELETE_LOCK(dcontext->client_data->sideline_heap_lock);
#endif

    /* could be heap space allocated for the todo list */
    todo = dcontext->client_data->to_do;
    while (todo != NULL) {
        client_todo_list_t *next_todo = todo->next;
        if (todo->ilist != NULL) {
            instrlist_clear_and_destroy(dcontext, todo->ilist);
        }
        HEAP_TYPE_FREE(dcontext, todo, client_todo_list_t, ACCT_OTHER, UNPROTECTED);
        todo = next_todo;
    }

    /* could be heap space allocated for the flush list */
    flush = dcontext->client_data->flush_list;
    while (flush != NULL) {
        client_flush_req_t *next_flush = flush->next;
        HEAP_TYPE_FREE(dcontext, flush, client_flush_req_t, ACCT_OTHER, UNPROTECTED);
        flush = next_flush;
    }        

    HEAP_TYPE_FREE(dcontext, dcontext->client_data, client_data_t,
                   ACCT_OTHER, UNPROTECTED);
    dcontext->client_data = NULL; /* for mutex_wait_contended_lock() */
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

static bool
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
        /* Landing pads that exist between hook points and the trampolines
         * shouldn't be seen by the client too.  PR 250294.
         */
        vmvector_overlap(landing_pad_areas, tag, tag + 1) ||
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
        is_syscall_trampoline(tag))
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
        if (instr_ok_to_mangle(in)) {
            DOLOG(LOG_INTERP, 1, {
                if (instr_get_translation(in) == NULL)
                    loginst(get_thread_private_dcontext(), 1, in, "translation is NULL");
            });
            CLIENT_ASSERT(instr_get_translation(in) != NULL,
                          "translation field must be set for every non-meta instruction");
        } else {
            /* The meta instr could indeed not affect app state, but
             * better I think to assert and make them put in an
             * empty restore event callback in that case. */
            DOLOG(LOG_INTERP, 1, {
                if (instr_get_translation(in) != NULL &&
                    !instr_is_our_mangling(in) &&
                    restore_state_callbacks.num == 0)
                    loginst(get_thread_private_dcontext(), 1, in, "translation != NULL");
            });
            CLIENT_ASSERT(instr_get_translation(in) == NULL ||
                          instr_is_our_mangling(in) ||
                          restore_state_callbacks.num > 0,
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
instrument_basic_block(dcontext_t *dcontext, app_pc tag, instrlist_t *bb,
                       bool for_trace, bool translating, dr_emit_flags_t *emitflags)
{
    dr_emit_flags_t ret = DR_EMIT_DEFAULT;
    
    /* return false if no BB hooks are registered */
    if (bb_callbacks.num == 0)
        return false;

    if (hide_tag_from_client(tag))
        return false;

    /* do not expand or up-decode the instrlist, client gets to choose
     * whether and how to do that
     */

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\ninstrument_basic_block ******************\n");
    LOG(THREAD, LOG_INTERP, 3, "\nbefore instrumentation:\n");
    if (stats->loglevel >= 3 && (stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, bb, THREAD);
#endif

    /* Note - currently we are couldbelinking and hold the
     * bb_building lock when this is called (see PR 227619).
     */
    /* We or together the return values */
    call_all_ret(ret, |=, , bb_callbacks,
                 int (*) (void *, void *, instrlist_t *, bool, bool),
                 (void *)dcontext, (void *)tag, bb, for_trace, translating);
    if (emitflags != NULL)
        *emitflags = ret;
    DODEBUG({ check_ilist_translations(bb); });

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\nafter instrumentation:\n");
    if (stats->loglevel >= 3 && (stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, bb, THREAD);
#endif

    return true;
}

/* Give the user the completely mangled and optimized trace just prior
 * to emitting into code cache, user gets final crack at it
 */
dr_emit_flags_t
instrument_trace(dcontext_t *dcontext, app_pc tag, instrlist_t *trace,
                 bool translating)
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
    if (stats->loglevel >= 3 && (stats->logmask & LOG_INTERP) != 0)
        instrlist_disassemble(dcontext, tag, trace, THREAD);
#endif

    /* We always pass Level 3 instrs to the client, since we no longer
     * expose the expansion routines.
     */
#ifdef UNSUPPORTED_API
    for (instr = instrlist_first_expanded(dcontext, trace);
         instr != NULL;
         instr = instr_get_next_expanded(dcontext, trace, instr)) {
        instr_decode(dcontext, instr);
    }
    /* ASSUMPTION: all ctis are already at Level 3, so we don't have
     * to do a separate pass to fix up intra-list targets like
     * instrlist_decode_cti() does
     */
#endif

    /* We or together the return values */
    call_all_ret(ret, |=, , trace_callbacks,
                 int (*)(void *, void *, instrlist_t *, bool),
                 (void *)dcontext, (void *)tag, trace, translating);

    DODEBUG({ check_ilist_translations(trace); });

#ifdef DEBUG
    LOG(THREAD, LOG_INTERP, 3, "\nafter instrumentation:\n");
    if (stats->loglevel >= 3 && (stats->logmask & LOG_INTERP) != 0)
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

    call_all(fragdel_callbacks, int (*)(void *, void *),
             (void *)dcontext, (void *)tag);
}

void
instrument_restore_state(dcontext_t *dcontext, app_pc tag, dr_mcontext_t *mc,
                         bool restore_memory, bool app_code_consistent)
{
    if (restore_state_callbacks.num == 0)
        return;

    call_all(restore_state_callbacks,
             int (*)(void *, void *, dr_mcontext_t *, bool, bool),
             (void *)dcontext, (void *)tag, mc, restore_memory, app_code_consistent);
}

#ifdef CUSTOM_TRACES
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
#endif


static module_data_t *
create_and_initialize_module_data(app_pc start, app_pc end, app_pc entry_point,
                                  uint flags, const module_names_t *names
#ifdef WINDOWS
                                  , version_number_t file_version,
                                  version_number_t product_version,
                                  uint checksum, uint timestamp,
                                  size_t mod_size
#endif
                                  )
{
    module_data_t *copy = (module_data_t *)
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, module_data_t, ACCT_OTHER, UNPROTECTED);
    memset(copy, 0, sizeof(module_data_t));

    copy->start = start;
    copy->end = end;
    copy->entry_point = entry_point;
    copy->flags = flags;

    if (names->module_name != NULL)
        copy->names.module_name = dr_strdup(names->module_name HEAPACCT(ACCT_OTHER));
    if (names->file_name != NULL)
        copy->names.file_name = dr_strdup(names->file_name HEAPACCT(ACCT_OTHER));
#ifdef WINDOWS
    if (names->exe_name != NULL)
        copy->names.exe_name = dr_strdup(names->exe_name HEAPACCT(ACCT_OTHER));
    if (names->rsrc_name != NULL)
        copy->names.rsrc_name = dr_strdup(names->rsrc_name HEAPACCT(ACCT_OTHER));

    copy->file_version = file_version;
    copy->product_version = product_version;
    copy->checksum = checksum;
    copy->timestamp = timestamp;
    copy->module_internal_size = mod_size;
#endif
    return copy;
}

module_data_t *
copy_module_area_to_module_data(const module_area_t *area)
{
    if (area == NULL)
        return NULL;

    return create_and_initialize_module_data(area->start, area->end, area->entry_point,
                                             0, &area->names
#ifdef WINDOWS
                                             , area->os_data.file_version,
                                             area->os_data.product_version,
                                             area->os_data.checksum,
                                             area->os_data.timestamp,
                                             area->os_data.module_internal_size
#endif
                                             );
}

DR_API
/* Makes a copy of a module_data_t for returning to the client.  We return a copy so
 * we don't have to hold the module areas list lock while in the client (xref PR 225020).
 * Note - dr_data is allowed to be NULL. */
module_data_t *
dr_copy_module_data(const module_data_t *data)
{
    if (data == NULL)
        return NULL;

    return create_and_initialize_module_data(data->start, data->end, data->entry_point,
                                             0, &data->names
#ifdef WINDOWS
                                             , data->file_version,
                                             data->product_version,
                                             data->checksum, data->timestamp,
                                             data->module_internal_size
#endif
                                             );
}

DR_API
/* Used to free a module_data_t created by dr_copy_module_data() */
void
dr_free_module_data(module_data_t *data)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    if (data == NULL)
        return;

    if (dcontext != NULL && data == dcontext->client_data->no_delete_mod_data) {
        CLIENT_ASSERT(false, "dr_free_module_data: don\'t free module_data passed to "
                      "the image load or image unload event callbacks.");
        return;
    }

    free_module_names(&data->names HEAPACCT(ACCT_OTHER));

    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, data, module_data_t, ACCT_OTHER, UNPROTECTED);
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

    call_all(module_unload_callbacks, int (*)(void *, module_data_t *),
             (void *)dcontext, data);

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
        /* skip syscall if any client wants to skip it */
        call_all_ret(exec, = exec &&, , pre_syscall_callbacks,
                     bool (*)(void *, int), (void *)dcontext, sysnum);
    }
    dcontext->client_data->in_pre_syscall = false;
    return exec;
}

void
instrument_post_syscall(dcontext_t *dcontext, int sysnum)
{
    if (post_syscall_callbacks.num == 0)
        return;
    dcontext->client_data->in_post_syscall = true;
    call_all(post_syscall_callbacks, int (*)(void *, int),
             (void *)dcontext, sysnum);
    dcontext->client_data->in_post_syscall = false;
}

bool
instrument_invoke_another_syscall(dcontext_t *dcontext)
{
    return dcontext->client_data->invoke_another_syscall;
}

#ifdef WINDOWS
/* Notify user of exceptions.  Note: not called for RaiseException */
void
instrument_exception(dcontext_t *dcontext, dr_exception_t *exception)
{
    call_all(exception_callbacks, int (*)(void *, dr_exception_t *),
             (void *)dcontext, exception);
}
#else
dr_signal_action_t
instrument_signal(dcontext_t *dcontext, dr_siginfo_t *siginfo)
{
    dr_signal_action_t ret = DR_SIGNAL_DELIVER;
    /* Highest priority callback decides what to do with signal.
     * If we get rid of DR_SIGNAL_BYPASS we could change to a bool and
     * then only deliver to app if nobody suppresses.
     */
    call_all_ret(ret, =, , signal_callbacks,
                 dr_signal_action_t (*)(void *, dr_siginfo_t *),
                 (void *)dcontext, siginfo);
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

    if (security_violation_callbacks.num == 0)
        return;

    /* FIXME - the source_tag, source_pc, and context can all be incorrect if the
     * violation ends up occuring in the middle of a bb we're building.  See case
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
      case STACK_EXECUTION_VIOLATION:
          dr_violation = DR_RCO_STACK_VIOLATION;
          break;
      case HEAP_EXECUTION_VIOLATION:
          dr_violation = DR_RCO_HEAP_VIOLATION;
          break;
      case RETURN_TARGET_VIOLATION:
          dr_violation = DR_RCT_RETURN_VIOLATION;
          break;
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
      case ACTION_TERMINATE_PROCESS:
          dr_action = DR_VIOLATION_ACTION_KILL_PROCESS;
          break;
      case ACTION_CONTINUE:
          dr_action = DR_VIOLATION_ACTION_CONTINUE;
          break;
      case ACTION_TERMINATE_THREAD:
          dr_action = DR_VIOLATION_ACTION_KILL_THREAD;
          break;
      case ACTION_THROW_EXCEPTION:
          dr_action = DR_VIOLATION_ACTION_THROW_EXCEPTION;
          break;
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
             (void *)dcontext, last->tag, source_pc, target_pc,
             dr_violation, get_mcontext(dcontext), &dr_action);

    if (dr_action != dr_action_original) {
        switch(dr_action) {
        case DR_VIOLATION_ACTION_KILL_PROCESS:
            *action = ACTION_TERMINATE_PROCESS;
            break;
        case DR_VIOLATION_ACTION_KILL_THREAD:
            *action = ACTION_TERMINATE_THREAD;
            break;
        case DR_VIOLATION_ACTION_THROW_EXCEPTION:
            *action = ACTION_THROW_EXCEPTION;
            break;
        case DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT:
            /* FIXME - not safe to implement till case 7380 is fixed. */
            CLIENT_ASSERT(false, "action DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT "
                          "not yet supported.");
            /* note - no break, fall through */
        case DR_VIOLATION_ACTION_CONTINUE:
            *action = ACTION_CONTINUE;
            break;
        default:
            CLIENT_ASSERT(false, "Security violation event callback returned invalid "
                          "action value.");
        }
    }
}
#endif

#ifdef WINDOWS
/* Notify the client of a nudge. */
void
instrument_nudge(dcontext_t *dcontext, client_id_t id, uint64 arg)
{
    size_t i;

    ASSERT(dcontext != NULL && dcontext != GLOBAL_DCONTEXT &&
           dcontext == get_thread_private_dcontext());
    /* synch_with_all_threads and flush API assume that client nudge threads
     * hold no dr locks and are !couldbelinking while in client lib code */
    ASSERT_OWN_NO_LOCKS();
    ASSERT(!is_couldbelinking(dcontext));

    /* find the client the nudge is intended for */
    for (i=0; i<num_client_libs; i++) {
        if (client_libs[i].id == id) {
            break;
        }
    }

    if (i == num_client_libs || client_libs[i].nudge_callbacks.num == 0)
        return;

    /* count the number of nudge events so we can make sure they're
     * all finished before exiting
     */
    mutex_lock(&client_nudge_count_lock);
    if (block_client_owned_threads) {
        /* FIXME - would be nice if there was a way to let the external agent know that
         * the nudge event wasn't delivered (but this only happens when the process
         * is detaching or exiting). */
        mutex_unlock(&client_nudge_count_lock);
        return;
    }

    /* atomic to avoid locking around the dec */
    ATOMIC_INC(int, num_client_owned_threads);
    mutex_unlock(&client_nudge_count_lock);

    /* We need to mark this as a client controlled thread for synch_with_all_threads
     * and otherwise treat it as native.  Xref PR 230836 on what to do if this
     * thread hits native_exec_syscalls hooks. */
    dcontext->client_data->is_client_thread = true;
    dcontext->thread_record->under_dynamo_control = false;

    call_all(client_libs[i].nudge_callbacks, int (*)(void *, uint64), 
             (void *)dcontext, arg);

    dcontext->thread_record->under_dynamo_control = true;
    dcontext->client_data->is_client_thread = false;

    ATOMIC_DEC(int, num_client_owned_threads);
}

/* wait for all nudges to finish */
void
wait_for_outstanding_nudges()
{
    /* block any new nudge threads from starting */
    mutex_lock(&client_nudge_count_lock);
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    block_client_owned_threads = true;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    DOLOG(1, LOG_TOP, {
        if (num_client_owned_threads > 0) {
            LOG(GLOBAL, LOG_TOP, 1,
                "Waiting for %d nudges to finish - app is about to kill all threads "
                "except the current one./n", num_client_owned_threads);
        }
    });

    while (num_client_owned_threads > 0) {
        /* yield with lock released to allow nudges to finish */
        mutex_unlock(&client_nudge_count_lock);
        dr_thread_yield();
        mutex_lock(&client_nudge_count_lock);
    }
    mutex_unlock(&client_nudge_count_lock);
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
    return (void *) dcontext;
}

DR_API
/* Aborts the process immediately */
void
dr_abort(void)
{
    os_terminate(NULL, TERMINATE_PROCESS);
}

DR_API
/* Returns true if all \DynamoRIO caches are thread private. */
bool
dr_using_all_private_caches(void)
{
    return !SHARED_FRAGMENTS_ENABLED();
}

DR_API
/* Returns the option string passed along with a client path via DR's
 * -client_lib option.
 */
const char *
dr_get_options(client_id_t id)
{
    size_t i;
    for (i=0; i<num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return client_libs[i].options;
        }
    }

    CLIENT_ASSERT(false, "dr_get_options(): invalid client id");
    return NULL;
}

DR_API 
/* Returns the path to the client library.  Client must pass its ID */
const char *
dr_get_client_path(client_id_t id)
{
    size_t i;
    for (i=0; i<num_client_libs; i++) {
        if (client_libs[i].id == id) {
            return client_libs[i].path;
        }
    }

    CLIENT_ASSERT(false, "dr_get_client_path(): invalid client id");
    return NULL;
}

DR_API const char *
dr_get_application_name(void)
{
#ifdef LINUX
    return get_application_name();
#else
    return get_application_short_unqualified_name();
#endif
}

DR_API process_id_t
dr_get_process_id(void)
{
    return (process_id_t) get_process_id();
}

#ifdef WINDOWS
DR_API
bool
dr_is_wow64(void)
{
    return is_wow64_process(NT_CURRENT_PROCESS);
}
#endif

DR_API
/* Retrieves the current time */
void
dr_get_time(dr_time_t *time)
{
#ifdef LINUX
    CLIENT_ASSERT(false, "dr_get_time NYI on linux");
#else
    SYSTEMTIME st;
    query_system_time(&st);
    time->year = st.wYear;
    time->month = st.wMonth;
    time->day_of_week = st.wDayOfWeek;
    time->day = st.wDay;
    time->hour = st.wHour;
    time->minute = st.wMinute;
    time->second = st.wSecond;
    time->milliseconds = st.wMilliseconds;
#endif 
}

DR_API 
/* Allocates memory from DR's memory pool specific to the
 * thread associated with drcontext.
 */
void *
dr_thread_alloc(void *drcontext, size_t size)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    return heap_alloc(dcontext, size HEAPACCT(ACCT_IR));
}

DR_API 
/* Frees thread-specific memory allocated by dr_thread_alloc.
 * size must be the same size passed to dr_thread_alloc.
 */
void
dr_thread_free(void *drcontext, void *mem, size_t size)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_thread_free: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_thread_free: drcontext is invalid");
    heap_free(dcontext, mem, size HEAPACCT(ACCT_IR));
}

DR_API 
/* Allocates memory from DR's global memory pool.
 */
void *
dr_global_alloc(size_t size)
{
    return global_heap_alloc(size HEAPACCT(ACCT_OTHER));
}

DR_API 
/* Frees memory allocated by dr_global_alloc.
 * size must be the same size passed to dr_global_alloc.
 */
void
dr_global_free(void *mem, size_t size)
{
    global_heap_free(mem, size HEAPACCT(ACCT_OTHER));
}

DR_API 
/* PR 352427: API routine to allocate executable memory */
void *
dr_nonheap_alloc(size_t size, uint prot)
{
    return heap_mmap_ex(size, size, prot, false/*no guard pages*/);
}

DR_API 
void
dr_nonheap_free(void *mem, size_t size)
{
    heap_munmap_ex(mem, size, false/*no guard pages*/);
}

#ifdef LINUX
DR_API
/* With ld's -wrap option, we can supply a replacement for malloc.
 * This routine allocates memory from DR's global memory pool.  Unlike
 * dr_global_alloc(), however, we store the size of the allocation in
 * the first few bytes so __wrap_free() can retrieve it.
 */
void *
__wrap_malloc(size_t size)
{
    void *mem;
    ASSERT(sizeof(size_t) >= HEAP_ALIGNMENT);
    size += sizeof(size_t);
    mem = global_heap_alloc(size HEAPACCT(ACCT_OTHER));
    if (mem == NULL) {
        CLIENT_ASSERT(false, "malloc failed: out of memory");
        return NULL;
    }
    *((size_t *)mem) = size;
    return mem + sizeof(size_t);
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
    void *buf = NULL;
    if (size > 0) {
        buf = __wrap_malloc(size);
        if (buf != NULL) {
            size_t old_size = *((size_t *)(mem - sizeof(size_t)));
            size_t min_size = MIN(old_size, size);
            memcpy(buf, mem, min_size);
        }
    }
    __wrap_free(mem);
    return buf;
}

DR_API
/* With ld's -wrap option, we can supply a replacement for free.  This
 * routine frees memory allocated by __wrap_alloc and expects the
 * allocation size to be available in the few bytes before 'mem'.
 */
void
__wrap_free(void *mem)
{
    /* PR 200203: must_not_be_inlined() is assuming this routine calls
     * no other DR routines besides global_heap_free!
     */
    if (mem != NULL) {
        mem -= sizeof(size_t);
        global_heap_free(mem, *((size_t *)mem) HEAPACCT(ACCT_OTHER));
    }
}
#endif

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
    if (!dynamo_vm_area_overlap(base, ((byte *)base) + size)) {
        uint mod_prot = new_prot;
        uint res = app_memory_protection_change(get_thread_private_dcontext(),
                                                base, size, new_prot, &mod_prot, NULL);
        if (res != DO_APP_MEM_PROT_CHANGE) {
            if (res == FAIL_APP_MEM_PROT_CHANGE ||
                res == PRETEND_APP_MEM_PROT_CHANGE) {
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
/* checks to see that all bytes with addresses from pc to pc+size-1 
 * are readable and that reading from there won't generate an exception.
 */
bool
dr_memory_is_readable(const byte *pc, size_t size)
{
    return is_readable_without_exception(pc, size);
}

DR_API
/* OS neutral memory query for clients, just wrapper around our get_memroy_info().
 * FIXME - do something about executable areas we made non-writable - see PR 198873 */
bool
dr_query_memory(const byte *pc, byte **base_pc, size_t *size, uint *prot)
{
#ifdef LINUX
    /* xref PR 246897 - the cached all memory list won't include the client lib
     * mappings and appears to be inaccurate at times. For now we use the from
     * os version instead (even though it's slower). FIXME */
    return get_memory_info_from_os(pc, base_pc, size, prot);
#else 
    return get_memory_info(pc, base_pc, size, prot);
#endif
}

#ifdef WINDOWS
DR_API
/* Calls NtQueryVirtualMemory.
 * FIXME - do something about executable areas we made non-writable - see PR 198873 */
size_t
dr_virtual_query(const byte *pc, MEMORY_BASIC_INFORMATION *mbi, size_t mbi_size)
{
    return query_virtual_memory(pc, mbi, mbi_size);
}
#endif

DR_API
/* Wrapper around our safe_read. Xref P4 198875, placeholder till we have try/except.
 * FIXME - the Linux version isn't actually safe - see PR 208562. */
bool
dr_safe_read(const void *base, size_t size, void *out_buf, size_t *bytes_read)
{
    return safe_read_ex(base, size, out_buf, bytes_read);
}

DR_API
/* Wrapper around our safe_write. Xref P4 198875, placeholder till we have try/except.
 * FIXME - the Linux version isn't actually safe - see PR 208562. */
bool
dr_safe_write(void *base, size_t size, const void *in_buf, size_t *bytes_written)
{
    return safe_write_ex(base, size, in_buf, bytes_written);
}

DR_API 
/* Initializes a mutex
 */
void *
dr_mutex_create(void)
{
    void *mutex = (void *)HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, mutex_t, 
                                          ACCT_OTHER, UNPROTECTED);
    ASSIGN_INIT_LOCK_FREE(*((mutex_t *) mutex), dr_client_mutex);
    return mutex;
}

DR_API
/* Deletes mutex
 */
void
dr_mutex_destroy(void *mutex)
{
    /* Delete mutex so locks_not_closed()==0 test in dynamo.c passes */
    DELETE_LOCK(*((mutex_t *) mutex));
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, (mutex_t *)mutex, mutex_t, ACCT_OTHER, UNPROTECTED);
}

DR_API 
/* Locks mutex
 */
void
dr_mutex_lock(void *mutex)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    /* set client_grab_mutex so that we now to set client_thread_safe_for_synch
     * around the actual wait for the lock */
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_grab_mutex = mutex;
    mutex_lock((mutex_t *) mutex);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_grab_mutex = NULL;
}

DR_API 
/* Unlocks mutex
 */
void
dr_mutex_unlock(void *mutex)
{
    mutex_unlock((mutex_t *) mutex);
}

DR_API 
/* Tries once to grab the lock, returns whether or not successful
 */
bool
dr_mutex_trylock(void *mutex)
{
    return mutex_trylock((mutex_t *) mutex);
}

DR_API
/* Looks up the module data containing pc.  Returns NULL if not found.
 * Returned module_data_t must be freed with dr_free_module_data(). */
module_data_t *
dr_lookup_module(byte *pc)
{
    module_area_t *area;
    module_data_t *client_data;
    os_get_module_info_lock();
    area = module_pc_lookup(pc);
    client_data = copy_module_area_to_module_data(area);
    os_get_module_info_unlock();
    return client_data;
}

DR_API
/* Looks up the module with name matching name (ignoring case).  Returns NULL if not
 * found. Returned module_data_t must be freed with dr_free_module_data(). */
module_data_t *
dr_lookup_module_by_name(const char *name)
{
    /* We have no quick way of doing this since our module list is indexed by pc. We
     * could use get_module_handle() but that's dangerous to call at arbitrary times,
     * so we just walk our full list here. */
    module_iterator_t *mi = module_iterator_start();
    CLIENT_ASSERT((name != NULL), "dr_lookup_module_info_by_name: null name");
    while (module_iterator_hasnext(mi)) {
        module_area_t *area = module_iterator_next(mi);
        module_data_t *client_data;
        if (strcasecmp(GET_MODULE_NAME(&area->names), name) == 0) {
            client_data = copy_module_area_to_module_data(area);
            module_iterator_stop(mi);
            return client_data;
        }
    }
    module_iterator_stop(mi);
    return NULL;
}

typedef struct _client_mod_iterator_list_t {
    module_data_t *info;
    struct _client_mod_iterator_list_t *next;
} client_mod_iterator_list_t;

typedef struct {
    client_mod_iterator_list_t *current;
    client_mod_iterator_list_t *full_list;
} client_mod_iterator_t;

DR_API
/* Initialize a new client module iterator. */
dr_module_iterator_t
dr_module_iterator_start(void)
{
    client_mod_iterator_t *client_iterator = (client_mod_iterator_t *)
        HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, client_mod_iterator_t, ACCT_OTHER, UNPROTECTED);
    module_iterator_t *dr_iterator = module_iterator_start();

    memset(client_iterator, 0, sizeof(*client_iterator)); 
    while (module_iterator_hasnext(dr_iterator)) {
        module_area_t *area = module_iterator_next(dr_iterator);
        client_mod_iterator_list_t *list = (client_mod_iterator_list_t *)
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, client_mod_iterator_list_t, ACCT_OTHER,
                            UNPROTECTED);

        ASSERT(area != NULL);
        list->info = copy_module_area_to_module_data(area);
        list->next = NULL;
        if (client_iterator->current == NULL) {
            client_iterator->current = list;
            client_iterator->full_list = client_iterator->current;
        } else {
            client_iterator->current->next = list;
            client_iterator->current = client_iterator->current->next;
        }
    }
    module_iterator_stop(dr_iterator);
    client_iterator->current = client_iterator->full_list;

    return (dr_module_iterator_t)client_iterator;
}

DR_API
/* Returns true if there is another loaded module in the iterator. */
bool
dr_module_iterator_hasnext(dr_module_iterator_t *mi)
{
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_hasnext: null iterator");
    return ((client_mod_iterator_t *)mi)->current != NULL;
}

DR_API
/* Retrieves the module_data_t for the next loaded module in the iterator. */
module_data_t *
dr_module_iterator_next(dr_module_iterator_t *mi)
{
    module_data_t *data;
    client_mod_iterator_t *ci = (client_mod_iterator_t *)mi;
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_next: null iterator");
    CLIENT_ASSERT((ci->current != NULL), "dr_module_iterator_next: has no next, use "
                  "dr_module_iterator_hasnext() first");
    if (ci->current == NULL)
        return NULL;
    data = ci->current->info;
    ci->current = ci->current->next;
    return data;
}

DR_API 
/* Free the module iterator. */
void
dr_module_iterator_stop(dr_module_iterator_t *mi)
{
    client_mod_iterator_t *ci = (client_mod_iterator_t *)mi;
    CLIENT_ASSERT((mi != NULL), "dr_module_iterator_stop: null iterator");

    /* free module_data_t's we didn't give to the client */
    while (ci->current != NULL) {
        dr_free_module_data(ci->current->info);
        ci->current = ci->current->next;
    }

    ci->current = ci->full_list;
    while (ci->current != NULL) {
        client_mod_iterator_list_t *next = ci->current->next;
        HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci->current, client_mod_iterator_list_t,
                       ACCT_OTHER, UNPROTECTED);
        ci->current = next;
    }
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, ci, client_mod_iterator_t, ACCT_OTHER, UNPROTECTED);
}

DR_API
/* Get the name dr uses for this module. */
const char *
dr_module_preferred_name(const module_data_t *data)
{
    if (data == NULL)
        return NULL;

    return GET_MODULE_NAME(&data->names);
}

#ifdef WINDOWS

DR_API
/* If pc is within a section of module lib returns true and (optionally) a copy of
 * the IMAGE_SECTION_HEADER in section_out.  If pc is not within a section of the
 * module mod return false. */
bool
dr_lookup_module_section(module_handle_t lib, byte *pc, IMAGE_SECTION_HEADER *section_out)
{
    CLIENT_ASSERT((lib != NULL), "dr_lookup_module_section: null module_handle_t");
    return module_pc_section_lookup((app_pc)lib, pc, section_out);
}

DR_API
/* Returns the entry point of the function with the given name in the module
 * with the given handle. */
generic_func_t
dr_get_proc_address(module_handle_t lib, const char *name)
{
    return get_proc_address(lib, name);
}

#endif

DR_API
/* Creates a new directory.  Fails if the directory already exists
 * or if it can't be created.
 */
bool
dr_create_dir(const char *fname)
{
    return os_create_dir(fname, CREATE_DIR_REQUIRE_NEW);
}

#ifdef WINDOWS
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
#endif

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

    if (TEST(DR_FILE_READ, mode_flags))
        flags |= OS_OPEN_READ;

    if (TEST(DR_FILE_ALLOW_LARGE, mode_flags))
        flags |= OS_OPEN_ALLOW_LARGE;

    CLIENT_ASSERT((flags != 0), "dr_open_file: no mode selected"); 
    return os_open(fname, flags);
}

DR_API 
/* Closes file f
 */
void
dr_close_file(file_t f)
{
    os_close(f);
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
 * returns true if succesful */
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
void
dr_log(void *drcontext, uint mask, uint level, const char *fmt, ...)
{
#ifdef DEBUG
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    va_list ap;
    if (stats != NULL &&
        ((stats->logmask & mask) == 0 ||
         stats->loglevel < level))
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
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
      case DR_RCO_STACK_VIOLATION:
          sec_violation = STACK_EXECUTION_VIOLATION;
          break;
      case DR_RCO_HEAP_VIOLATION:
          sec_violation = HEAP_EXECUTION_VIOLATION;
          break;
      case DR_RCT_RETURN_VIOLATION:
          sec_violation = RETURN_TARGET_VIOLATION;
          break;
      case DR_RCT_INDIRECT_CALL_VIOLATION:
          sec_violation = INDIRECT_CALL_RCT_VIOLATION;
          break;
      case DR_RCT_INDIRECT_JUMP_VIOLATION:
          sec_violation = INDIRECT_JUMP_RCT_VIOLATION;
          break;
      default:
          CLIENT_ASSERT(false, "dr_write_forensics_report does not support "
                        "DR_UNKNOWN_VIOLATION or invalid violation types");
          return;
    }

    switch (action) {
      case DR_VIOLATION_ACTION_KILL_PROCESS:
          sec_action = ACTION_TERMINATE_PROCESS;
          break;
      case DR_VIOLATION_ACTION_CONTINUE:
      case DR_VIOLATION_ACTION_CONTINUE_CHANGED_CONTEXT:
          sec_action = ACTION_CONTINUE;
          break;
      case DR_VIOLATION_ACTION_KILL_THREAD:
          sec_action = ACTION_TERMINATE_THREAD;
          break;
      case DR_VIOLATION_ACTION_THROW_EXCEPTION:
          sec_action = ACTION_THROW_EXCEPTION;
          break;
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
    dcontext_t *dcontext = get_thread_private_dcontext();
    char msg[MAX_LOG_LENGTH];
    wchar_t wmsg[MAX_LOG_LENGTH];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, BUFFER_SIZE_ELEMENTS(msg), fmt, ap);
    NULL_TERMINATE_BUFFER(msg);
    snwprintf(wmsg, BUFFER_SIZE_ELEMENTS(wmsg), L"%S", msg);
    NULL_TERMINATE_BUFFER(wmsg);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    nt_messagebox(wmsg, L"Notice");
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
    va_end(ap);
}
#endif

DR_API void
dr_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    do_file_write(STDOUT, fmt, ap);
    va_end(ap);
}

DR_API void 
dr_fprintf(file_t f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    do_file_write(f, fmt, ap);
    va_end(ap);
}

DR_API int
/* We have to temporarily suspend our snprintf->_snprintf define */
#undef snprintf
snprintf(char *buf, size_t max, const char *fmt, ...)
#ifdef WINDOWS
# define snprintf _snprintf /* restore the define */
#else
# define snprintf our_snprintf /* restore the define */
#endif
{
    /* we would share code w/ dr_snprintf but no easy way to do that w/ varargs
     * (macro too ugly; export forwarder maybe)
     */
    /* in io.c */
    extern int our_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
    int res;
    va_list ap;
    va_start(ap, fmt);
    res = our_vsnprintf(buf, max, fmt, ap);
    va_end(ap);
    /* Normalize Linux behavior to match Windows */
    if ((size_t)res > max)
        res = -1;
    return res;
}

DR_API int
dr_snprintf(char *buf, size_t max, const char *fmt, ...)
{
    /* in io.c */
    extern int our_vsnprintf(char *s, size_t max, const char *fmt, va_list ap);
    int res;
    va_list ap;
    va_start(ap, fmt);
    /* PR 219380: we use our_vsnprintf instead of ntdll._vsnprintf b/c the
     * latter does not support floating point (while ours does not support
     * wide chars: but we also forward _snprintf to ntdll for clients).
     */
    res = our_vsnprintf(buf, max, fmt, ap);
    va_end(ap);
    /* Normalize Linux behavior to match Windows */
    if ((size_t)res > max)
        res = -1;
    return res;
}

DR_API void 
dr_print_instr(void *drcontext, file_t f, instr_t *instr, const char *msg)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_print_instr: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_print_instr: drcontext is invalid");
    dr_fprintf(f, "%s "PFX" ", msg, instr_get_translation(instr));
    instr_disassemble(dcontext, instr, f);
    dr_fprintf(f, "\n");
}

DR_API void 
dr_print_opnd(void *drcontext, file_t f, opnd_t opnd, const char *msg)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_print_opnd: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
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
    return (void *) dcontext;
}

DR_API thread_id_t
dr_get_thread_id(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_get_thread_id: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_get_thread_id: drcontext is invalid");
    return dcontext->owning_thread;
}

DR_API void *
dr_get_tls_field(void *drcontext)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_get_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_get_tls_field: drcontext is invalid");
    return dcontext->client_data->user_field;
}

DR_API void 
dr_set_tls_field(void *drcontext, void *value)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_set_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_set_tls_field: drcontext is invalid");
    dcontext->client_data->user_field = value;
}

DR_API
bool
dr_raw_tls_calloc(OUT reg_id_t *segment_register,
                  OUT uint *offset,
                  IN  uint num_slots,
                  IN  uint alignment)
{
    CLIENT_ASSERT(segment_register != NULL,
                  "dr_raw_tls_calloc: segment_register cannot be NULL");
    CLIENT_ASSERT(offset != NULL,
                  "dr_raw_tls_calloc: offset cannot be NULL");
    *segment_register = SEG_TLS;
    return os_tls_calloc(offset, num_slots, alignment);
}

DR_API
bool
dr_raw_tls_cfree(uint offset, uint num_slots)
{
    return os_tls_cfree(offset, num_slots);
}

DR_API
/* Current thread gives up its time quantum. */
void
dr_thread_yield(void)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    thread_yield();
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
}

/* for now tied to sideline feature PR 222812, not implemented on Linux */
# if defined(CLIENT_SIDELINE) && defined(WINDOWS)
DR_API
/* Current thread sleeps for time_ms milliseconds. */
void
dr_sleep(int time_ms)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    LARGE_INTEGER liDueTime;
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = true;
    /* FIXME - add an os agnostic os_sleep() at some point when we need it on Linux */
    liDueTime.QuadPart= - time_in_milliseconds * TIMER_UNITS_PER_MILLISECOND;
    nt_sleep(&liDueTime);
    if (IS_CLIENT_THREAD(dcontext))
        dcontext->client_data->client_thread_safe_for_synch = false;
}
# endif

#endif /* CLIENT_INTERFACE */

DR_API
/* Inserts inst as a non-application instruction into ilist prior to "where" */
void
instrlist_meta_preinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_ok_to_mangle(inst, false);
    instrlist_preinsert(ilist, where, inst);
}

DR_API
/* Inserts inst as a non-application instruction into ilist after "where" */
void
instrlist_meta_postinsert(instrlist_t *ilist, instr_t *where, instr_t *inst)
{
    instr_set_ok_to_mangle(inst, false);
    instrlist_postinsert(ilist, where, inst);
}

DR_API
/* Inserts inst as a non-application instruction onto the end of ilist */
void
instrlist_meta_append(instrlist_t *ilist, instr_t *inst)
{
    instr_set_ok_to_mangle(inst, false);
    instrlist_append(ilist, inst);
}

/* dr_insert_* are used by general DR */

/* Inserts a complete call to callee with the passed-in arguments */
void
dr_insert_call(void *drcontext, instrlist_t *ilist, instr_t *where,
               void *callee, uint num_args, ...)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    va_list ap;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_call: drcontext cannot be NULL");
    /* we don't check for GLOBAL_DCONTEXT since DR internally calls this */
    va_start(ap, num_args);
    insert_meta_call_vargs(dcontext, ilist, where, false/*not clean*/,
                           callee, num_args, ap);
    va_end(ap);
}

/* Inserts a complete call to callee with the passed-in arguments, wrapped
 * by an app save and restore.
 * If "save_fpstate" is true, saves the fp/mmx/sse state.
 *
 * NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_prepare_for_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched.
 */
void
dr_insert_clean_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                     void *callee, bool save_fpstate, uint num_args, ...)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    uint dstack_offs, pad = 0;
    size_t buf_sz = 0;
    va_list ap;
    CLIENT_ASSERT(drcontext != NULL, "dr_insert_clean_call: drcontext cannot be NULL");
    /* we don't check for GLOBAL_DCONTEXT since DR internally calls this */
    va_start(ap, num_args);
    dstack_offs = dr_prepare_for_call(dcontext, ilist, where);
#ifdef X64
    /* PR 218790: we assume that dr_prepare_for_call() leaves stack 16-byte
     * aligned, which is what insert_meta_call_vargs requires. */
    CLIENT_ASSERT(ALIGNED(dstack_offs, 16), "internal error: bad stack alignment");
#endif
    if (save_fpstate) {
        /* save on the stack: xref PR 202669 on clients using more stack */
        buf_sz = proc_fpstate_save_size();
        /* we need 16-byte-alignment */
        pad = ALIGN_FORWARD_UINT(dstack_offs, 16) - dstack_offs;
        IF_X64(CLIENT_ASSERT(CHECK_TRUNCATE_TYPE_int(buf_sz + pad),
                             "dr_insert_clean_call: internal truncation error"));
        MINSERT(ilist, where, INSTR_CREATE_sub(dcontext, opnd_create_reg(REG_XSP),
                                               OPND_CREATE_INT32((int)(buf_sz + pad))));
        dr_insert_save_fpstate(drcontext, ilist, where,
                               opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0,
                                                     OPSZ_512));
    }

    /* PR 302951: restore state if clean call args reference app memory.
     * We use a hack here: this is the only instance where we mark as our-mangling
     * but do not have a translation target set, which indicates to the restore
     * routines that this is a clean call.  If the client adds instrs in the middle
     * translation will fail; if the client modifies any instr, the our-mangling
     * flag will disappear and translation will fail.
     */
    instrlist_set_our_mangling(ilist, true);
    insert_meta_call_vargs(dcontext, ilist, where, true/*clean*/, callee, num_args, ap);
    instrlist_set_our_mangling(ilist, false);

    if (save_fpstate) {
        dr_insert_restore_fpstate(drcontext, ilist, where,
                                  opnd_create_base_disp(REG_XSP, REG_NULL, 0, 0,
                                                        OPSZ_512));
        MINSERT(ilist, where, INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                                               OPND_CREATE_INT32(buf_sz + pad)));
    }
    dr_cleanup_after_call(dcontext, ilist, where, 0);
    va_end(ap);
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    instr_t *in = (where == NULL) ? instrlist_last(ilist) : instr_get_prev(where);
    uint dstack_offs;
    CLIENT_ASSERT(drcontext != NULL, "dr_prepare_for_call: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_prepare_for_call: drcontext is invalid");
    dstack_offs = prepare_for_clean_call(dcontext, ilist, where);
    /* now go through and mark inserted instrs as meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != where) {
        instr_set_ok_to_mangle(in, false);
        in = instr_get_next(in);
    }
    return dstack_offs;
}

DR_API void
dr_cleanup_after_call(void *drcontext, instrlist_t *ilist, instr_t *where,
                       uint sizeof_param_area)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    instr_t *in = (where == NULL) ? instrlist_last(ilist) : instr_get_prev(where);
    CLIENT_ASSERT(drcontext != NULL, "dr_cleanup_after_call: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_cleanup_after_call: drcontext is invalid");
    if (sizeof_param_area > 0) {
        /* clean up the parameter area */
        CLIENT_ASSERT(sizeof_param_area <= 127,
                      "dr_cleanup_after_call: sizeof_param_area must be <= 127");
        /* mark it meta down below */
        instrlist_preinsert(ilist, where,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_XSP),
                             OPND_CREATE_INT8(sizeof_param_area)));
    }
    cleanup_after_clean_call(dcontext, ilist, where);
    /* now go through and mark inserted instrs as meta */
    if (in == NULL)
        in = instrlist_first(ilist);
    else
        in = instr_get_next(in);
    while (in != where) {
        instr_set_ok_to_mangle(in, false);
        in = instr_get_next(in);
    }
}

#ifdef CLIENT_INTERFACE
DR_API void
dr_swap_to_clean_stack(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_swap_to_clean_stack: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_swap_to_clean_stack: drcontext is invalid");

    /* PR 219620: For thread-shared, we need to get the dcontext
     * dynamically rather than use the constant passed in here.
     */
    if (SHARED_FRAGMENTS_ENABLED()) {
        MINSERT(ilist, where, instr_create_save_to_tls
                (dcontext, REG_XAX, TLS_XAX_SLOT));
        insert_get_mcontext_base(dcontext, ilist, where, REG_XAX);
        /* save app xsp, and then bring in dstack to xsp */
        MINSERT(ilist, where, instr_create_save_to_dc_via_reg
                (dcontext, REG_XAX, REG_XSP, XSP_OFFSET));
        /* DSTACK_OFFSET isn't within the upcontext so if it's separate this won't
         * work right.  FIXME - the dcontext accessing routines are a mess of shared
         * vs. no shared support, separate context vs. no separate context support etc. */
        ASSERT_NOT_IMPLEMENTED(!TEST(SELFPROT_DCONTEXT, dynamo_options.protect_mask));
        MINSERT(ilist, where, instr_create_restore_from_dc_via_reg
                (dcontext, REG_XAX, REG_XSP, DSTACK_OFFSET));
        MINSERT(ilist, where, instr_create_restore_from_tls
                (dcontext, REG_XAX, TLS_XAX_SLOT));
    }
    else {
        MINSERT(ilist, where, instr_create_save_to_dcontext
                (dcontext, REG_XSP, XSP_OFFSET));
        MINSERT(ilist, where, instr_create_restore_dynamo_stack(dcontext));
    }
}

DR_API void
dr_restore_app_stack(void *drcontext, instrlist_t *ilist, instr_t *where)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_app_stack: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_app_stack: drcontext is invalid");
    /* restore stack */
    if (SHARED_FRAGMENTS_ENABLED()) {
        /* use the register we're about to clobber as scratch space */        
        insert_get_mcontext_base(dcontext, ilist, where, REG_XSP);
        MINSERT(ilist, where, instr_create_restore_from_dc_via_reg
                (dcontext, REG_XSP, REG_XSP, XSP_OFFSET));
    } else {
        MINSERT(ilist, where, instr_create_restore_from_dcontext
                (dcontext, REG_XSP, XSP_OFFSET));
    }
}

#define SPILL_SLOT_TLS_MAX 2
#define NUM_TLS_SPILL_SLOTS (SPILL_SLOT_TLS_MAX + 1)
#define NUM_SPILL_SLOTS (SPILL_SLOT_MAX + 1)
/* The three tls slots we make available to clients.  We reserve TLS_XAX_SLOT for our
 * own use in dr convenience routines. Note the +1 is because the max is an array index
 * (so zero based) while array size is number of slots.  We don't need to +1 in 
 * SPILL_SLOT_MC_REG because subtracting SPILL_SLOT_TLS_MAX already accounts for it. */
static const ushort SPILL_SLOT_TLS_OFFS[NUM_TLS_SPILL_SLOTS] =
    { TLS_XDX_SLOT, TLS_XCX_SLOT, TLS_XBX_SLOT };
/* The dcontext reg slots we make available to clients.  We reserve XAX and XSP for
 * our own use in dr convenience routines. */
static const reg_id_t SPILL_SLOT_MC_REG[NUM_SPILL_SLOTS - NUM_TLS_SPILL_SLOTS] = { 
#ifdef X64
    REG_R15, REG_R14, REG_R13, REG_R12, REG_R11, REG_R10, REG_R9, REG_R8,
#endif
    REG_XDI, REG_XSI, REG_XBP, REG_XDX, REG_XCX, REG_XBX };

DR_API void 
dr_save_reg(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg,
            dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_save_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_save_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_save_reg: invalid spill slot selection");

    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_save_reg requires pointer-sized gpr");

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        MINSERT(ilist, where,
                INSTR_CREATE_mov_st(dcontext, opnd_create_tls_slot(offs),
                                    opnd_create_reg(reg)));
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        if (SHARED_FRAGMENTS_ENABLED()) {
            /* PR 219620: For thread-shared, we need to get the dcontext
             * dynamically rather than use the constant passed in here.
             */
            reg_id_t tmp = (reg == REG_XAX) ? REG_XBX : REG_XAX;
            
            MINSERT(ilist, where, instr_create_save_to_tls
                    (dcontext, tmp, TLS_XAX_SLOT));

            insert_get_mcontext_base(dcontext, ilist, where, tmp);

            MINSERT(ilist, where, instr_create_save_to_dc_via_reg
                    (dcontext, tmp, reg, offs));

            MINSERT(ilist, where, instr_create_restore_from_tls
                    (dcontext, tmp, TLS_XAX_SLOT));
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_restore_reg: invalid spill slot selection");

    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "dr_restore_reg requires a pointer-sized gpr");

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        MINSERT(ilist, where,
                INSTR_CREATE_mov_ld(dcontext, opnd_create_reg(reg),
                                    opnd_create_tls_slot(offs)));
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        if (SHARED_FRAGMENTS_ENABLED()) {
            /* PR 219620: For thread-shared, we need to get the dcontext
             * dynamically rather than use the constant passed in here.
             */
            /* use the register we're about to clobber as scratch space */
            insert_get_mcontext_base(dcontext, ilist, where, reg);

            MINSERT(ilist, where, instr_create_restore_from_dc_via_reg
                    (dcontext, reg, reg, offs));
        } else {
            MINSERT(ilist, where,
                    instr_create_restore_from_dcontext(dcontext, reg, offs));
        }
    }
}

DR_API dr_spill_slot_t
dr_max_opnd_accessible_spill_slot()
{
    if (SHARED_FRAGMENTS_ENABLED())
        return SPILL_SLOT_TLS_MAX;
    else
        return SPILL_SLOT_MAX;
}

/* creates an opnd to access spill slot slot, slot must be <=
 * dr_max_opnd_accessible_spill_slot() */
opnd_t
dr_reg_spill_slot_opnd(void *drcontext, dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_reg_spill_slot_opnd: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_reg_spill_slot_opnd: drcontext is invalid");
    CLIENT_ASSERT(slot <= dr_max_opnd_accessible_spill_slot(),
                  "dr_reg_spill_slot_opnd: slot must be less than "
                  "dr_max_opnd_accessible_spill_slot()");

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = os_tls_offset(SPILL_SLOT_TLS_OFFS[slot]);
        return opnd_create_tls_slot(offs);
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        int offs = opnd_get_reg_dcontext_offs(reg_slot);
        ASSERT(!SHARED_FRAGMENTS_ENABLED()); /* client assert above should catch */
        return opnd_create_dcontext_field(dcontext, offs);
    }
}

DR_API
/* used to read a saved register spill slot from a clean call or a restore_state_event */
reg_t
dr_read_saved_reg(void *drcontext, dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_read_saved_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_read_saved_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_read_saved_reg: invalid spill slot selection");

    /* FIXME - should we allow clients to read other threads saved registers? It's not
     * as dangerous as write, but I can't think of a usage scenario where you'd want to
     * Seems more likely to be a bug.  */
    CLIENT_ASSERT(dcontext == get_thread_private_dcontext(),
                  "dr_read_saved_reg(): drcontext does not belong to current thread");
    
    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = SPILL_SLOT_TLS_OFFS[slot];
        return *(reg_t *)(((byte *)&dcontext->local_state->spill_space) + offs);
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        return reg_get_value(reg_slot, get_mcontext(dcontext));
    }
}

DR_API
/* used to write a saved register spill slot from a clean call */
void
dr_write_saved_reg(void *drcontext, dr_spill_slot_t slot, reg_t value)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_write_saved_reg: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_write_saved_reg: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_write_saved_reg: invalid spill slot selection");

    /* FIXME - should we allow clients to write to other threads saved registers?
     * I can't think of a usage scenario where that would be correct, seems much more
     * likely to be a difficult to diagnose bug that crashes the app or dr.  */
    CLIENT_ASSERT(dcontext == get_thread_private_dcontext(),
                  "dr_write_saved_reg(): drcontext does not belong to current thread");

    if (slot <= SPILL_SLOT_TLS_MAX) {
        ushort offs = SPILL_SLOT_TLS_OFFS[slot];
        *(reg_t *)(((byte *)&dcontext->local_state->spill_space) + offs) = value;
    } else {
        reg_id_t reg_slot = SPILL_SLOT_MC_REG[slot - NUM_TLS_SPILL_SLOTS];
        reg_set_value(reg_slot, get_mcontext(dcontext), value);
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_read_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    if (SHARED_FRAGMENTS_ENABLED()) {
        /* For thread-shared, since reg must be general-purpose we can
         * use it as a base pointer (repeatedly).  Plus it's already dead.
         */
        MINSERT(ilist, where, instr_create_restore_from_tls
                (dcontext, reg, TLS_DCONTEXT_SLOT));
        MINSERT(ilist, where, instr_create_restore_from_dc_via_reg
                (dcontext, reg, reg, CLIENT_DATA_OFFSET));
        MINSERT(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(reg),
                 OPND_CREATE_MEMPTR(reg, offsetof(client_data_t, user_field))));
    } else {
        MINSERT(ilist, where, INSTR_CREATE_mov_ld
                (dcontext, opnd_create_reg(reg),
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_write_tls_field: drcontext cannot be NULL");
    CLIENT_ASSERT(reg_is_pointer_sized(reg),
                  "must use a pointer-sized general-purpose register");
    if (SHARED_FRAGMENTS_ENABLED()) {
        reg_id_t spill = REG_XAX;
        if (reg == spill) /* don't need sub-reg test b/c we know it's pointer-sized */
            spill = REG_XDI;
        MINSERT(ilist, where, instr_create_save_to_tls(dcontext, spill, TLS_XAX_SLOT));
        MINSERT(ilist, where, instr_create_restore_from_tls
                (dcontext, spill, TLS_DCONTEXT_SLOT));
        MINSERT(ilist, where, instr_create_restore_from_dc_via_reg
                (dcontext, spill, spill, CLIENT_DATA_OFFSET));
        MINSERT(ilist, where, INSTR_CREATE_mov_st
                (dcontext, OPND_CREATE_MEMPTR(spill,
                                              offsetof(client_data_t, user_field)),
                 opnd_create_reg(reg)));
        MINSERT(ilist, where,
                instr_create_restore_from_tls(dcontext, spill, TLS_XAX_SLOT));
    } else {
        MINSERT(ilist, where, INSTR_CREATE_mov_st
                (dcontext, OPND_CREATE_ABSMEM
                 (&dcontext->client_data->user_field, OPSZ_PTR),
                 opnd_create_reg(reg)));
    }
}

DR_API void 
dr_save_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                    dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_save_arith_flags: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_save_arith_flags: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_save_arith_flags: invalid spill slot selection");

    /* flag saving code:
     *   save eax
     *   lahf
     *   seto al
     */
    dr_save_reg(drcontext, ilist, where, REG_XAX, slot);
    MINSERT(ilist, where, INSTR_CREATE_lahf(dcontext));
    MINSERT(ilist, where,
            INSTR_CREATE_setcc(dcontext, OP_seto, opnd_create_reg(REG_AL)));
}

DR_API void 
dr_restore_arith_flags(void *drcontext, instrlist_t *ilist, instr_t *where,
                       dr_spill_slot_t slot)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    CLIENT_ASSERT(drcontext != NULL, "dr_restore_arith_flags: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_restore_arith_flags: drcontext is invalid");
    CLIENT_ASSERT(slot <= SPILL_SLOT_MAX,
                  "dr_restore_arith_flags: invalid spill slot selection");

    /* flag restoring code:
     *   add 0x7f,%al
     *   sahf
     *   restore eax
     */
    /* do an add such that OF will be set only if seto set
     * the MSB of saveto to 1
     */
    MINSERT(ilist, where,
            INSTR_CREATE_add(dcontext, opnd_create_reg(REG_AL), OPND_CREATE_INT8(0x7f)));
    MINSERT(ilist, where, INSTR_CREATE_sahf(dcontext));
    dr_restore_reg(drcontext, ilist, where, REG_XAX, slot);
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
    address = (ptr_uint_t) instr_get_translation(instr);
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
        target = (ptr_uint_t) opnd_get_pc(instr_get_target(instr));
    }
    else if (opnd_is_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        target = (ptr_uint_t) instr_get_translation(tgt);
        CLIENT_ASSERT(target != 0,
                      "dr_insert_{ubr,call}_instrumentation: unknown target");
        if (opnd_is_far_instr(instr_get_target(instr))) {
            /* FIXME: handle far instr */
            CLIENT_ASSERT(false,
                          "dr_insert_{ubr,call}_instrumentation: far instr not supported");
        }
    } else {
        CLIENT_ASSERT(false, "dr_insert_{ubr,call}_instrumentation: unknown target");
        target = 0;
    }

    dr_insert_clean_call(drcontext, ilist, instr, callee, false/*no fpstate*/, 2,
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    ptr_uint_t address = (ptr_uint_t) instr_get_translation(instr);
    opnd_t tls_opnd;
    instr_t *newinst;

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
    newinst = INSTR_CREATE_mov_st(dcontext, tls_opnd, opnd_create_reg(REG_XCX));

    /* PR 214962: ensure we'll properly translate the de-ref of app
     * memory by marking the spill and de-ref as INSTR_OUR_MANGLING.
     */
    instr_set_our_mangling(newinst, true);
    MINSERT(ilist, instr, newinst);            

    if (instr_is_return(instr)) {
        /* the retaddr operand is always the final source for all OP_ret* instrs */
        opnd_t retaddr = instr_get_src(instr, instr_num_srcs(instr) - 1);
        opnd_size_t sz = opnd_get_size(retaddr);
        /* even for far ret and iret, retaddr is at TOS */
        newinst = instr_create_1dst_1src(dcontext, sz == OPSZ_2 ? OP_movzx : OP_mov_ld,
                                         opnd_create_reg(REG_XCX), retaddr);
    } else {
        /* call* or jmp* */
        opnd_t src = instr_get_src(instr, 0);
        opnd_size_t sz = opnd_get_size(src);
        reg_id_t reg_target = REG_XCX;
        /* if a far cti, we can't fit it into a register: asserted above.
         * in release build we'll get just the address here.
         */
        if (instr_is_far_cti(instr)) {
            if (sz == OPSZ_10) {
                sz = OPSZ_8;
                reg_target = REG_RCX;
            } else if (sz == OPSZ_6) {
                sz = OPSZ_4;
                reg_target = REG_ECX;
            } else /* target has OPSZ_4 */ {
                sz = OPSZ_2;
                reg_target = REG_XCX; /* we use movzx below */
            }
            opnd_set_size(&src, sz);
        }
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
            INSTR_CREATE_xchg(dcontext, tls_opnd, opnd_create_reg(REG_XCX)));

    dr_insert_clean_call(drcontext, ilist, instr, callee, false/*no fpstate*/, 2,
                         /* address of mbr is 1st param */
                         OPND_CREATE_INTPTR(address),
                         /* indirect target (in tls, xchg-d from ecx) is 2nd param */
                         tls_opnd);
}

/* NOTE : this routine clobbers TLS_XAX_SLOT and the XSP mcontext slot via
 * dr_insert_clean_call(). We guarantee to clients that all other slots
 * (except the XAX mcontext slot) will remain untouched. */
DR_API void
dr_insert_cbr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee)
{
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    ptr_uint_t address, target;
    int opc;
    instr_t *app_flags_ok;
    CLIENT_ASSERT(drcontext != NULL,
                  "dr_insert_cbr_instrumentation: drcontext cannot be NULL");
    address = (ptr_uint_t) instr_get_translation(instr);
    CLIENT_ASSERT(address != 0,
                  "dr_insert_cbr_instrumentation: can't determine app address");
    CLIENT_ASSERT(instr_is_cbr(instr),
                  "dr_insert_cbr_instrumentation must be applied to a cbr");
    CLIENT_ASSERT(opnd_is_near_pc(instr_get_target(instr)) ||
                  opnd_is_near_instr(instr_get_target(instr)),
                  "dr_insert_cbr_instrumentation: target opnd must be a near pc or "
                  "near instr");
    if (opnd_is_near_pc(instr_get_target(instr)))
        target = (ptr_uint_t) opnd_get_pc(instr_get_target(instr));
    else if (opnd_is_near_instr(instr_get_target(instr))) {
        instr_t *tgt = opnd_get_instr(instr_get_target(instr));
        target = (ptr_uint_t) instr_get_translation(tgt);
        CLIENT_ASSERT(target != 0, "dr_insert_cbr_instrumentation: unknown target");
    } else {
        CLIENT_ASSERT(false, "dr_insert_cbr_instrumentation: unknown target");
        target = 0;
    }

    app_flags_ok = instr_get_prev(instr);
    dr_insert_clean_call(drcontext, ilist, instr, callee, false/*no fpstate*/, 3,
                         /* push address of mbr onto stack as 1st parameter */
                         OPND_CREATE_INTPTR(address),
                         /* target is 2nd parameter */
                         OPND_CREATE_INTPTR(target),
                         /* branch direction (put in ebx below) is 3rd parameter */
                         opnd_create_reg(REG_XBX));

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
    if (app_flags_ok == NULL)
        app_flags_ok = instrlist_first(ilist);
    while (!instr_opcode_valid(app_flags_ok) ||
           instr_get_opcode(app_flags_ok) != OP_popf) {
        app_flags_ok = instr_get_next(app_flags_ok);
        CLIENT_ASSERT(app_flags_ok != NULL, 
                      "dr_insert_cbr_instrumentation: cannot find eflags save");
    }
    /* put our code before the popf */

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
        instr_t *branch = instr_clone(dcontext, instr);
        instr_t *not_taken =
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                                 OPND_CREATE_INT32(0));
        instr_t *taken =
            INSTR_CREATE_mov_imm(dcontext, opnd_create_reg(REG_EBX),
                                 OPND_CREATE_INT32(1));
        instr_set_target(branch, opnd_create_instr(taken));
        /* client-added meta instrs should not have translation set */
        instr_set_translation(branch, NULL);
        MINSERT(ilist, app_flags_ok, branch);
        MINSERT(ilist, app_flags_ok, not_taken);
        MINSERT(ilist, app_flags_ok,
                INSTR_CREATE_jmp_short(dcontext, opnd_create_instr(app_flags_ok)));
        MINSERT(ilist, app_flags_ok, taken);
    } else {
        /* build a setcc equivalent of instr's jcc operation
         * WARNING: this relies on order of OP_ enum!
         */
        opc = instr_get_opcode(instr);
        if (opc <= OP_jnle_short)
            opc += (OP_jo - OP_jo_short);
        CLIENT_ASSERT(opc >= OP_jo && opc <= OP_jnle,
                      "dr_insert_cbr_instrumentation: unknown opcode");
        opc = opc - OP_jo + OP_seto;
        MINSERT(ilist, app_flags_ok,
                INSTR_CREATE_setcc(dcontext, opc, opnd_create_reg(REG_BL)));
        /* movzx ebx <- bl */
        MINSERT(ilist, app_flags_ok,
                INSTR_CREATE_movzx(dcontext, opnd_create_reg(REG_EBX),
                                   opnd_create_reg(REG_BL)));
    }

    /* now branch dir is in ebx and will be passed to clean call */
}

DR_API void
dr_insert_ubr_instrumentation(void *drcontext, instrlist_t *ilist, instr_t *instr,
                              void *callee)
{
    /* same as call */
    dr_insert_call_instrumentation(drcontext, ilist, instr, callee);
}

DR_API bool
dr_mcontext_xmm_fields_valid(void)
{
    return preserve_xmm_caller_saved();
}

#endif /* CLIENT_INTERFACE */
/* dr_get_mcontext() needed for translating clean call arg errors */

DR_API void
dr_get_mcontext(void *drcontext, dr_mcontext_t *context, int *app_errno)
{
    char *state;
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)),
                  "DR context protection NYI");
    CLIENT_ASSERT(context != NULL, "invalid context");

#ifdef CLIENT_INTERFACE
    /* PR 207947: support mcontext access from syscall events */
    if (dcontext->client_data->in_pre_syscall ||
        dcontext->client_data->in_post_syscall) {
        *context = *get_mcontext(dcontext);
        if (app_errno != NULL)
            *app_errno = dcontext->app_errno;
        return;
    }
#endif

    /* dr_prepare_for_call() puts the machine context on the dstack
     * with pusha and pushf, but only fills in xmm values for
     * preserve_xmm_caller_saved(): however, we tell the client that the xmm
     * fields are not valid otherwise.  so, we just have to copy the
     * state from the dstack.
     */
    state = (char *)dcontext->dstack - sizeof(dr_mcontext_t);
    *context = *((dr_mcontext_t *)state);
    if (app_errno != NULL) {
        state -= sizeof(int);
        *app_errno = *((int *)state);
    }

    /* esp is a dstack value -- get the app stack's esp from the dcontext */
    context->xsp = get_mcontext(dcontext)->xsp;

    /* FIXME: should we set the pc field? */
}

#ifdef CLIENT_INTERFACE
DR_API void
dr_set_mcontext(void *drcontext, dr_mcontext_t *context, const int *app_errno)
{
    char *state;
    dcontext_t *dcontext = (dcontext_t *)drcontext;
    CLIENT_ASSERT(!TEST(SELFPROT_DCONTEXT, DYNAMO_OPTION(protect_mask)),
                  "DR context protection NYI");
    CLIENT_ASSERT(context != NULL, "invalid context");

    /* PR 207947: support mcontext access from syscall events */
    if (dcontext->client_data->in_pre_syscall ||
        dcontext->client_data->in_post_syscall) {
        *get_mcontext(dcontext) = *context;
        if (app_errno != NULL)
            dcontext->app_errno = *app_errno;
        return;
    }

    /* copy the machine context to the dstack area created with 
     * dr_prepare_for_call().  note that xmm0-5 copied there
     * will override any save_fpstate xmm values, as desired.
     */
    state = (char *)dcontext->dstack - sizeof(dr_mcontext_t);
    *((dr_mcontext_t *)state) = *context;
    if (app_errno != NULL) {
        state -= sizeof(int);
        *((int *)state) = *app_errno;
    }

    /* esp will be restored from a field in the dcontext */
    get_mcontext(dcontext)->xsp = context->xsp;

    /* FIXME: should we support setting the pc field? */
}

DR_API
void
dr_redirect_execution(dr_mcontext_t *mcontext, int app_errno)
{
    dcontext_t *dcontext = get_thread_private_dcontext();

    /* PR 352429: squash current trace.
     * FIXME: will clients use this so much that this will be a perf issue?
     * samples/cbr doesn't hit this even at -trace_threshold 1
     */
    if (is_building_trace(dcontext)) {
        LOG(THREAD, LOG_INTERP, 1, "squashing trace-in-progress\n");
        trace_abort(dcontext);
    }

    dcontext->next_tag = mcontext->pc;
    dcontext->whereami = WHERE_FCACHE;
    set_last_exit(dcontext, (linkstub_t *)get_client_linkstub());
    transfer_to_dispatch(dcontext, app_errno, mcontext);
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
    bool deletable = false;
    CLIENT_ASSERT(!SHARED_FRAGMENTS_ENABLED(),
                  "dr_delete_fragment() only valid with -thread_private");
    CLIENT_ASSERT(drcontext != NULL, "dr_delete_fragment(): drcontext cannot be NULL");
#ifdef CLIENT_SIDELINE
    mutex_lock(&(dcontext->client_data->sideline_mutex));
    fragment_get_fragment_delete_mutex(dcontext);
#else
    CLIENT_ASSERT(drcontext == get_thread_private_dcontext(),
                  "dr_delete_fragment(): drcontext does not belong to current thread");
#endif
    f = fragment_lookup(dcontext, tag);
    if (f != NULL && (f->flags & FRAG_CANNOT_DELETE) == 0) {
        client_todo_list_t * todo = HEAP_TYPE_ALLOC(dcontext, client_todo_list_t, ACCT_OTHER, UNPROTECTED);
        client_todo_list_t * iter = dcontext->client_data->to_do;
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
         * inspecting the to_do list in dispatch.
         */
        if ((f->flags & FRAG_LINKED_INCOMING) != 0)
            unlink_fragment_incoming(dcontext, f);
        fragment_remove_from_ibt_tables(dcontext, f, false);
    }
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
    mutex_unlock(&(dcontext->client_data->sideline_mutex));
#endif
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    bool frag_found;
    fragment_t * f;
    CLIENT_ASSERT(!SHARED_FRAGMENTS_ENABLED(),
                  "dr_replace_fragment() only valid with -thread_private");
    CLIENT_ASSERT(drcontext != NULL, "dr_replace_fragment(): drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_replace_fragment: drcontext is invalid");
#ifdef CLIENT_SIDELINE
    mutex_lock(&(dcontext->client_data->sideline_mutex));
    fragment_get_fragment_delete_mutex(dcontext);
#else
    CLIENT_ASSERT(drcontext == get_thread_private_dcontext(),
                  "dr_replace_fragment(): drcontext does not belong to current thread");
#endif
    f = fragment_lookup(dcontext, tag);
    frag_found = (f != NULL);
    if (frag_found) {
        client_todo_list_t * iter = dcontext->client_data->to_do;
        client_todo_list_t * todo = HEAP_TYPE_ALLOC(dcontext, client_todo_list_t, ACCT_OTHER, UNPROTECTED);
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
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
    mutex_unlock(&(dcontext->client_data->sideline_mutex));
#endif
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
void dr_flush_fragments(void *drcontext, void *curr_tag, void *flush_tag)
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

    flush = HEAP_TYPE_ALLOC(dcontext, client_flush_req_t, ACCT_OTHER, UNPROTECTED);
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
    }
    else {
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
 * to be !couldbelinking (xref PR 199115, 227619). Caller must use
 * dr_redirect_execution() to return to the cache. */
bool
dr_flush_region(app_pc start, size_t size)
{
    dcontext_t *dcontext = get_thread_private_dcontext();
    ASSERT(dcontext != NULL);

    /* Flush requires !couldbelinking. FIXME - not all event callbacks to the client are
     * !couldbelinking (see PR 227619) restricting where this routine can be used. */
    CLIENT_ASSERT(!is_couldbelinking(dcontext), "dr_flush_region: called from an event "
                  "callback that doesn't support calling this routine; see header file "
                  "for restrictions.");
    /* Flush requires caller to hold no locks that might block a couldbelinking thread
     * (which includes almost all dr locks).  FIXME - some event callbacks are holding
     * dr locks (see PR 227619) so can't call this routine.  Since we are going to use
     * a synchall flush, holding client locks is disallowed too (could block a thread
     * at an unsafe spot for synch). */
    CLIENT_ASSERT(thread_owns_no_locks(dcontext), "dr_flush_region: caller owns a client "
                  "lock or was called from an event callback that doesn't support "
                  "calling this routine; see header file for restrictions.");
    CLIENT_ASSERT(size != 0, "dr_flush_region: 0 is invalid size for flush");

    /* release build check of requirements, as many as possible at least */
    if (size == 0 || is_couldbelinking(dcontext))
        return false;

    if (!executable_vm_area_executed_from(start, start + size))
        return true;

    flush_fragments_from_region(dcontext, start, size, true/*force synchall*/);

    return true;
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
    ASSERT(dcontext != NULL);

    /* This routine won't work with coarse_units */
    CLIENT_ASSERT(!DYNAMO_OPTION(coarse_units),
                  /* as of now, coarse_units are always disabled with -thread_private. */
                  "dr_unlink_flush_region is not supported with -opt_memory unless -thread_private or -enable_full_api is also specified");

    /* Flush requires !couldbelinking. FIXME - not all event callbacks to the client are
     * !couldbelinking (see PR 227619) restricting where this routine can be used. */
    CLIENT_ASSERT(!is_couldbelinking(dcontext), "dr_flush_region: called from an event "
                  "callback that doesn't support calling this routine, see header file "
                  "for restrictions.");
    /* Flush requires caller to hold no locks that might block a couldbelinking thread
     * (which includes almost all dr locks).  FIXME - some event callbacks are holding
     * dr locks (see PR 227619) so can't call this routine.  FIXME - some event callbacks
     * are couldbelinking (see PR 227619) so can't allow the caller to hold any client
     * locks that could block threads in one of those events (otherwise we don't need
     * to care about client locks) */
    CLIENT_ASSERT(thread_owns_no_locks(dcontext), "dr_flush_region: caller owns a client "
                  "lock or was called from an event callback that doesn't support "
                  "calling this routine, see header file for restrictions.");
    CLIENT_ASSERT(size != 0, "dr_unlink_flush_region: 0 is invalid size for flush");

    /* release build check of requirements, as many as possible at least */
    if (size == 0 || is_couldbelinking(dcontext))
        return false;

    if (!executable_vm_area_executed_from(start, start + size))
        return true;

    flush_fragments_from_region(dcontext, start, size, false/*don't force synchall*/);

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
                      void (*flush_completion_callback) (int flush_id))
{
    client_flush_req_t *flush;

    if (size == 0) {
        CLIENT_ASSERT(false, "dr_delay_flush_region: 0 is invalid size for flush");
        return false;
    }

    /* FIXME - would be nice if we could check the requirements and call
     * dr_unlink_flush_region() here if it's safe. Is difficult to detect non-dr locks
     * that could block a couldbelinking thread though. */

    flush = HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, client_flush_req_t, ACCT_OTHER, UNPROTECTED);
    memset(flush, 0x0, sizeof(client_flush_req_t));
    flush->start = (app_pc)start;
    flush->size = size;
    flush->flush_id = flush_id;
    flush->flush_callback = flush_completion_callback;

    mutex_lock(&client_flush_request_lock);
    flush->next = client_flush_requests;
    client_flush_requests = flush;
    mutex_unlock(&client_flush_request_lock);

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
#ifdef CLIENT_SIDELINE
    fragment_get_fragment_delete_mutex(dcontext);
#endif
    f = fragment_lookup(dcontext, tag);
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
#endif
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    fragment_t *f;
    int size = 0;
    CLIENT_ASSERT(drcontext != NULL, "dr_fragment_size: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_fragment_size: drcontext is invalid");
#ifdef CLIENT_SIDELINE
    /* used to check to see if owning thread, if so don't need lock */
    /* but the check for owning thread more expensive then just getting lock */
    /* to check if owner get_thread_id() == dcontext->owning_thread */
    fragment_get_fragment_delete_mutex(dcontext);
#endif
    f = fragment_lookup(dcontext, tag);
    if (f == NULL)
        size = 0;
    else
        size = f->size;
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
#endif
    return size;
}

DR_API 
/* Retrieves the application PC of a fragment */
app_pc
dr_fragment_app_pc(void *tag)
{
#ifdef WINDOWS
    /* Only the returning (second) jump in a landing pad is interpreted, thus
     * visible to a client.  The first jump is filtered out by
     * must_not_be_elided().  The second jump will always be a 32-bit rel
     * returning after the hook point (i.e., not the interception buffer).
     */
    if (vmvector_overlap(landing_pad_areas, (app_pc)tag, (app_pc)tag + 1)) {
        ASSERT(*((byte *)tag) == JMP_REL32_OPCODE);
        /* End of jump + relative address. */
        tag = ((app_pc)tag + 5) + *(int *)((app_pc)tag + 1);
        ASSERT(!is_in_interception_buffer(tag));
    }

    if (is_in_interception_buffer(tag))
        tag = get_app_pc_from_intercept_pc(tag);
    CLIENT_ASSERT(tag != NULL, "dr_fragment_app_pc shouldn't be NULL");

    if (DYNAMO_OPTION(hide)) {
#endif
        CLIENT_ASSERT(!is_dynamo_address(tag), "dr_fragment_app_pc shouldn't be DR pc");
#ifdef WINDOWS
    } /* without -hide our DllMain routine ends up in the cache (xref PR 223120) */
#endif
    return tag;
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
    dcontext_t *dcontext = (dcontext_t *) drcontext;
    fragment_t *f;
    fragment_t coarse_f;
    bool success = true;
    CLIENT_ASSERT(drcontext != NULL, "dr_mark_trace_head: drcontext cannot be NULL");
    CLIENT_ASSERT(drcontext != GLOBAL_DCONTEXT,
                  "dr_mark_trace_head: drcontext is invalid");
#ifdef CLIENT_SIDELINE
    /* used to check to see if owning thread, if so don't need lock */
    /* but the check for owning thread more expensive then just getting lock */
    /* to check if owner get_thread_id() == dcontext->owning_thread */
    fragment_get_fragment_delete_mutex(dcontext);
#endif
    f = fragment_lookup_fine_and_coarse(dcontext, tag, &coarse_f, NULL);
    if (f == NULL) {
        future_fragment_t *fut;
        /* make the lookup and add atomic */
        SHARED_FLAGS_RECURSIVE_LOCK(FRAG_SHARED, acquire, change_linking_lock);
        fut = fragment_lookup_future(dcontext, tag);
        if (fut == NULL) {
            /* need to create a future fragment */
            fut = fragment_create_and_add_future(dcontext, tag, FRAG_IS_TRACE_HEAD);
        } else {
            /* don't call mark_trace_head, it will try to do some linking */
            fut->flags |= FRAG_IS_TRACE_HEAD;
        }
        SHARED_FLAGS_RECURSIVE_LOCK(FRAG_SHARED, release, change_linking_lock);
#ifndef CLIENT_SIDELINE
        LOG(THREAD, LOG_MONITOR, 2,
            "Client mark trace head : will mark fragment as trace head when built "
            ": address "PFX"\n", tag);
#endif
    } else {
        /* check precluding conditions */
        if (TEST(FRAG_IS_TRACE, f->flags)) {
#ifndef CLIENT_SIDELINE
            LOG(THREAD, LOG_MONITOR, 2,
                "Client mark trace head : not marking as trace head, is already "
                "a trace : address "PFX"\n", tag);
#endif
            success = false;
        } else if (TEST(FRAG_CANNOT_BE_TRACE, f->flags)) {
#ifndef CLIENT_SIDELINE
            LOG(THREAD, LOG_MONITOR, 2,
                "Client mark trace head : not marking as trace head, particular "
                "fragment cannot be trace head : address "PFX"\n", tag);
#endif
            success = false;
        } else if (TEST(FRAG_IS_TRACE_HEAD, f->flags)) {
#ifndef CLIENT_SIDELINE
            LOG(THREAD, LOG_MONITOR, 2,
                "Client mark trace head : fragment already marked as trace head : "
                "address "PFX"\n", tag);
#endif
            success =  true;
        } else {
            /* if reach here is all right to mark as trace head */
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, acquire, change_linking_lock);
            mark_trace_head(dcontext, f, NULL, NULL);
            SHARED_FLAGS_RECURSIVE_LOCK(f->flags, release, change_linking_lock);
#ifndef CLIENT_SIDELINE
            LOG(THREAD, LOG_MONITOR, 3,
                "Client mark trace head : just marked as trace head : address "PFX"\n",
                tag);
#endif
        }
    }
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
#endif
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
#ifdef CLIENT_SIDELINE
    fragment_get_fragment_delete_mutex(dcontext);
#endif
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
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
#endif
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
#ifdef CLIENT_SIDELINE
    fragment_get_fragment_delete_mutex(dcontext);
#endif
    f = fragment_lookup(dcontext, tag);
    if (f != NULL)
        trace = (f->flags & FRAG_IS_TRACE) != 0;
    else
        trace = false;
#ifdef CLIENT_SIDELINE
    fragment_release_fragment_delete_mutex(dcontext);
#endif
    return trace;
}

#ifdef UNSUPPORTED_API
DR_API 
/* All basic blocks created after this routine is called will have a prefix
 * that restores the ecx register.  Exit ctis can be made to target this prefix
 * instead of the normal entry point by using the instr_branch_set_prefix_target()
 * routine.
 * WARNING: this routine should almost always be called during client
 * initialization, since having a mixture of prefixed and non-prefixed basic
 * blocks can lead to trouble.
 */
void
dr_add_prefixes_to_basic_blocks(void)
{
    if (DYNAMO_OPTION(coarse_units)) {
        /* coarse_units doesn't support prefixes in general.
         * the variation by addr prefix according to processor type
         * is also not stored in pcaches.
         */
        CLIENT_ASSERT(false,
                      "dr_add_prefixes_to_basic_blocks() not supported with -opt_memory");
    }
    options_make_writable();
    dynamo_options.bb_prefixes = true;
    options_restore_readonly();
}
#endif /* UNSUPPORTED_API */

#endif /* CLIENT_INTERFACE */

