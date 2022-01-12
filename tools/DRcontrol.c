/* **********************************************************
 * Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
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

/*
 * DRcontrol.c
 *
 * command-line tool for changing registry options
 *
 *
 * (c) araksha, inc. all rights reserved
 */
#include <wchar.h>

#include "share.h"
#include "config.h"
#include "processes.h"

#include <stdio.h>

/* convenience macros for secure string buffer operations */
#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0
#define BUFFER_ROOM_LEFT_W(wbuf) (BUFFER_SIZE_ELEMENTS(wbuf) - wcslen(wbuf) - 1)
#define BUFFER_ROOM_LEFT(abuf) (BUFFER_SIZE_ELEMENTS(abuf) - strlen(abuf) - 1)

typedef unsigned char byte;

// helper from config.c, not defined in confg.h
void
dump_config_group(char *, char *, ConfigGroup *, BOOL);

void
usage()
{
    fprintf(stderr,
            "Usage:\nDRControl [-help] [-create path] [-destroy] [-exists] [-reset] \n"
            "\t[-app name] [-add name] [-remove name] [-run N] [-options string] \n"
            "\t[-detach pid] [-detachexe name] [-detachall] [-drlib dll]\n"
            "\t[-preinject {ON|OFF|CLEAR|LIST|REPORT|LOAD_ON|LOAD_OFF|dll}]\n"
            "\t[-create_eventlog path] [-destroy_eventlog] [-logdir path]\n"
            "\t[-sharedcache path]\n"
            "\t[-dump] [-fulldump] [-appdump name] [-load file] [-save file]\n"
            "\t[-drhome path] [-modes path] [-defs path] \n"
            "\t[-v] [-hot_patch_nudge pid] [-hot_patch_modes_nudge pid]\n"
            "\t[-hot_patch_nudge_all] [-hot_patch_modes_nudge_all]\n"
            "\t[-pid pid] [-all] [-timeout ms] [-delay ms]\n"
            "\t[-drpop] [-nudge type] [-client_nudge arg] [-verbose] [-canary_default]\n"
            "\t[-canary <full path of canary.exe> <path to a scratch folder]\n"
            "\t[-canary_run <run_flags>] [-canary_fault <run_flag> <ops>]\n"
            "\t[-32] [-64]\n");
    exit(1);
}

