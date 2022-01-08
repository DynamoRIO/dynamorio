/* **********************************************************
 * Copyright (c) 2012-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2006-2010 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2006-2007 Determina Corp. */

#include "globals.h"
#include "utils.h"
#include "moduledb.h"
#include "module_shared.h"

/* An array of pointers to the various exempt lists indexed by the
 * moduledb_exempt_list_t enum.  We have the ugliness of an extra indirection
 * and dynamic sizing to move the array into the heap and avoid self
 * protection changes. */
static char **exemption_lists = NULL;
#define GET_EXEMPT_LIST(list) (exemption_lists[(list)])

/* protects access to the above lists, we assume it is safe to check for NULL
 * without holding the lock */
DECLARE_CXTSWPROT_VAR(read_write_lock_t moduledb_lock,
                      INIT_READWRITE_LOCK(moduledb_lock));

static char *
moduledb_add_to_exempt_list(char *list, const char *name)
{
    size_t cur_size_less_null = (list == NULL) ? 0 : strlen(list);
    size_t needed_size = cur_size_less_null + ((cur_size_less_null > 0) ? 1 /* ; */ : 0) +
        strlen(name) + 1 /* NULL */;
    char *new_list =
        (char *)global_heap_realloc(list, cur_size_less_null + 1 /* NULL */, needed_size,
                                    sizeof(*list) HEAPACCT(ACCT_OTHER));
    if (cur_size_less_null == 0)
        *new_list = '\0';
    else
        ASSERT(strlen(new_list) == cur_size_less_null);

    if (cur_size_less_null > 0) {
        /* add separator */
        strncat(new_list, ";", needed_size - cur_size_less_null - 1 /* null */);
    }
    strncat(new_list, name, needed_size - strlen(new_list) - 1 /* NULL */);

    return new_list;
}

static char *
moduledb_remove_from_exempt_list(char *list, const char *name)
{
    /* FIXME - does nothing, worry is if enough unique names are added to the
     * list it could use a substantial amt of memory and take a while to walk
     * (though note we won't get duplicate entries in module churn situations)
     * or we could an get an accidental name collision with a later module. */
    return list;
}

static void
moduledb_update_exempt_list(char **list, const char *name, bool add)
{
    LOG(GLOBAL, LOG_MODULEDB, 2, "\tlist before update \"%s\"\n",
        (*list == NULL) ? "" : *list);
    d_r_write_lock(&moduledb_lock);
    if (add && (*list == NULL || !check_filter(*list, name))) {
        *list = moduledb_add_to_exempt_list(*list, name);
    } else if (!add && *list != NULL && check_filter(*list, name)) {
        *list = moduledb_remove_from_exempt_list(*list, name);
    }
    d_r_write_unlock(&moduledb_lock);
    LOG(GLOBAL, LOG_MODULEDB, 2, "\tlist after update \"%s\"\n",
        (*list == NULL) ? "" : *list);
}

static void
moduledb_process_relaxation_flags(uint flags, const char *name, bool add)
{
    ASSERT_NOT_IMPLEMENTED(
        !TESTANY(~(MODULEDB_ALL_SECTIONS_BITS | MODULEDB_RCT_EXEMPT_TO |
                   MODULEDB_REPORT_ON_LOAD | MODULEDB_DLL2HEAP | MODULEDB_DLL2STACK),
                 flags));
    if (TEST(MODULEDB_RCT_EXEMPT_TO, flags)) {
        LOG(GLOBAL, LOG_MODULEDB, 1, "%s module %s to moduledb exempt rct\n",
            add ? "Adding" : "Removing", name);
        moduledb_update_exempt_list(&GET_EXEMPT_LIST(MODULEDB_EXEMPT_RCT), name, add);
    }
    if (TEST(MODULEDB_DLL2HEAP, flags)) {
        LOG(GLOBAL, LOG_MODULEDB, 1, "%s module %s to moduledb exempt dll2heap\n",
            add ? "Adding" : "Removing", name);
        moduledb_update_exempt_list(&GET_EXEMPT_LIST(MODULEDB_EXEMPT_DLL2HEAP), name,
                                    add);
    }
    if (TEST(MODULEDB_DLL2STACK, flags)) {
        LOG(GLOBAL, LOG_MODULEDB, 1, "%s module %s to moduledb exempt dll2stack\n",
            add ? "Adding" : "Removing", name);
        moduledb_update_exempt_list(&GET_EXEMPT_LIST(MODULEDB_EXEMPT_DLL2STACK), name,
                                    add);
    }
    if (TESTANY(MODULEDB_ALL_SECTIONS_BITS, flags)) {
        uint all_sections =
            (flags & MODULEDB_ALL_SECTIONS_BITS) >> MODULEDB_ALL_SECTIONS_SHIFT;
        ASSERT_NOT_IMPLEMENTED(all_sections == SECTION_ALLOW);
        if (all_sections == SECTION_ALLOW) {
            LOG(GLOBAL, LOG_MODULEDB, 1, "%s module %s to moduledb exec if image\n",
                add ? "Adding" : "Removing", name);
            moduledb_update_exempt_list(&GET_EXEMPT_LIST(MODULEDB_EXEMPT_IMAGE), name,
                                        add);
        }
    }
}

