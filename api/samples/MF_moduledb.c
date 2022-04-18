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

/* Usage :
 * Should be run in DR_MODE_MEMORY_FIREWALL mode.
 *
 * Expects a single -client_ops option: the full absolute path to a configuration file
 * (for example '-client_ops "C:\MF_moduledb_VIPA\MF_moduledb-sample.config"').
 *
 * The configuration file is specified as a series of table_value_t structures defined
 * below with no line breaks or extra padding.  Each table_value_t structure consists
 * of a module_name padded with spaces to MAXIMUM_PATH in length followed by three
 * 'y' or 'n' letters specifying the allow_to_stack, allow_to_heap, and allow_to_here
 * modes.
 *
 * If allow_to_stack is 'y', execution is allowed to go from the module specified to
 * a violating location on the stack. If allow_to_heap is 'y', execution is allowed to
 * go from the module specified to a violating location on the heap. If allow_to_here is
 * 'y', violating transfers targeting the module are allowed.
 *
 * A sample config file is included: MF_moduledb-sample.config that is set up to work
 * with VIPA_test.exe (also in the sample folder). The VIPA_test.exe program has two
 * buttons, one to generate a stack overflow attack and one to generate a heap
 * overflow attack.  The sample config file MF_moduledb-sample.config is set up to
 * allow the heap attack, but not the stack attack. To demonstrate :
 *
 * Use drdeploy.exe to configure VIPA_test.exe to run under security_api mode with
 * the appropriate options.
 * drdeploy.exe -reg VIPA_test.exe -root <root path> -mode security_api -client <path
 * to MF_moduledb.dll> -ops "-client_ops <path to MF_moduledb-sample.config>"
 *
 * Then run VIPA_test.exe. Clicking on the heap attack button should produce messages
 * that a potential security violation is being allowed.  Clicking on the stack attack
 * button should produce a message that a potential security violation is being blocked
 * by killing the process.
 *
 * Use 'drdeploy.exe -nudge VIPA_text.exe' to nudge the process to re-read the
 * configuration file.
 *
 * The #define VERBOSE and #define VVERBOSE can be adjusted below to generate more
 * verbose logging.
 */

#include "dr_api.h"

#define VERBOSE 0
#define VVERBOSE 0
#define USE_MESSAGEBOX 1

#define NAME "MF_moduledb"

#ifdef SHOW_RESULTS
#    if defined(WINDOWS) && USE_MESSAGEBOX
#        define DISPLAY_FUNC dr_messagebox
#    else /* no messageboxes, use dr_printf() instead */
#        define DISPLAY_FUNC dr_printf
#    endif
#else
#    define DISPLAY_FUNC dummy_func
#endif

#if VERBOSE
#    define VDISPLAY_FUNC DISPLAY_FUNC
#else
#    define VDISPLAY_FUNC dummy_func
#endif
#if VVERBOSE
#    define VVDISPLAY_FUNC DISPLAY_FUNC
#else
#    define VVDISPLAY_FUNC dummy_func
#endif

/* used to avoid displaying a string */
static void
dummy_func(char *fmt, ...)
{
}

typedef struct {
    char module_name[MAXIMUM_PATH];

    /* exempt transfers to violating stack regions from this module */
    char allow_to_stack; /* either 'y' or 'n' */
    /* exempt transfers to violating heap regions from this module */
    char allow_to_heap; /* either 'y' or 'n' */
    /* exempt violating transfers to this module */
    char allow_to_here; /* either 'y' or 'n' */

    /* Additional relaxation options such as allowing violating code origins regions
     * within this module (some .data sections for ex.) could be added here. */
} table_value_t;

struct _table_entry_t;
typedef struct _table_entry_t table_entry_t;

struct _table_entry_t {
    table_value_t value; /* the entry we read out of the config file */
    table_entry_t *next; /* table is just a singly linked list */
};                       /* table_entry_t */

static const char *table_def_file_name; /* name of configuration file */
static void *table_lock;                /* for multithreaded access to the table */
static table_entry_t *table = NULL;     /* table of relaxations */

static void
event_security_violation(void *drcontext, void *source_tag, app_pc source_pc,
                         app_pc target_pc, dr_security_violation_type_t violation,
                         dr_mcontext_t *mcontext, dr_security_violation_action_t *action);
static void
event_nudge(void *drcontext, uint64 argument);
static void
event_about_to_terminate_nudges(int total_non_app_threads, int live_non_app_threads);
static void
event_exit(void);

static table_entry_t *
get_entry_for_address(app_pc addr);
static void
read_table();
static void
free_table();

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_set_client_name("DynamoRIO Sample Client 'MF_moduledb'",
                       "http://dynamorio.org/issues");
    VDISPLAY_FUNC(NAME " initializing.");

    /* register the events we wish to handle */
    dr_register_security_event(event_security_violation);
    dr_register_nudge_event(event_nudge, id);
    dr_register_exit_event(event_exit);

    /* read the client options */
    table_def_file_name = dr_get_options(id);
    if (table_def_file_name == NULL || table_def_file_name[0] == '\0') {
        DISPLAY_FUNC(NAME " requires the table name as parameter\n");
        dr_abort();
    }

    /* initialize structures */
    table_lock = dr_mutex_create();
    read_table();
}

static void
event_exit(void)
{
    VDISPLAY_FUNC(NAME " exiting.");

    /* free structures */
    free_table();
    dr_mutex_destroy(table_lock);
}

