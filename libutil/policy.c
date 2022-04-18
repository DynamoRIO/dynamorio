/* **********************************************************
 * Copyright (c) 2004-2008 VMware, Inc.  All rights reserved.
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

#include "share.h"
#include "policy.h"
#include "parser.h"
#include "config.h"
#include "processes.h"

#ifndef UNIT_TEST

WCHAR *msg_id_keys[] = {
#    define MSG_FIELD(x) L#    x,
    POLICY_DEF_KEYS
#    undef MSG_FIELD
    L"<invalid message field>"
};

msg_id
get_msgkey_id(WCHAR *msgk)
{
    msg_id id = -1;

    DO_DEBUG(DL_FINEST, printf("trying to ID %S\n", msgk););

    while (++id != MSGKEY_BAD_FIELD)
        if (0 == wcscmp(msg_id_keys[id], msgk))
            break;

    return id;
}

char *
parse_policy_line(char *start, BOOL *done, msg_id *mfield, WCHAR *param, WCHAR *value,
                  SIZE_T maxchars)
{
    char *next = parse_line(start, done, param, value, maxchars);

    if (mfield != NULL)
        *mfield = get_msgkey_id(param);

    return next;
}

#    define MAX_SUPPORTED_ENGINES 16

void
add_to_engines(int *engines, const WCHAR *neweng)
{
    int i;
    for (i = 0; engines[i] != 0; i++)
        ;
    engines[i] = _wtoi(neweng);
}

DWORD
parse_policy(char *policy_definition,
             /* OUT */ ConfigGroup **config,
             /* OUT */ ConfigGroup **options, BOOL validating)
{
    WCHAR namebuf[MAX_PARAM_LEN], valuebuf[MAX_PARAM_LEN];
    WCHAR app_name[MAX_PATH];
    msg_id mfield;
    DWORD res;
    char *polstr = policy_definition;
    ConfigGroup *policy, *app;
    char *modes_file;
    SIZE_T modes_file_size;
    BOOL parsing_done;
    int engines[MAX_SUPPORTED_ENGINES] = { 0 };

    DO_ASSERT(config != NULL);
    DO_ASSERT(options != NULL);

    *config = NULL;
    *options = NULL;

    DO_DEBUG(DL_VERB, printf("policy string received: %s\n ", polstr););

    res = read_config_group(config, L_PRODUCT_NAME, FALSE);
    if (res != ERROR_SUCCESS)
        return res;

    *options = new_config_group(L"options");

    /* alias the pointer for clarity */
    policy = *config;

    policy->should_clear = TRUE;

    /* starting parsing the polstr */
    polstr = parse_policy_line(polstr, &parsing_done, &mfield, namebuf, valuebuf,
                               MAX_PARAM_LEN);

    /* first, load the options at the beginning
     *  (GLOBAL_PROTECT, VERSION, etc) */
    while (!parsing_done && mfield != MSGKEY_BEGIN_BLOCK) {
        if (0 == wcscmp(namebuf, L"ENGINE")) {
            add_to_engines(engines, valuebuf);
        } else {
            set_config_group_parameter(*options, namebuf, valuebuf);
        }
        polstr = parse_policy_line(polstr, &parsing_done, &mfield, namebuf, valuebuf,
                                   MAX_PARAM_LEN);
    }

    /* now do all of the blocks */
    while (!parsing_done) {

        if (mfield != MSGKEY_BEGIN_BLOCK) {
            DO_DEBUG(DL_ERROR, printf("BEGIN_BLOCK not found, instead %S\n", namebuf););
            return ERROR_PARSE_ERROR;
        }

        polstr = parse_policy_line(polstr, &parsing_done, &mfield, namebuf, valuebuf,
                                   MAX_PARAM_LEN);

        if (mfield == MSGKEY_GLOBAL) {
            app = policy;
        } else if (mfield == MSGKEY_APP_NAME) {
            wcsncpy(app_name, valuebuf, MAX_PATH);
            NULL_TERMINATE_BUFFER(app_name);
            app = new_config_group(app_name);
            add_config_group(policy, app);
        } else {
            DO_DEBUG(DL_ERROR, printf("bad appname token: %S\n", namebuf););
            return ERROR_PARSE_ERROR;
        }

        DO_DEBUG(DL_FINEST, printf("'%S' is the app being parsed\n", app->name););

        modes_file = NULL;
        modes_file_size = 0xffffffff;

        for (;;) {

            polstr = parse_policy_line(polstr, &parsing_done, &mfield, namebuf, valuebuf,
                                       MAX_PARAM_LEN);

            if (parsing_done || mfield == MSGKEY_END_BLOCK)
                break;

            if (mfield == MSGKEY_BEGIN_MP_MODES) {
                polstr = next_token(polstr, &modes_file_size);
                modes_file = polstr;
                DO_DEBUG(DL_VERB, printf("mf=%s\n", modes_file););
                polstr = get_message_block_size(polstr, msg_id_keys[MSGKEY_END_MP_MODES],
                                                &modes_file_size);
                if (modes_file_size == 0xffffffff)
                    return ERROR_PARSE_ERROR;

                continue;
            }

            DO_DEBUG(DL_VERB,
                     printf("option setting: %S, %S=%S\n", app_name, namebuf, valuebuf););

            set_config_group_parameter(app, namebuf, valuebuf);

        } // for(;;)

        /* FIXME:
         * strictly speaking, this isn't parsing, and so it's
         *  kind of messy to be doing this here. (eg: notice that
         *  validate_policy at this point will write all of the
         *  modes files!) but it's ok for now..sort of... */
        if (!validating && modes_file != NULL) {
            char backup;
            WCHAR *modes_path;
            BOOL changed;
            WCHAR modes_filename[MAX_PATH];
            int j;

            modes_path = get_config_group_parameter(app, L_DYNAMORIO_VAR_HOT_PATCH_MODES);
            if (modes_path == NULL) {
                DO_DEBUG(DL_ERROR, printf("missing modes file name!\n"););
                return ERROR_PARSE_ERROR;
            }

            backup = modes_file[modes_file_size];
            modes_file[modes_file_size] = L'\0';

            /* need to write modes file for all supported engines */
            for (j = 0; engines[j] != 0; j++) {
                _snwprintf(modes_filename, MAX_PATH, L"%s\\%d\\%S", modes_path,
                           engines[j], HOTP_MODES_FILENAME);
                res = write_file_contents_if_different(modes_filename, modes_file,
                                                       &changed);
            }

            modes_file[modes_file_size] = backup;

            if (res != ERROR_SUCCESS)
                return res;

            /* FIXME: we have the 'changed' info, so we may want to nudge
             *  selectively based on that...but it gets kind of dangerous
             *  so just nudge_all for now. */
        } else {
            /* FIXME: should we delete old modes files? */
        }

        /* and prep the next line */
        polstr = parse_policy_line(polstr, &parsing_done, &mfield, namebuf, valuebuf,
                                   MAX_PARAM_LEN);

    } // while(!parsing_done)

    return ERROR_SUCCESS;
}