/* for DO_THRESHOLD_SAFE statics */
START_DATA_SECTION(FREQ_PROTECTED_SECTION, "w")

void
moduledb_report_exemption(const char *fmt, app_pc addr1, app_pc addr2, const char *name)
{
    ASSERT(exemption_lists != NULL);
    /* FIXME - need a release version of this */
    /* FIXME - should these respect some MODULEDB_REPORT flag? */
    DODEBUG({
        /* FIXME - would be nice to only report unique module names per type. */
        DO_THRESHOLD_SAFE(DYNAMO_OPTION(moduledb_exemptions_report_max),
                          FREQ_PROTECTED_SECTION,
                          {
                              /* < max */
                              SYSLOG_INTERNAL_WARNING(fmt, addr1, addr2, name);
                          },
                          { /* > max -> nothing */ });
    });
}

/* somewhat arbitrary, but more than long enough for current usage */
#define MAX_COMPANY_NAME 256

void
moduledb_process_image(const char *name, app_pc base, bool add)
{
    char company_name[MAX_COMPANY_NAME];
    bool got_company_name;
    bool relax = true;
    ASSERT(exemption_lists != NULL);

    if (!DYNAMO_OPTION(use_moduledb))
        return;

    /* caller has already verified this is a pe module */
    got_company_name =
        get_module_company_name(base, company_name, BUFFER_SIZE_ELEMENTS(company_name));
    if (!got_company_name)
        company_name[0] = '\0';

    if (got_company_name && company_name[0] != '\0' &&
        (!IS_STRING_OPTION_EMPTY(allowlist_company_names_default) ||
         !IS_STRING_OPTION_EMPTY(allowlist_company_names)) &&
        check_list_default_and_append(dynamo_options.allowlist_company_names_default,
                                      dynamo_options.allowlist_company_names,
                                      company_name)) {
        relax = false;
        LOG(GLOBAL, LOG_MODULEDB, 1,
            "Found module \"%s\" from allowlisted company \"%s\"\n",
            name == NULL ? "no-name" : name, company_name);
        /* FIXME - not all of our modules have the Company name
         * field set (drpreinject & liveshields don't), need to avoid
         * relaxing for those. Should add version info and also check
         * nodgemgr and our other tools etc.  xref case 8723 */
    }
    if (relax) {
        if (name == NULL || *name == '\0') {
            if (add) {
                /* FIXME - not able to relax for these nameless dlls, there shouldn't be
                 * too many of these once we also fall back to the version original
                 * filename for modules with no exports and we'll eventually exempt by
                 * area in the modules list instead of by name anyways. */
                /* FIXME - would be nice to use get_module_name to get the filename of
                 * the module at least, but this a bad time w/respect to the loader to
                 * be walking the lists. */
                SYSLOG_INTERNAL_WARNING("Unable to relax for nameless unknown module "
                                        "from \"%s\" @" PFX,
                                        company_name, base);
            }
        } else {
            LOG(GLOBAL, LOG_MODULEDB, 1, "Loaded unknown module %s\n", name);
            /* process the relaxations */
            moduledb_process_relaxation_flags(DYNAMO_OPTION(unknown_module_policy), name,
                                              add);
            /* FIXME - prob. too noisy, on my machine there are
             * usually 5 of these per process, two for logitech mouse hook
             * dlls, 1 for drpreinject and 2 for Norton av. */
            if (add &&
                TEST(MODULEDB_REPORT_ON_LOAD, DYNAMO_OPTION(unknown_module_policy))) {
                /* FIXME - will prob. need a release version of this */
                DODEBUG({
                    DO_THRESHOLD_SAFE(DYNAMO_OPTION(unknown_module_load_report_max),
                                      FREQ_PROTECTED_SECTION,
                                      {
                                          /* < max */
                                          SYSLOG_INTERNAL_INFO(
                                              "Relaxing protections for unknown module "
                                              "%s @" PFX " from \"%s\"",
                                              name, base, company_name);
                                      },
                                      { /* > max -> nothing */ });
                });
            }
        }
    }
}