static void
event_nudge(void *drcontext, uint64 argument)
{
    DISPLAY_FUNC(NAME " received nudge event; re-reading config file.");

    /* An external process has nudged us with dr_nudge_process() telling us
     * to re-read the configuration file. */
    dr_mutex_lock(table_lock);
    free_table();
    read_table();
    dr_mutex_unlock(table_lock);
}

#ifdef WINDOWS
/* Lookup the module containing addr and see if we have a table entry for it */
static table_entry_t *
get_entry_for_address(app_pc addr)
{
    module_data_t *data = dr_lookup_module(addr);
    table_entry_t *entry = NULL;
    if (data != NULL) {
        entry = table;
        while (entry != NULL &&
               _stricmp(dr_module_preferred_name(data), entry->value.module_name) != 0) {
            entry = entry->next;
        }
        dr_free_module_data(data);
    }
    return entry;
}
#endif

static void
event_security_violation(void *drcontext, void *source_tag, app_pc source_pc,
                         app_pc target_pc, dr_security_violation_type_t violation,
                         dr_mcontext_t *mcontext, dr_security_violation_action_t *action)
{
    /* A potential security violation was detected. */
    char *violation_str = NULL; /* a name for this violation */
    bool allow = false;         /* should we let execution continue */

#ifdef WINDOWS
    /* find the module we came from */
    app_pc source = (source_pc == NULL ? source_tag : source_pc);
    table_entry_t *entry;

    /* check our source relaxations */
    entry = get_entry_for_address(source);
    if (entry != NULL) {
        /* we have a match, check the relaxations */
        if (violation == DR_RCO_STACK_VIOLATION &&
            (entry->value.allow_to_stack == 'y' || entry->value.allow_to_stack == 'Y')) {
            allow = true;
        }
        if (violation == DR_RCO_HEAP_VIOLATION &&
            (entry->value.allow_to_heap == 'y' || entry->value.allow_to_heap == 'Y')) {
            allow = true;
        }
    }

    /* check our target relaxations */
    entry = get_entry_for_address(target_pc);
    if (entry != NULL) {
        /* we have a match, check the relaxations */
        if ((violation == DR_RCT_RETURN_VIOLATION ||
             violation == DR_RCT_INDIRECT_CALL_VIOLATION ||
             violation == DR_RCT_INDIRECT_JUMP_VIOLATION) &&
            (entry->value.allow_to_here == 'y' || entry->value.allow_to_here == 'Y')) {
            allow = true;
        }
    }

#endif

    switch (violation) {
    case DR_RCO_STACK_VIOLATION: violation_str = "stack execution violation"; break;
    case DR_RCO_HEAP_VIOLATION: violation_str = "heap execution violation"; break;
    case DR_RCT_RETURN_VIOLATION: violation_str = "return target violation"; break;
    case DR_RCT_INDIRECT_CALL_VIOLATION: violation_str = "call transfer violation"; break;
    case DR_RCT_INDIRECT_JUMP_VIOLATION: violation_str = "jump transfer violation"; break;
    default: violation_str = "unknown"; break;
    }

    if (allow)
        *action = DR_VIOLATION_ACTION_CONTINUE;

    /* could use dr_write_forensics_report() here to log additional information */

    DISPLAY_FUNC("WARNING - possible security violation \"%s\" detected, %s.",
                 violation_str, allow ? "allowing" : "blocking");
}

static void
read_table()
{
    file_t file;
    bool read_entry = true;

    file = dr_open_file(table_def_file_name, DR_FILE_READ);
    if (file == INVALID_FILE) {
        DISPLAY_FUNC(NAME " error opening config file \"%s\"\n", table_def_file_name);
        return;
    }

    VVDISPLAY_FUNC(NAME " reading config file: \"%s\"\n", table_def_file_name);

    do {
        table_entry_t *entry = (table_entry_t *)dr_global_alloc(sizeof(table_entry_t));
        if (dr_read_file(file, &entry->value, sizeof(table_value_t)) !=
            sizeof(table_value_t)) {
            /* end of file */
            read_entry = false;
            dr_global_free(entry, sizeof(table_entry_t));
        } else {
            int i;
            /* insert NULL termination for module name (including space padding) */
            for (i = sizeof(entry->value.module_name) - 1;
                 i >= 0 && entry->value.module_name[i] == ' '; i--) {
                entry->value.module_name[i] = '\0';
            }
            /* just in case */
            entry->value.module_name[sizeof(entry->value.module_name) - 1] = '\0';

            /* add to the table */
            entry->next = table;
            table = entry;
            VVDISPLAY_FUNC(
                NAME " read entry for module=\"%s\" to_stack=%s to_heap=%s "
                     "transfer_to_here=%s\n",
                entry->value.module_name,
                (entry->value.allow_to_stack == 'y' || entry->value.allow_to_stack == 'Y')
                    ? "yes"
                    : "no",
                (entry->value.allow_to_heap == 'y' || entry->value.allow_to_heap == 'Y')
                    ? "yes"
                    : "no",
                (entry->value.allow_to_here == 'y' || entry->value.allow_to_here == 'Y')
                    ? "yes"
                    : "no");
        }
    } while (read_entry);
    VVDISPLAY_FUNC(NAME " done reading config file.");
}

static void
free_table()
{
    /* Free all table entries */
    table_entry_t *next;
    while (table != NULL) {
        next = table->next;
        dr_global_free(table, sizeof(table_entry_t));
        table = next;
    }
}