/* the amount of time to wait after startup before doing
 * a nudge reset on all processes. */
#    define DEFAULT_RESET_INTERVAL_MS 2 * 60 * 1000

/* timeout for nudge reset operation */
#    define DEFAULT_RESET_TIMEOUT_MS 30 * 1000

/* to mitigate the possibility of bringing the system to a halt, wait
 * 2 seconds between process resets. */
#    define DEFAULT_RESET_DELAY_MS 2 * 1000

DWORD
policy_import(char *policy_definition, BOOL synchronize_system, BOOL *inject_flag,
              DWORD *warning)
{
    ConfigGroup *policy;
    ConfigGroup *options;
    DWORD res;
    WCHAR *global_protect;

    DO_ASSERT(policy_definition != NULL);

    if (warning != NULL)
        *warning = ERROR_SUCCESS;

    res = parse_policy(policy_definition, &policy, &options, FALSE);

    if (res != ERROR_SUCCESS)
        return res;

    res = write_config_group(policy);

    if (res != ERROR_SUCCESS)
        return res;

    /* note global protect is optional */
    global_protect =
        get_config_group_parameter(options, msg_id_keys[MSGKEY_GLOBAL_PROTECT]);

    if (NULL != global_protect) {

        if (_wtoi(global_protect)) {
            if (inject_flag != NULL)
                *inject_flag = TRUE;
            else
                set_autoinjection();
        } else {
            if (inject_flag != NULL) {
                *inject_flag = FALSE;
            } else {
                if (!using_system32_for_preinject(NULL)) {
                    /* NOTE: on NT the appinit value is cached per-boot;
                     *  so instead of clearing it, we just leave our
                     *  preinject in permanently, and only remove it at
                     *  uninstall.  this is ok for global protect = OFF
                     *  since that message will come with an empty list of
                     *  apps, so nothing will be run under dr.
                     * but for slightly saner platforms we prefer to take
                     *  the safe route of clearing out the appinit value. */
                    unset_autoinjection();
                }
            }
            res = detach_all(DETACH_RECOMMENDED_TIMEOUT);
            if (res != ERROR_SUCCESS && warning != NULL)
                *warning = res;
        }
    }

    if (res != ERROR_SUCCESS)
        return res;

    if (synchronize_system) {
        /* FIXME: This is a ugly hack to fix case 9436; reasonable because 4.2
         * release is close and this is the minimal change.  The better fix
         * would be to make the core flag thin_client via drmarker and use it in
         * system_info_cb.  Though that is better, it still isn't great because
         * node manager will still be issuing detach to all that aren't in the
         * config set.  The best would be either to make EV send an incremental
         * policy update rather than a full one or have node manager identify
         * which ones to nudge by maintaining the prior policy update. */
        wchar_t *global_options;
        global_options = get_config_group_parameter(policy, L_DYNAMORIO_VAR_OPTIONS);
        if (global_options == NULL || wcsstr(global_options, L"-thin_client") == NULL) {
            res = detach_all_not_in_config_group(policy, DETACH_RECOMMENDED_TIMEOUT);
        }
        if (res != ERROR_SUCCESS) {
            DO_DEBUG(DL_WARN, printf("error %d doing consistency detach!\n", res););

            if (warning && *warning != ERROR_SUCCESS)
                *warning = res;
        }

        /* FIXME: very inefficient to issue a process control nudge each time
         * there is an generic update; ev should split update messages into
         * regular, process control, hotpatch mode, hotpatch defs, etc and use
         * them only as needed - will prevent needless performance bottlenecks,
         * esp. with hotpatch nudges.  02-Nov-06: Bharath. */
        /* FIXME: reset_threadproc() just seems to do a generic nudge with
         * sleep - parameterize the sleep and check its uses to see if it can
         * be made generic.  02-Nov-06: Bharath. */
        /* FIXME: Also, there are two layers of delaying/sleeping one at
         * reset_threadproc() level and one at the process_walk level for
         * nudges.  Why wasn't one enough?  02-Nov-06e: Bharath */
        /* FIXME: return value from generic_nudge_all and
         * hotp_notify_all_modes_update (and hotp_notify_all_defs_update else
         * where) haven't been checked.  Don't even know what that means for a
         * nudge all.  Is it supposed to indicate that the whole thing failed
         * or just one nudge?  If so what is the intended action.  02-Nov-06:
         * Bharath. */
        /* FIXME: make both nodemanager and core use the generic nudge
         * interface to do hotpatch and detach nudges and do away with the
         * nudge-specific code.  02-Nov-06: Bharath. */
        generic_nudge_all(NUDGE_GENERIC(process_control), NULL, DEFAULT_RESET_TIMEOUT_MS,
                          0);

        /* FIXME: for now we do this at every policy update, but
         *  maybe should be more efficient? */
        res = hotp_notify_all_modes_update(DETACH_RECOMMENDED_TIMEOUT);

        if (res != ERROR_SUCCESS) {

            DO_DEBUG(DL_WARN, printf("Hotpatch nudge failed! %d\n", res););

            if (warning && *warning != ERROR_SUCCESS)
                *warning = res;
        }
    }

    free_config_group(options);
    free_config_group(policy);

    DO_DEBUG(DL_INFO, printf("Processed policy update.\n"););

    return ERROR_SUCCESS;
}