/* back to normal section */
END_DATA_SECTION()

static const char *const exempt_list_names[MODULEDB_EXEMPT_NUM_LISTS] = { "rct", "image",
                                                                          "dll2heap",
                                                                          "dll2stack" };

void
moduledb_init()
{
    uint exempt_array_size = (MODULEDB_EXEMPT_NUM_LISTS) * sizeof(char *);
    ASSERT(exemption_lists == NULL);
    exemption_lists = (char **)global_heap_alloc(exempt_array_size HEAPACCT(ACCT_OTHER));
    ASSERT(exemption_lists != NULL);
    memset(exemption_lists, 0, exempt_array_size);
    DOCHECK(1, {
        int i;
        for (i = 0; i < MODULEDB_EXEMPT_NUM_LISTS; i++)
            ASSERT(exempt_list_names[i] != NULL);
    });
}

void
moduledb_exit()
{
    int i;
    ASSERT(exemption_lists != NULL);
    DOLOG(1, LOG_MODULEDB, {
        LOG(GLOBAL, LOG_MODULEDB, 1, "Moduledb exit:\n");
        print_moduledb_exempt_lists(GLOBAL);
    });
    for (i = 0; i < MODULEDB_EXEMPT_NUM_LISTS; i++) {
        char *list = GET_EXEMPT_LIST(i);
        if (list != NULL) {
            global_heap_free(list,
                             strlen(list) +
                                 1 /* NULL */
                                 HEAPACCT(ACCT_OTHER));
        }
    }
    global_heap_free(exemption_lists,
                     (MODULEDB_EXEMPT_NUM_LISTS) * sizeof(char *) HEAPACCT(ACCT_OTHER));
    DELETE_READWRITE_LOCK(moduledb_lock);
    if (doing_detach)
        exemption_lists = NULL;
}

bool
moduledb_exempt_list_empty(moduledb_exempt_list_t list)
{
    return GET_EXEMPT_LIST(list) == NULL;
}

/* checks to see if module name is on the moduledb exempt list pointed
 * to by list */
bool
moduledb_check_exempt_list(moduledb_exempt_list_t list, const char *name)
{
    bool found = false;
    ASSERT(exemption_lists != NULL);
    d_r_read_lock(&moduledb_lock);
    if (GET_EXEMPT_LIST(list) != NULL) {
        LOG(GLOBAL, LOG_MODULEDB, 2,
            "Moduledb checking %s exempt list =\"%s\" for module \"%s\"\n",
            exempt_list_names[list],
            GET_EXEMPT_LIST(list) == NULL ? "" : GET_EXEMPT_LIST(list), name);
        found = check_filter(GET_EXEMPT_LIST(list), name);
    }
    d_r_read_unlock(&moduledb_lock);
    return found;
}

void
print_moduledb_exempt_lists(file_t file)
{
    int i;
    ASSERT(exemption_lists != NULL);
    d_r_read_lock(&moduledb_lock);
    for (i = 0; i < MODULEDB_EXEMPT_NUM_LISTS; i++) {
        ASSERT(exempt_list_names[i] != NULL);
        print_file(file, "moduledb %s exemption list =\"%s\"\n", exempt_list_names[i],
                   GET_EXEMPT_LIST(i) == NULL ? "" : GET_EXEMPT_LIST(i));
    }
    d_r_read_unlock(&moduledb_lock);
}

#ifdef PROCESS_CONTROL
/***************************************************************************/
/* Process control */

/* Note: if the process control/lockdown feature increases in size, then
 *       create a separate file; for now let it be here.
 */

/* As of today, either the blocklist or the allowlist can be used, not both,
 * according to the PRD.  In that case there is no need to distinguish between
 * "not matched" and "no hashlist".  However, if we decide that both need to
 * co-exist (Windows sw restriction policies do a mix), then this is needed.
 * The core is designed to be flexible to handle both.
 */
enum {
    /* NOTE: this isn't the same as "empty" hashlist.  This just means that
     * the hashlist registry key doesn't exist.
     */
    PROCESS_CONTROL_NO_HASHLIST,
    PROCESS_CONTROL_HASHLIST_EMPTY,