void
help()
{
    fprintf(stderr, "Configuration Options:\n");
    fprintf(stderr,
            " -create path\t\tcreate a registry and log dir setup, using 'path' "
            "as\n\t\t\tDYNAMORIO_HOME (does not change an existing setup)\n");
    fprintf(stderr, " -destroy\t\tdelete entire product registry key\n");
    fprintf(stderr,
            " -exists\t\tdisplays whether or not product reg key exists\n\t\t\t(returns "
            "0 if exists, not 0 otherwises)\n");
    fprintf(stderr, " -reset\t\t\tremove all app-specific keys\n");

    fprintf(stderr, " -app name\t\tset app-specific options\n");
    fprintf(stderr,
            " -add name\t\tadd a new application to the configuration (if not there "
            "already)\n");
    fprintf(stderr, " -remove name\t\tremove the apps from the configuration\n");
    fprintf(stderr, " -run N\t\t\tset global or app-specific RUNUNDER=N\n");
    fprintf(stderr, " -options string\tsets the options string to 'string'\n");
    fprintf(stderr, " -drlib dll\t\tsets the SC library to use; must be a fully\n");
    fprintf(stderr, "\t\t\tqualified pathname\n");

    fprintf(stderr,
            " -preinject {ON|OFF|CLEAR|LIST|REPORT|dll}\n\t\t\tON=set to default, "
            "OFF=remove, CLEAR=wipe out list,\n\t\t\tLIST=display current AppInit_DLLs "
            "setting;,\n\t\t\tREPORT=display current preinject setting, if "
            "any;\n\t\t\tLOAD_OFF=(vista only) turns off loading preinject "
            "library;\n\t\t\tLOAD_ON=(vista ionly) turns on loading preinject "
            "library;\n\t\t\tdll sets the preinject library to use, must be "
            "a\n\t\t\tfully qualified pathname\n");
    fprintf(stderr,
            " -create_eventlog path\t\tinitializes eventlog for " PRODUCT_NAME
            " library at path\n");
    fprintf(stderr, " -destroy_eventlog\t\tfrees our eventlog\n");
    fprintf(stderr, " -drhome path\t\tsets DYNAMORIO_HOME to path\n");
    fprintf(stderr, " -modes path\t\tsets the modes file directory to path\n");
    fprintf(stderr, " -defs path\t\tsets the defs file directory to path\n");
    fprintf(stderr, " -logdir path\t\tsets the logging directory\n");
    fprintf(stderr, " -sharedcache path\t\tsets the shared DLL cache directory\n");
    fprintf(stderr, " -dump\t\t\tdisplays the current configuration\n");
    fprintf(stderr, " -fulldump\t\tsame as -dump (deprecated)\n");
    fprintf(stderr, " -appdump\t\tdisplays the current app configuration\n");

    fprintf(stderr, " -load file\t\tloads configuration from the specified file\n");
    fprintf(stderr, " -save file\t\tsaves current configuration to the specified file\n");
    fprintf(stderr, " -v\t\t\tdisplay version information\n\n");
    fprintf(stderr, "all options/lists/strings should be enclosed in \"'s\n");
    fprintf(stderr, "by 'app', we mean the name of an executable (eg notepad.exe), \n");
    fprintf(stderr, "\tor the name of a svchost group (eg svchost.exe-netsvcs).\n");
    fprintf(stderr, "when not using -app, all settings apply to the globals\n\n");
    fprintf(stderr, " -32\t\tconfigure for 32-bit (WOW64) applications\n");
    fprintf(stderr, " -64\t\tconfigure for 64-bit (non-WOW64) applications\n");

    fprintf(stderr, "Control Options:\n");
    fprintf(stderr, " -detach pid\t\tdetaches from indicated pid\n");
    fprintf(stderr,
            " -detachexe name\tdetaches from all processes with given .exe name\n");
    fprintf(stderr, " -detachall\t\tdetaches from all processes\n");
    fprintf(stderr,
            " -hot_patch_nudge\tforces hot patch defs and modes information to be "
            "re-read for pid\n");
    fprintf(stderr,
            " -hot_patch_modes_nudge\tforces hot patch modes information to be re-read "
            "for pid\n");
    fprintf(stderr,
            " -hot_patch_nudge_all\tforces hot patch defs and modes information to be "
            "re-read for all processes\n");
    fprintf(stderr,
            " -hot_patch_modes_nudge_all\tforces hot patch modes information to be "
            "re-read for all processes\n");
    fprintf(stderr, " -pid pid\t\tpid to be nudged\n");
    fprintf(stderr, " -all or -pid -1\t\tnudge all DR processes\n");
    fprintf(stderr, " -delay ms\t\tdelay between nudges\n");
    fprintf(stderr, " -timeout ms\t\texpected time for nudge completion\n");
    fprintf(stderr, " -nudge type\t\tnudge action, can be repeated\n");
    fprintf(stderr, " -client_nudge arg\t\tsend client nudge with specified hex arg\n");
    /* same as -nudge reset -nudge opt, but NOT the same as -reset which wipes out the
     * registry!  */
    fprintf(stderr, " -drpop\t\t\tcache reset\n");
    fprintf(stderr,
            " -canary_default\trun canary test as PE would using registry implict setup, "
            "returns canary code\n");
    fprintf(stderr,
            " -canary path_canary path_scratch_folder\trun customized canary test, "
            "returns canary code\n");
    fprintf(stderr,
            " -canary_run run_flags\toverride the runs flags for the canary run\n");
    fprintf(stderr,
            " -canary_fault run_flag ops\tinject a fault at canary run run_flag by "
            "setting canry options to ops\n");

    exit(1);
}

void
checked_operation(char *label, DWORD res)
{
    if (res != ERROR_SUCCESS) {
        fprintf(stderr, "Error %d on operation: %s\n", res, label);
        exit(-1);
    }
}