DWORD
clear_policy()
{
    ConfigGroup *config;
    DWORD res;

    /* wipe out the registry by writing an empty policy. */
    res = read_config_group(&config, L_PRODUCT_NAME, FALSE);
    if (res != ERROR_SUCCESS)
        return res;

    config->should_clear = TRUE;
    res = write_config_group(config);
    free_config_group(config);

    return res;
}

/* returns ERROR_SUCCESS unless the policy_buffer is invalid. */
DWORD
validate_policy(char *policy_definition)
{
    ConfigGroup *policy;
    ConfigGroup *options;
    DWORD res;

    DO_ASSERT(policy_definition != NULL);

    res = parse_policy(policy_definition, &policy, &options, TRUE);

    if (res != ERROR_SUCCESS)
        return res;

    free_config_group(policy);
    free_config_group(options);

    return ERROR_SUCCESS;
}

void
append_policy_block(char *policy_buffer, SIZE_T maxchars, SIZE_T *accumlen,
                    ConfigGroup *cfg)
{
    NameValuePairNode *nvpn = NULL;

    msg_append(policy_buffer, maxchars, msg_id_keys[MSGKEY_BEGIN_BLOCK], accumlen);
    msg_append(policy_buffer, maxchars, L_NEWLINE, accumlen);

    if (0 == wcscmp(cfg->name, L_PRODUCT_NAME)) {
        msg_append(policy_buffer, maxchars, msg_id_keys[MSGKEY_GLOBAL], accumlen);
        msg_append(policy_buffer, maxchars, L_NEWLINE, accumlen);
    } else {
        msg_append_nvp(policy_buffer, maxchars, accumlen, msg_id_keys[MSGKEY_APP_NAME],
                       cfg->name);
    }

    for (nvpn = cfg->params; NULL != nvpn; nvpn = nvpn->next) {
        msg_append_nvp(policy_buffer, maxchars, accumlen, nvpn->name, nvpn->value);
    }

    msg_append(policy_buffer, maxchars, msg_id_keys[MSGKEY_END_BLOCK], accumlen);
    msg_append(policy_buffer, maxchars, L_NEWLINE, accumlen);
}