    /* Hash list is too big to fit in our buffer so no list was obtained, which
     * means we can't use this to make any decision about process control,
     * i.e., don't do any process control.
     */
    PROCESS_CONTROL_LONG_LIST,

    PROCESS_CONTROL_NOT_MATCHED,
    PROCESS_CONTROL_MATCHED
};

/* This macro defines "matched" to either being on the hashlist or the hashlist
 * being empty; empty hashlist is a wildcard match for allow and block list
 * modes, but not for allowlist integrity mode.  Note, this macro is used only
 * for allow and block list modes, not allowlist integrity mode.
 */
#    define IS_PROCESS_CONTROL_MATCHED(x) \
        ((x) == PROCESS_CONTROL_MATCHED || (x) == PROCESS_CONTROL_HASHLIST_EMPTY)

/* Records an event static that the hash list in reg_key is too long.
 * Note: the reg_key here is (char *) as opposed to (wchar_t *) because our
 *       syslogging can only handle regular chars!
 */
static void
process_control_report_long_list(const char *reg_key)
{
    char num_hash_str[6]; /* Won't need more than 5 digits for num hashes! */

    snprintf(num_hash_str, BUFFER_SIZE_ELEMENTS(num_hash_str), "%d",
             DYNAMO_OPTION(pc_num_hashes));

    /* Be safe, in case some one uses more than 99999 hashes! */
    NULL_TERMINATE_BUFFER(num_hash_str);

    SYSLOG(SYSLOG_WARNING, PROC_CTL_HASH_LIST_TOO_LONG, 4, get_application_name(),
           get_application_pid(), reg_key, num_hash_str);
}

/* This routine reads the hash keys from either the app-specific or the anonymous
 * reg_key for the app and checks if md5_hash is on that list of keys.
 * If so it returns true, if not false.
 */
static int
process_control_match(const char *md5_hash,
#    ifdef PARAMS_IN_REGISTRY
                      const IF_WINDOWS_ELSE_NP(wchar_t, char) * reg_key
#    else
                      const char *reg_key
#    endif
)
{
    int ret_val, res;
    char *hash_list;

    /* The default size of the hash list is set to 100 hashes.  It is unlikely
     * that anyone will exceed it even for anonymous hashes.  If they do then
     * process control will be disabled, i.e., no enforcement will be done.
     * Case 9252.  BTW, the +1 is for the delimiting ';'.
     * FIXME: when we read md5 from a file this should go.
     */
    uint list_size = DYNAMO_OPTION(pc_num_hashes) * (MD5_STRING_LENGTH + 1);

    /* Read the hash_list from the registry.  The hash_list is nothing but a
     * semicolon separated string, just like all core option string.
     */
    hash_list = heap_alloc(GLOBAL_DCONTEXT, list_size HEAPACCT(ACCT_OTHER));

    hash_list[0] = '\0'; /* be safe, in case there is no list */
    /* xref case 9119, we want the value from the unqualified key since our usage of
     * this only depends on the exe (not its cmdline). */
    res = get_unqualified_parameter(reg_key, hash_list, list_size);
    hash_list[list_size - 1] = '\0'; /* be safe */

    if (IS_GET_PARAMETER_SUCCESS(res)) {
        if (hash_list[0] == '\0') {
            /* empty hash is a wildcard match only for allow and block list
             * modes, not for allowlist integrity mode. */
            ret_val = PROCESS_CONTROL_HASHLIST_EMPTY;
        } else if (check_filter(hash_list, md5_hash)) { /* hash matched */
            ret_val = PROCESS_CONTROL_MATCHED;
        } else {
            ret_val = PROCESS_CONTROL_NOT_MATCHED;
        }

        /* FIXME: Anonymous hashes should be in a global registry key and app
         * specific hashes in app specific keys.  Though there is the facility
         * to do otherwise, we should restrict them because combinations
         * like app specific anonymous hashes don't make sense.  But how to?
         */
    } else if (res == GET_PARAMETER_BUF_TOO_SMALL) { /* Case 9252. */
        ret_val = PROCESS_CONTROL_LONG_LIST;
    } else {
        ASSERT(res == GET_PARAMETER_FAILURE);
        /* Couldn't read the key, so assuming there is no hashlist. */
        ret_val = PROCESS_CONTROL_NO_HASHLIST;
    }

    heap_free(GLOBAL_DCONTEXT, hash_list, list_size HEAPACCT(ACCT_OTHER));
    return ret_val;
}