int
main(int argc, char **argv)
{
    int res = 0, run = 0, dump = 0, reset = 0, detachall = 0, detachpid = 0,
        all = 0,  /* applies to all running processes */
        pid = -1, /* applies to pid, -1 means -all */
        hotp_nudge_pid = 0, hotp_modes_nudge_pid = 0, hotp_nudge_all = 0,
        hotp_modes_nudge_all = 0, nudge = 0, /* generic nudge with argument */
        nudge_action_mask = 0,               /* generic nudge action mask */
        delay_ms_all =                       /* delay between acting on processes */
        NUDGE_NO_DELAY,
        timeout_ms = /* timeout for finishing a nudge on a single process */
        DETACH_RECOMMENDED_TIMEOUT,
        runval = 0, canary_default = 0, canary_run = CANARY_RUN_FLAGS_DEFAULT,
        canary_fault_run = 0, exists = 0, destroy = 0, free_eventlog = 0;

    uint64 nudge_client_arg = 0; /* client nudge argument */

    int verbose = 0;

    char *create = NULL, *addapp = NULL, *appdump = NULL, *removeapp = NULL,
         *opstring = NULL, *drdll = NULL, *preinject = NULL, *logdir = NULL,
         *sharedcache = NULL, *appname = NULL, *drhome = NULL, *modes = NULL,
         *defs = NULL, *detach_exename = NULL, *load = NULL, *save = NULL,
         *eventlog = NULL, *canary_process = NULL, *scratch_folder = NULL,
         *canary_fault_ops = NULL;

    dr_platform_t dr_platform = DR_PLATFORM_DEFAULT;

    int argidx = 1;

    WCHAR wbuf[MAX_PATH];
    ConfigGroup *policy = NULL, *working_group;

    if (argc < 2)
        usage();

    while (argidx < argc) {

        if (!strcmp(argv[argidx], "-help")) {
            help();
        }
        /* ******************** actions on active processes ******************** */
        else if (!strcmp(argv[argidx], "-detachall")) {
            detachall = 1;
        } else if (!strcmp(argv[argidx], "-detach")) {
            if (++argidx >= argc)
                usage();
            detachpid = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-detachexe")) {
            if (++argidx >= argc)
                usage();
            detach_exename = argv[argidx];
        } else if (!strcmp(argv[argidx], "-pid") || !strcmp(argv[argidx], "-p")) {
            if (++argidx >= argc)
                usage();
            pid = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-all")) {
            all = 1;
        } else if (!strcmp(argv[argidx], "-delay")) {
            /* in milliseconds */
            if (++argidx >= argc)
                usage();
            delay_ms_all = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-timeout")) {
            /* in milliseconds */
            if (++argidx >= argc)
                usage();
            timeout_ms = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-hot_patch_nudge")) {
            if (++argidx >= argc)
                usage();
            hotp_nudge_pid = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-hot_patch_modes_nudge")) {
            if (++argidx >= argc)
                usage();
            hotp_modes_nudge_pid = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-hot_patch_nudge_all")) {
            hotp_nudge_all = 1;
        } else if (!strcmp(argv[argidx], "-verbose")) {
            verbose = 1;
        } else if (!strcmp(argv[argidx], "-hot_patch_modes_nudge_all")) {
            hotp_modes_nudge_all = 1;
        } else if (!strcmp(argv[argidx], "-drpop")) {
            nudge = 1;
            /* allow composition */
            nudge_action_mask |= NUDGE_GENERIC(opt) | NUDGE_GENERIC(reset);
        } else if (!strcmp(argv[argidx], "-nudge")) {
            int nudge_numeric;
            if (++argidx >= argc)
                usage();
            nudge_numeric = atoi(argv[argidx]); /* 0 if fails */
            nudge_action_mask |= nudge_numeric;

            /* compare against numeric new code, or against symbolic names */
            /* -nudge opt -nudge reset -nudge stats -nudge 30000 */
            {
                int found = 0;
#define NUDGE_DEF(name, comment)                  \
    if (strcmp(#name, argv[argidx]) == 0) {       \
        found = 1;                                \
        nudge_action_mask |= NUDGE_GENERIC(name); \
    }
                NUDGE_DEFINITIONS();
#undef NUDGE_DEF
                if (!found && nudge_numeric == 0) {
                    printf("unknown -nudge %s\n", argv[argidx]);
                    usage();
                }
            }

            nudge = 1;
        } else if (!strcmp(argv[argidx], "-client_nudge")) {
            if (++argidx >= argc)
                usage();
            nudge_client_arg = _strtoui64(argv[argidx], NULL, 16);
            nudge_action_mask |= NUDGE_GENERIC(client);
            nudge = 1;
        }
        /* ******************** configuration actions ******************** */
        else if (!strcmp(argv[argidx], "-reset")) {
            reset = 1;
        } else if (!strcmp(argv[argidx], "-create")) {
            if (++argidx >= argc)
                usage();
            create = argv[argidx];
        } else if (!strcmp(argv[argidx], "-destroy")) {
            destroy = 1;
        } else if (!strcmp(argv[argidx], "-exists")) {
            exists = 1;
        } else if (!strcmp(argv[argidx], "-run")) {
            run = 1;
            if (++argidx >= argc)
                usage();
            runval = atoi(argv[argidx]);
        } else if (!strcmp(argv[argidx], "-app")) {
            if (++argidx >= argc)
                usage();
            appname = argv[argidx];
        } else if (!strcmp(argv[argidx], "-add")) {
            if (++argidx >= argc)
                usage();
            addapp = argv[argidx];
        } else if (!strcmp(argv[argidx], "-remove")) {
            if (++argidx >= argc)
                usage();
            removeapp = argv[argidx];
        } else if (!strcmp(argv[argidx], "-options")) {
            if (++argidx >= argc)
                usage();
            opstring = argv[argidx];
        } else if (!strcmp(argv[argidx], "-drlib")) {
            if (++argidx >= argc)
                usage();
            drdll = argv[argidx];
        } else if (!strcmp(argv[argidx], "-preinject")) {
            if (++argidx >= argc)
                usage();
            preinject = argv[argidx];
        } else if (!strcmp(argv[argidx], "-create_eventlog")) {
            if (++argidx >= argc)
                usage();
            eventlog = argv[argidx];
        } else if (!strcmp(argv[argidx], "-destroy_eventlog")) {
            free_eventlog = 1;
        } else if (!strcmp(argv[argidx], "-drhome")) {
            if (++argidx >= argc)
                usage();
            drhome = argv[argidx];
        } else if (!strcmp(argv[argidx], "-modes")) {
            if (++argidx >= argc)
                usage();
            modes = argv[argidx];
        } else if (!strcmp(argv[argidx], "-defs")) {
            if (++argidx >= argc)
                usage();
            defs = argv[argidx];
        } else if (!strcmp(argv[argidx], "-logdir")) {
            if (++argidx >= argc)
                usage();
            logdir = argv[argidx];
        } else if (!strcmp(argv[argidx], "-sharedcache")) {
            if (++argidx >= argc)
                usage();
            sharedcache = argv[argidx];
        } else if (!strcmp(argv[argidx], "-load")) {
            if (++argidx >= argc)
                usage();
            load = argv[argidx];
        } else if (!strcmp(argv[argidx], "-save")) {
            if (++argidx >= argc)
                usage();
            save = argv[argidx];
        } else if (!strcmp(argv[argidx], "-dump")) {
            dump = 1;
        } else if (!strcmp(argv[argidx], "-appdump")) {
            if (++argidx >= argc)
                usage();
            appdump = argv[argidx];
        } else if (!strcmp(argv[argidx], "-fulldump")) {
            dump = 1;
        } else if (!strcmp(argv[argidx], "-v")) {
#ifdef BUILD_NUMBER
            printf("DRcontrol.exe build %d -- %s", BUILD_NUMBER, __DATE__);
#else
            printf("DRcontrol.exe custom build -- %s, %s", __DATE__, __TIME__);
#endif
        } else if (!strcmp(argv[argidx], "-canary_default")) {
            canary_default = 1;
        } else if (!strcmp(argv[argidx], "-canary")) {
            if (++argidx >= argc)
                usage();
            canary_process = argv[argidx];
            if (++argidx >= argc)
                usage();
            scratch_folder = argv[argidx];
        } else if (!strcmp(argv[argidx], "-canary_run")) {
            if (++argidx >= argc)
                usage();
            canary_run = strtol(argv[argidx], NULL, 0);
        } else if (!strcmp(argv[argidx], "-canary_fault")) {
            char *dummy;
            if (++argidx >= argc)
                usage();
            canary_fault_run = strtol(argv[argidx], &dummy, 0);
            if (++argidx >= argc)
                usage();
            canary_fault_ops = argv[argidx];
        } else if (!strcmp(argv[argidx], "-32")) {
            dr_platform = DR_PLATFORM_32BIT;
        } else if (!strcmp(argv[argidx], "-64")) {
            dr_platform = DR_PLATFORM_64BIT;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage();
        }
        argidx++;
    }

    /* PR 244206: set the registry view before any registry access */
    set_dr_platform(dr_platform);

    if (canary_process != NULL || canary_default != 0) {
        BOOL result = TRUE;
        WCHAR canary_fault_args[MAX_PATH];
        CANARY_INFO info = { 0 };

        info.run_flags = canary_run;
        info.info_flags = CANARY_INFO_FLAGS_DEFAULT;
        info.fault_run = canary_fault_run;
        _snwprintf(canary_fault_args, BUFFER_SIZE_ELEMENTS(canary_fault_args), L"%S",
                   canary_fault_ops);
        NULL_TERMINATE_BUFFER(canary_fault_args);
        info.canary_fault_args = canary_fault_args;

        if (canary_process != NULL && *canary_process != '\0' && scratch_folder != NULL &&
            *scratch_folder != '\0') {
            wchar_t canary[MAX_PATH], scratch[MAX_PATH], out[MAX_PATH];
            FILE *out_file;
            _snwprintf(canary, BUFFER_SIZE_ELEMENTS(canary), L"%S", canary_process);
            NULL_TERMINATE_BUFFER(canary);
            _snwprintf(scratch, BUFFER_SIZE_ELEMENTS(scratch), L"%S\\canary_test",
                       scratch_folder);
            NULL_TERMINATE_BUFFER(scratch);
            CreateDirectory(scratch, NULL);
            _snwprintf(out, BUFFER_SIZE_ELEMENTS(out), L"%S\\canary_report.crep",
                       scratch_folder);
            out_file = _wfopen(out, L"wb");
            /* FIXME - check directory, out_file, and canary proc exist */
            result = run_canary_test_ex(out_file, &info, scratch, canary);
        } else if (canary_default != 0) {
            result = run_canary_test(&info, L_EXPAND_LEVEL(STRINGIFY(BUILD_NUMBER)));
            printf("See report file \"%S\"\n", info.report);
        }
        printf("Canary test - %s enable protection - code 0x%08x\n"
               "  msg=\"%S\"\n  url=\"%S\"\n",
               result ? "do" : "don\'t", info.canary_code, info.msg, info.url);
        return info.canary_code;
    }

    if (exists) {
        if (get_dynamorio_home() != NULL) {
            printf("Registry setup exists\n");
            return 0;
        }
        printf("Registry setup doesn't exist\n");
        return 1;
    }

    if (save) {
        _snwprintf(wbuf, MAX_PATH, L"%S", save);
        NULL_TERMINATE_BUFFER(wbuf);
        checked_operation("save policy", save_policy(wbuf));
    }

    if (destroy) {
        checked_operation("delete product key", destroy_root_key());
        if (!load && create == NULL)
            return 0;
    }

    if (load) {
        _snwprintf(wbuf, MAX_PATH, L"%S", load);
        NULL_TERMINATE_BUFFER(wbuf);
        checked_operation("load policy", load_policy(wbuf, FALSE, NULL));
    }

    if (create != NULL) {
        _snwprintf(wbuf, MAX_PATH, L"%S", create);
        NULL_TERMINATE_BUFFER(wbuf);
        /* FALSE: do not overwrite (preserves old behavior) */
        checked_operation("create registry", setup_installation(wbuf, FALSE));
    }

    /* ensure we init dynamorio_home, case 4009 */
    get_dynamorio_home(); /* ignore return value */

    if (nudge) {
        if (verbose)
            printf("-nudge %d -pid %d %s\n", nudge_action_mask, pid, all ? "all" : "");
        if (pid == -1) /* explicitly set */
            all = 1;

        if (all)
            checked_operation("nudge all",
                              generic_nudge_all(nudge_action_mask, nudge_client_arg,
                                                timeout_ms, delay_ms_all));
        else
            checked_operation("nudge",
                              generic_nudge(pid, TRUE, nudge_action_mask,
                                            0, /* client ID (ignored here) */
                                            nudge_client_arg, timeout_ms));
        goto finished;
    }

    if (detachall) {
        checked_operation("detach all", detach_all(timeout_ms));
        goto finished;
    }
    if (detachpid) {
        checked_operation("detach", detach(detachpid, TRUE, timeout_ms));
        goto finished;
    }

    if (detach_exename) {
        _snwprintf(wbuf, MAX_PATH, L"%S", detach_exename);
        NULL_TERMINATE_BUFFER(wbuf);
        checked_operation("detach-exe", detach_exe(wbuf, timeout_ms));
        goto finished;
    }

    if (hotp_nudge_pid) {
        checked_operation("hot patch update",
                          hotp_notify_defs_update(hotp_nudge_pid, TRUE, timeout_ms));
        goto finished;
    }

    if (hotp_modes_nudge_pid) {
        checked_operation(
            "hot patch modes update",
            hotp_notify_modes_update(hotp_modes_nudge_pid, TRUE, timeout_ms));
        goto finished;
    }

    if (hotp_nudge_all) {
        checked_operation("hot patch nudge all", hotp_notify_all_defs_update(timeout_ms));
        goto finished;
    }

    if (hotp_modes_nudge_all) {
        checked_operation("hot patch modes nudge all",
                          hotp_notify_all_modes_update(timeout_ms));
        goto finished;
    }

    checked_operation("read config", read_config_group(&policy, L_PRODUCT_NAME, TRUE));

    if (reset) {
        remove_children(policy);
        policy->should_clear = TRUE;
        checked_operation("write registry", write_config_group(policy));
    }

    working_group = policy;

    if (dump || appdump)
        goto dumponly;

    if (preinject) {
        if (0 == strcmp(preinject, "OFF")) {
            checked_operation("unset autoinject", unset_autoinjection());
        } else if (0 == strcmp(preinject, "ON")) {
            checked_operation("set autoinject", set_autoinjection());
        } else if (0 == strcmp(preinject, "CLEAR")) {
            checked_operation("clear autoinject",
                              set_autoinjection_ex(FALSE, APPINIT_USE_ALLOWLIST, NULL,
                                                   L"", NULL, NULL, NULL, 0));
        } else if (0 == strcmp(preinject, "LIST")) {
            WCHAR list[MAX_PARAM_LEN];
            checked_operation("read appinit",
                              get_config_parameter(INJECT_ALL_KEY_L, TRUE,
                                                   INJECT_ALL_SUBKEY_L, list,
                                                   MAX_PARAM_LEN));
            printf("%S\n", list);
            if (is_vista()) {
                printf("LoadAppInit is %s\n", is_loadappinit_set() ? "on" : "off");
            }
        } else if (0 == strcmp(preinject, "REPORT")) {
            WCHAR list[MAX_PARAM_LEN], *entry, *sep;
            checked_operation("read appinit",
                              get_config_parameter(INJECT_ALL_KEY_L, TRUE,
                                                   INJECT_ALL_SUBKEY_L, list,
                                                   MAX_PARAM_LEN));
            entry = get_entry_location(list, L_EXPAND_LEVEL(INJECT_DLL_8_3_NAME),
                                       APPINIT_SEPARATOR_CHAR);
            if (NULL != entry) {
                sep = wcschr(entry, APPINIT_SEPARATOR_CHAR);
                if (NULL != sep)
                    *sep = L'\0';
                printf("%S\n", entry);
                if (is_vista()) {
                    printf("LoadAppInit is %s\n", is_loadappinit_set() ? "on" : "off");
                }
            }
        } else if (0 == strcmp(preinject, "LOAD_OFF")) {
            checked_operation("unset load autoinject", unset_loadappinit());
        } else if (0 == strcmp(preinject, "LOAD_ON")) {
            checked_operation("set load autoinject", set_loadappinit());
        } else {
            _snwprintf(wbuf, MAX_PATH, L"%S", preinject);
            NULL_TERMINATE_BUFFER(wbuf);
            checked_operation("set custom autoinject",
                              set_autoinjection_ex(TRUE, APPINIT_OVERWRITE, NULL, NULL,
                                                   NULL, wbuf, NULL, 0));
        }

        if (0 != strcmp(preinject, "LIST") && 0 != strcmp(preinject, "REPORT") &&
            using_system32_for_preinject(NULL)) {
            DWORD platform;
            if (get_platform(&platform) == ERROR_SUCCESS &&
                platform == PLATFORM_WIN_NT_4) {
                fprintf(stderr,
                        "Warning! On NT4, new AppInit_DLLs setting will not take effect "
                        "until reboot!\n");
            }
        }
    }

    if (free_eventlog) {
        checked_operation("free eventlog", destroy_eventlog());
    }

    if (eventlog) {
        _snwprintf(wbuf, BUFFER_SIZE_ELEMENTS(wbuf), L"%S", eventlog);
        NULL_TERMINATE_BUFFER(wbuf);
        checked_operation("create eventlog", create_eventlog(wbuf));
    }

    /* configuration */

    if (addapp) {
        _snwprintf(wbuf, MAX_PATH, L"%S", addapp);
        NULL_TERMINATE_BUFFER(wbuf);
        if (NULL == get_child(wbuf, policy)) {
            add_config_group(policy, new_config_group(wbuf));
        }
    }

    if (removeapp) {
        _snwprintf(wbuf, MAX_PATH, L"%S", removeapp);
        NULL_TERMINATE_BUFFER(wbuf);
        remove_child(wbuf, policy);
        policy->should_clear = TRUE;
    }

    if (appname) {
        _snwprintf(wbuf, MAX_PATH, L"%S", appname);
        NULL_TERMINATE_BUFFER(wbuf);
        working_group = get_child(wbuf, policy);

        if (NULL == working_group) {
            working_group = new_config_group(wbuf);
            add_config_group(policy, working_group);
        }
    }

    if (run) {
        _snwprintf(wbuf, MAX_PATH, L"%d", runval);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_RUNUNDER, wbuf);
    }

    if (opstring) {
        _snwprintf(wbuf, MAX_PATH, L"%S", opstring);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_OPTIONS, wbuf);
    }

    if (drdll) {
        _snwprintf(wbuf, MAX_PATH, L"%S", drdll);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_AUTOINJECT, wbuf);
    }

    if (drhome) {
        _snwprintf(wbuf, MAX_PATH, L"%S", drhome);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_HOME, wbuf);
    }

    if (modes) {
        _snwprintf(wbuf, MAX_PATH, L"%S", modes);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_HOT_PATCH_MODES, wbuf);
    }

    if (defs) {
        _snwprintf(wbuf, MAX_PATH, L"%S", defs);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_HOT_PATCH_POLICIES,
                                   wbuf);
    }

    if (logdir) {
        _snwprintf(wbuf, MAX_PATH, L"%S", logdir);
        NULL_TERMINATE_BUFFER(wbuf);
        set_config_group_parameter(working_group, L_DYNAMORIO_VAR_LOGDIR, wbuf);
    }

    if (sharedcache) {
        /* note if the sharedcache root directory doesn't exist it should be
         * created before calling this function */
        _snwprintf(wbuf, MAX_PATH, L"%S", sharedcache);
        NULL_TERMINATE_BUFFER(wbuf);

        res = setup_cache_shared_directories(wbuf);
        if (res != ERROR_SUCCESS) {
            fprintf(stderr, "error %d creating directories!\n", res);
        }
        setup_cache_shared_registry(wbuf, working_group);
    }

    checked_operation("write policy", write_config_group(policy));

dumponly:

    if (appdump) {
        _snwprintf(wbuf, MAX_PATH, L"%S", appdump);
        NULL_TERMINATE_BUFFER(wbuf);
        working_group = get_child(wbuf, policy);
    } else {
        working_group = policy;
    }

    if (dump || appdump) {
        if (NULL == working_group)
            fprintf(stderr, "No Configuration Exists!\n");
        else
            dump_config_group("", "  ", working_group, FALSE);
    }

finished:
    if (policy != NULL)
        free_config_group(policy);

    return 0;
}