/* FIXME: does not export modes! */
DWORD
policy_export(char *policy_buffer, SIZE_T maxchars, SIZE_T *needed)
{
    ConfigGroup *config, *chld;
    DWORD res;
    SIZE_T accumlen = 0;

    res = read_config_group(&config, L_PRODUCT_NAME, TRUE);
    if (res != ERROR_SUCCESS)
        return res;

    /* NOTE: we don't specify global protect when exporting */

    /* FIXME: hardcoded ID and version */
    msg_append_nvp(policy_buffer, maxchars, &accumlen, L"POLICY_VERSION", L"30000");

    append_policy_block(policy_buffer, maxchars, &accumlen, config);

    for (chld = config->children; chld != NULL; chld = chld->next) {
        append_policy_block(policy_buffer, maxchars, &accumlen, chld);
    }

    /* for the null terminator */
    accumlen++;

    if (needed != NULL)
        *needed = accumlen;

    if (policy_buffer == NULL || maxchars < accumlen)
        return ERROR_MORE_DATA;
    else
        return ERROR_SUCCESS;
}

DWORD
load_policy(WCHAR *filename, BOOL synchronize_system, DWORD *warning)
{
    DWORD res;
    SIZE_T len;
    char *policy;

    DO_ASSERT(filename != NULL);

    res = read_file_contents(filename, NULL, 0, &len);
    DO_ASSERT(res == ERROR_MORE_DATA);

    policy = (char *)malloc(len);
    res = read_file_contents(filename, policy, len, NULL);
    if (res != ERROR_SUCCESS)
        return res;

    res = policy_import(policy, synchronize_system, NULL, warning);

    free(policy);

    return res;
}