/* In the regular allowlist mode, the process will be allowed to run if its
 * executable's hash matches a hash either on the app specific list or the
 * anonymous list, or if any of those lists are empty.
 *
 * In the allowlist integrity mode, the process will be allowed to run if its
 * executable's hash matches a hash on its app specific hashlist or there is no
 * app specific hashlist at all.  The idea is to ascertain that an executable
 * hasn't changed.  If there is no need to track the change, then those exe's
 * won't have a hashlist.
 *      Though the regular allowlist mode can be used to do the same, there are
 * holes in it like the ones below that have to be manually fixed.
 *  1. support for anonymous hashes; would have to be disabled or not used.
 *  2. support exe names without hashes; same resolution as point #1.
 *  3. apps would be killed if there is no hashlist; would have to add empty
 *      global hashlists.
 *
 * Note: xref case 10969 for allowlist integrity mode.
 */
static void
process_control_allowlist(const char *md5_hash)
{
    security_option_t type_handling;
    action_type_t desired_action;

    const char *threat_id = NULL;
    int anonymous = PROCESS_CONTROL_NOT_MATCHED;
    int app_specific =
        process_control_match(md5_hash, PARAM_STR(DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST));

    /* Do the pure allowlist mode check in case both modes were specified
     * accidentally; a matter of precedence.
     */
    if (IS_PROCESS_CONTROL_MODE_ALLOWLIST()) {
        /* Allow the process if md5_hash matched a hash on either the app
         * specific or anonymous hash list.
         */
        if (IS_PROCESS_CONTROL_MATCHED(app_specific))
            return;
        anonymous = process_control_match(
            md5_hash, PARAM_STR(DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST));
        if (IS_PROCESS_CONTROL_MATCHED(anonymous))
            return;

        threat_id = "WHIT.NOMA"; /* WHIT.NOMA = WHITe list NOt MAtched. */

        /* If there was no match on the anonymous list and if it was too
         * long, then we can't decide to kill the process because we didn't
         * search the full list.  Do no harm and ignore process control.  Case 9252.
         */
        if (anonymous == PROCESS_CONTROL_LONG_LIST) {
            process_control_report_long_list(DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST);
            return;
        }
    } else if (IS_PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY()) {
        /* Allow the process only if md5_hash matched a hash on the app
         * specific hash list; this is the integrity tracking part.  If there
         * is no hash list, it means that this process's exe wasn't added to
         * the integrity mode, so let it run.
         */
        if (app_specific == PROCESS_CONTROL_MATCHED ||
            app_specific == PROCESS_CONTROL_NO_HASHLIST)
            return;

        /* There is a bug in the controller if app specific hashlist is empty
         * for the integrity mode.
         */
        ASSERT(app_specific != PROCESS_CONTROL_HASHLIST_EMPTY);

        threat_id = "WHIT.INTG"; /* WHIT.NOMA = WHITe list INTeGrity mode. */
    } else {
        ASSERT_NOT_REACHED();
        return; /* play it safe */
    }

    /* If there was no match on the app specific list and if it was too
     * long, then we can't decide to kill the process because we didn't search
     * the full list.  Do no harm and ignore process control.  Case 9252.
     */
    if (app_specific == PROCESS_CONTROL_LONG_LIST) {
        process_control_report_long_list(DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST);
        return;
    }

    /* At this point, it should either be not-matched or no-hashlist.
     * Note: No hashlist is equivalent to no match; for allowlist, this means
     * kill.
     */
    ASSERT((app_specific == PROCESS_CONTROL_NOT_MATCHED ||
            app_specific == PROCESS_CONTROL_NO_HASHLIST));
    /* Anonymous lists aren't applicable for integrity mode. */
    if (IS_PROCESS_CONTROL_MODE_ALLOWLIST()) {
        ASSERT((anonymous == PROCESS_CONTROL_NOT_MATCHED ||
                anonymous == PROCESS_CONTROL_NO_HASHLIST));
    }

#    ifdef PROGRAM_SHEPHERDING
    /* Process control has its own detect_mode; case 10610. */
    if (DYNAMO_OPTION(pc_detect_mode)) {
        type_handling = OPTION_REPORT;
        desired_action = ACTION_CONTINUE;
    } else {
        type_handling = OPTION_REPORT | OPTION_BLOCK_IGNORE_DETECT;
        desired_action = ACTION_TERMINATE_PROCESS;
    }

    /* All process control violations will be .K.  As the exe name and
     * pid are already in the event, the threat ID has nothing else to
     * convey, hence a constant string is used.
     */
    security_violation_internal(GLOBAL_DCONTEXT, NULL, PROCESS_CONTROL_VIOLATION,
                                type_handling, threat_id, desired_action, NULL);

    /* Can reach here only if process control is in detect mode. */
    ASSERT(DYNAMO_OPTION(pc_detect_mode));
#    endif
}