DWORD
save_policy(WCHAR *filename)
{
    DWORD res;
    SIZE_T len;
    char *policy;

    res = policy_export(NULL, 0, &len);
    if (res != ERROR_MORE_DATA && res != ERROR_SUCCESS)
        return res;

    policy = (char *)malloc(len);
    policy[0] = '\0';

    res = policy_export(policy, len, NULL);

    if (res == ERROR_SUCCESS) {
        res = write_file_contents(filename, policy, TRUE);
    }

    free(policy);

    return res;
}

#else // ifdef UNIT_TEST

void
test_sample_mfp(ConfigGroup *globals, ConfigGroup *config)
{
    ConfigGroup *chld;

    if (globals != NULL) {
        DO_ASSERT_WSTR_EQ(L"77777",
                          get_config_group_parameter(globals, L"POLICY_VERSION"));
    }

    DO_ASSERT_WSTR_EQ(L"1", get_config_group_parameter(config, L"DYNAMORIO_RUNUNDER"));
    DO_ASSERT_WSTR_EQ(L"", get_config_group_parameter(config, L"DYNAMORIO_OPTIONS"));
    DO_ASSERT(NULL !=
              wcsstr(get_config_group_parameter(config, L"DYNAMORIO_AUTOINJECT"),
                     L"\\lib\\77777\\dynamorio.dll"));

    chld = get_child(L"svchost.exe-bitsgroup", config);
    DO_ASSERT(chld != NULL);

    DO_ASSERT_WSTR_EQ(L"17", get_config_group_parameter(chld, L"DYNAMORIO_RUNUNDER"));
    DO_ASSERT_WSTR_EQ(L"-report_max 0 -kill_thread -kill_thread_max 1000",
                      get_config_group_parameter(chld, L"DYNAMORIO_OPTIONS"));
    DO_ASSERT(NULL !=
              wcsstr(get_config_group_parameter(chld, L"DYNAMORIO_AUTOINJECT"),
                     L"\\lib\\77777\\dynamorio.dll"));
}

int
main()
{
    char *testline = "GLOBAL_PROTECT=1\r\nBEGIN_BLOCK\r\nAPP_NAME=inetinfo."
                     "exe\r\nDYNAMORIO_OPTIONS=\r\nFOO=\\bar.dll\r\n";
    WCHAR *sample = L"sample.mfp";
    SIZE_T len;
    DWORD res;
    char *policy;

    set_debuglevel(DL_INFO);
    set_abortlevel(DL_WARN);

    /* load the sample policy file for testing */
    res = read_file_contents(sample, NULL, 0, &len);
    DO_ASSERT(res == ERROR_MORE_DATA);
    DO_ASSERT(len > 1000);

    policy = (char *)malloc(len);
    res = read_file_contents(sample, policy, len, NULL);
    DO_ASSERT(res == ERROR_SUCCESS);

    /* parse_policy_line tests */
    {
        WCHAR param[MAX_PATH], value[MAX_PATH];
        BOOL done;
        msg_id mfield;
        char *ptr, *line = testline;

        ptr = parse_policy_line(line, &done, &mfield, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT(mfield == MSGKEY_GLOBAL_PROTECT);
        DO_ASSERT_WSTR_EQ(param, L"GLOBAL_PROTECT");
        DO_ASSERT_WSTR_EQ(value, L"1");

        ptr = parse_policy_line(ptr, &done, &mfield, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT(mfield == MSGKEY_BEGIN_BLOCK);
        DO_ASSERT_WSTR_EQ(param, L"BEGIN_BLOCK");
        DO_ASSERT_WSTR_EQ(value, L"");

        ptr = parse_policy_line(ptr, &done, &mfield, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT(mfield == MSGKEY_APP_NAME);
        DO_ASSERT_WSTR_EQ(param, L"APP_NAME");
        DO_ASSERT_WSTR_EQ(value, L"inetinfo.exe");

        ptr = parse_policy_line(ptr, &done, &mfield, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT(mfield == MSGKEY_BAD_FIELD);
        DO_ASSERT_WSTR_EQ(param, L"DYNAMORIO_OPTIONS");
        DO_ASSERT_WSTR_EQ(value, L"");

        ptr = parse_policy_line(ptr, &done, &mfield, param, value, MAX_PATH);
        DO_ASSERT(!done);
        DO_ASSERT(mfield == MSGKEY_BAD_FIELD);
        DO_ASSERT_WSTR_EQ(param, L"FOO");
        DO_ASSERT(NULL != wcsstr(value, L"bar"));
        DO_ASSERT(NULL != wcsstr(value, get_dynamorio_home()));
    }

    /* parse policy tests */
    {
        ConfigGroup *config, *globals;

        res = parse_policy(policy, &config, &globals, FALSE);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT(globals != NULL);
        DO_ASSERT(config != NULL);

        test_sample_mfp(globals, config);

        free_config_group(globals);
        free_config_group(config);
    }

    /* validate policy tests */
    {}

    /* import policy tests */
    {
        DWORD warning;
        ConfigGroup *config;

        warning = ERROR_SUCCESS;
        res = policy_import(policy, FALSE, NULL, &warning);
        DO_ASSERT(res == ERROR_SUCCESS);

        res = read_config_group(&config, L_PRODUCT_NAME, TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        test_sample_mfp(NULL, config);

        free_config_group(config);
    }

    /* export policy tests */
    {
        WCHAR *outfn = L"test.mfp";
        char *outpol;
        DWORD warning;

        warning = ERROR_SUCCESS;
        res = policy_import(policy, FALSE, NULL, &warning);
        DO_ASSERT(res == ERROR_SUCCESS);

        /* load the sample policy file for testing */
        res = policy_export(NULL, 0, &len);
        DO_ASSERT(res == ERROR_MORE_DATA);
        DO_ASSERT(len > 1000);

        outpol = (char *)malloc(len);
        outpol[0] = '\0';
        res = policy_export(outpol, len, NULL);
        DO_ASSERT(res == ERROR_SUCCESS);
        DO_ASSERT(len == 1 + strlen(outpol));

        res = write_file_contents(outfn, outpol, TRUE);
        DO_ASSERT(res == ERROR_SUCCESS);
        // DO_ASSERT(0 == strcmp(policy, outpol));
    }

    /* load/save */
    {
        DWORD warning = ERROR_SUCCESS;
        ConfigGroup *c;

        /* TODO: better */

        CHECKED_OPERATION(clear_policy());
        CHECKED_OPERATION(load_policy(sample, FALSE, &warning));
        DO_ASSERT(warning == ERROR_SUCCESS);

        CHECKED_OPERATION(read_config_group(&c, L_PRODUCT_NAME, TRUE));
        test_sample_mfp(NULL, c);
        free_config_group(c);

        delete_file_rename_in_use(L"test2.mfp");
        CHECKED_OPERATION(save_policy(L"test2.mfp"));
    }

    /* cleanup */
    CHECKED_OPERATION(clear_policy());

    printf("All Test Passed\n");

    return 0;
}

#endif