static void
process_control_blocklist(const char *md5_hash)
{
    security_option_t type_handling;
    action_type_t desired_action;
    int app_specific =
        process_control_match(md5_hash, PARAM_STR(DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST));

    int anonymous =
        process_control_match(md5_hash, PARAM_STR(DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST));

    ASSERT(IS_PROCESS_CONTROL_MODE_BLOCKLIST());

    if (IS_PROCESS_CONTROL_MATCHED(app_specific) ||
        IS_PROCESS_CONTROL_MATCHED(anonymous)) {
#    ifdef PROGRAM_SHEPHERDING
        /* Process control has its own detect_mode; case 10610. */
        if (DYNAMO_OPTION(pc_detect_mode)) {
            type_handling = OPTION_REPORT;
            desired_action = ACTION_CONTINUE;
        } else {
            type_handling = OPTION_REPORT | OPTION_BLOCK_IGNORE_DETECT;
            desired_action = ACTION_TERMINATE_PROCESS;
        }

        /* All process control violations will be .K; for blocklist we kill
         * if the current executable's process is either on the anonymous
         * or on the app specific allowlist.  As the exe name and
         * pid are already in the event, the threat ID will only convey what
         * list was used, anonymous ("ANON.BLAC") or app-specific ("ANON.BLAC").
         */
        security_violation_internal(
            GLOBAL_DCONTEXT, NULL, PROCESS_CONTROL_VIOLATION, type_handling,
            app_specific == PROCESS_CONTROL_MATCHED ? "BLAC.APPS" : "BLAC.ANON",
            desired_action, NULL);

        /* Can reach here only if process control is in detect mode. */
        ASSERT(DYNAMO_OPTION(pc_detect_mode));
#    endif
    } else if (app_specific == PROCESS_CONTROL_LONG_LIST) { /* Case 9252. */
        process_control_report_long_list(DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST);
    } else if (anonymous == PROCESS_CONTROL_LONG_LIST) { /* Case 9252. */
        process_control_report_long_list(DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST);
    } else {
        /* At this point, it should either be not-matched or no-hashlist.
         * Note: No hashlist is equivalent to no match; for blocklist, this
         * means don't kill.
         */
        ASSERT((app_specific == PROCESS_CONTROL_NOT_MATCHED ||
                app_specific == PROCESS_CONTROL_NO_HASHLIST) &&
               (anonymous == PROCESS_CONTROL_NOT_MATCHED ||
                anonymous == PROCESS_CONTROL_NO_HASHLIST));
    }
}

/* This routine does all the process control work.  This work is the same for
 * both the startup scenario and the nudge scenario.  The bool is to identify
 * whether or not we are in the startup phase because only in that phase we
 * dump a process-start-event.
 */
void
process_control(void)
{
    const char *md5_hash = get_application_md5();
    bool is_black, is_white, is_white_intg;

    if (strlen(md5_hash) != MD5_STRING_LENGTH) {
        /* FIXME: what to do if we couldn't get md5 for the current process?
         * today the default is to ignore and keep going, but is that ok?
         * should ev be notified?
         */
        ASSERT_NOT_REACHED();
        return;
    }

    is_black = IS_PROCESS_CONTROL_MODE_BLOCKLIST();
    is_white = IS_PROCESS_CONTROL_MODE_ALLOWLIST();
    is_white_intg = IS_PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY();

    /* Currently only one mode can be used; this is how our product is sold.
     * However, there is nothing preventing us from using all of them (just
     * make each process control mode below a separate if) with a precedence
     * order.  That was how it was originally; however if there is a bug in EV,
     * we would accidentally run multiple modes at once.  So to safeguard
     * against that the if-else construct below is used.
     */
    ASSERT((is_black && !is_white && !is_white_intg) ||
           (!is_black && is_white && !is_white_intg) ||
           (!is_black && !is_white && is_white_intg));

    if (is_black)
        process_control_blocklist(md5_hash);
    else if (is_white || is_white_intg)
        process_control_allowlist(md5_hash);
    else
        ASSERT_NOT_REACHED();
}

void
process_control_init(void)
{
    process_control();
}
/***************************************************************************/
#endif /* PROCESS_CONTROL */
