/* **********************************************************
 * Copyright (c) 2012-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2008 VMware, Inc.  All rights reserved.
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
 * DRview.c
 *
 * command-line tool for determining what is running under DR
 *
 * (c) araksha, inc. all rights reserved
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <tchar.h>

/* The order here now matters: share.h needs to be the first to include
 * globals_shared.h, as it has to set HOT_PATCHING_INTERFACE first;
 * but ntdll.h now includes globals_shared.h, so we put it second.
 * But, processes.h duplicates _VM_COUNTERS from ntdll.h but can handle
 * going 2nd but not first: so it must go last.
 */
#include "share.h"
#include "ntdll.h"
#include "processes.h"

#define NAME "DR"

int
procwalk();
void
dllwalk();

void
usage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr,
            "DRview [-help] [-pid n] [-exe name] [-listdr] [-listall] [-listdlls] "
            "[-showdlls] [-nopid] [-no32] [-out file] [-cmdline] [-showmem] [-showtime] "
            "[-nobuildnum] [-qname strip] [-noqnames] [-hot_patch] [-s n] [-tillidle] "
            "[-idlecpu c] [-showmemfreq f] [-idleafter s] [-v]\n");
    exit(1);
}

void
help()
{
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -pid n\t\t\tdisplays whether the process is injected into\n");
    fprintf(stderr, " -exe name\t\tfinds all processes whose executable matches\n");
    fprintf(stderr, "\t\t\t'name', shows whether injected into\n");
    fprintf(stderr, " -listdr\t\tlist all processes injected into\n");
    fprintf(
        stderr,
        " -listall\t\tlist all processes on the system, show whether injected_into\n");
    fprintf(stderr,
            " -listdlls\t\tlist all DLLs [short] for a specific pid or executable\n");
    fprintf(stderr,
            " -showdlls\t\tlist all DLLs [long] for a specific pid or executable\n");
    fprintf(
        stderr,
        " -nopid\t\t\tdoes not display PIDs of processes (useful for expect files)\n");
#ifdef X64
    fprintf(stderr, " -no32\t\t\tdoes not display whether 32-bit\n");
#endif
    fprintf(stderr, " -onlypid\t\tonly shows PID\n");
    fprintf(stderr, " -out file\t\toutput to file instead of stdout\n");
    fprintf(stderr, " -cmdline\t\tshow process command lines\n");
    fprintf(stderr, " -showmem\t\tshow memory stats\n");
    fprintf(stderr,
            " -showtime\t\tshow scheduled time for each process (needs -showmem)\n");
    fprintf(stderr, " -showstats\t\tshow internal stats\n");
    fprintf(stderr,
            " -nobuildnum\t\tdoes not display build number of SC (useful for expect "
            "files)\n");
    fprintf(stderr, " -qname strip\t\tshow qualified names; set strip to 0 or 1\n");
    fprintf(stderr, " -noqnames\t\tdon\'t show qualified names\n");
    fprintf(stderr, " -hot_patch\t\tshow hot patch status\n");
    fprintf(stderr,
            " -s n\t\t\tsample every n millis (default: 500ms, e.g. 1s=1000, 5min=300000 "
            "5*60*1000)\n");
    fprintf(stderr, " -tillidle\t\tsample until idle (default: < 3%% cpu for 3s)\n");
    fprintf(stderr,
            " -idlecpu c\t\tconsider < c%% cpu utilization idle (default: 3%%)\n");
    /* Sometimes -listall -showmem causes lsass %cpu to remain high, so
     * we provide an option to sample frequently but query lsass less frequently.
     */
    fprintf(stderr,
            " -showmemfreq f\t\tfor -tillidle -showmem, show -showmem every f samples "
            "(default: 1)\n");
    fprintf(stderr,
            " -idleafter s\t\tflag machine is idle after s seconds (default: 3s)\n");
    fprintf(stderr, " -v\t\t\tdisplay version information\n\n");

    exit(1);
}

BOOL listdr = FALSE, listall = FALSE, nopid = FALSE, no32 = FALSE, onlypid = FALSE,
     listdlls = FALSE, showdlls = FALSE, qname = FALSE, strip = FALSE, noqnames = FALSE;
cmdline = FALSE, hotp = FALSE, showtime = FALSE, sampling = FALSE, tillidle = FALSE,
                 idle = FALSE, skip = FALSE, showmem = FALSE, showbuild = TRUE,
                 showstats = FALSE;
uint pid = 0;
uint millis = 0;
uint idlecpu = 0;
uint showmemfreq = 1;
uint flag_after_ms = 0;
char *exe = NULL, *outf = NULL;
process_id_t save_pid = 0;
WCHAR save_module[MAX_PATH];
WCHAR wexe[MAX_PATH];
FILE *fp;
LONGLONG total_user = 0;
LONGLONG total_kernel = 0;

#define MAX_CMDLINE 2048

#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0
#define NULL_TERMINATE_SIZED_BUFFER(buf, size) (buf)[(size)-1] = 0

static void
generate_process_name(process_info_t *pi, WCHAR *name_buf /* OUT */,
                      uint name_buf_length /* elements */)
{
    WCHAR qual_name[MAX_CMDLINE];
    WCHAR qual_args[MAX_CMDLINE];
    int res;
    BOOL use_args = FALSE;

    /* hack: we assume only need qualified names for these hardcoded apps
     * FIXME: read registry to see whether need qualification
     */
    if (wcsicmp(pi->ProcessName, L"svchost.exe") == 0 ||
        wcsicmp(pi->ProcessName, L"msiexec.exe") == 0 ||
        wcsicmp(pi->ProcessName, L"tomcat.exe") == 0 ||
        wcsicmp(pi->ProcessName, L"dllhost.exe") == 0) {
        /* use qualified names */
        res = get_process_cmdline(pi->ProcessID, qual_name,
                                  BUFFER_SIZE_ELEMENTS(qual_name));
        NULL_TERMINATE_BUFFER(qual_name);
        if (res == ERROR_SUCCESS) {
            if (get_commandline_qualifier(
                    qual_name, qual_args, BUFFER_SIZE_ELEMENTS(qual_args),
                    /* hack: we assume we only strip svchost,
                     * and we also strip dllhost here to
                     * fit more of the GUI in and avoid the
                     * "Processid" string.
                     * FIXME: read from registry.
                     */
                    (wcsicmp(pi->ProcessName, L"svchost.exe") != 0 &&
                     wcsicmp(pi->ProcessName, L"dllhost.exe") != 0))) {
                NULL_TERMINATE_BUFFER(qual_args);
                use_args = TRUE;
            } /* else not an error, just no args. e.g.: plain sqlservr.exe */
        } else {
            /* this is an error => notify user */
            _snwprintf(qual_args, BUFFER_SIZE_ELEMENTS(qual_args), L"<error>");
            NULL_TERMINATE_BUFFER(qual_args);
            use_args = TRUE;
        }
    }
    if (use_args) {
        _snwprintf(name_buf, name_buf_length, L"%s-%s", pi->ProcessName, qual_args);
    } else {
        _snwprintf(name_buf, name_buf_length, L"%s", pi->ProcessName);
    }
    NULL_TERMINATE_SIZED_BUFFER(name_buf, name_buf_length);
}

static void
print_mem_stats(process_info_t *pi, char reschar, int version)
{
    WCHAR qual_name[MAX_CMDLINE];
    int cpu = -1, user = -1;
    LONGLONG wallclock_time = get_system_time() - pi->CreateTime.QuadPart;
    LONGLONG scheduled_time = pi->UserTime.QuadPart + pi->KernelTime.QuadPart;
    static LONGLONG firstproc_time = 0;
    if (wallclock_time != (LONGLONG)0) {
        cpu = (int)((100 * scheduled_time) / wallclock_time);
    }
    if (scheduled_time != (LONGLONG)0) {
        user = (int)((100 * pi->UserTime.QuadPart) / scheduled_time);
    }

    /* Total user and kernel time scheduled for all processes.  Don't include idle
     * process, and if -skip option is specified don't include DRview.exe */
    if (wcsicmp(pi->ProcessName, L"") != 0 &&
        (wcsicmp(pi->ProcessName, L"drview.exe") != 0 || !skip)) {
        total_user += pi->UserTime.QuadPart;
        total_kernel += pi->KernelTime.QuadPart;
    }

    /* CreateTime.QuadPart is a counter since 1916.  Both idle process and
     * System have create time of 0, so report create time, in ms, relative to
     * smss.exe */
    if (wcsicmp(pi->ProcessName, L"") == 0 || wcsicmp(pi->ProcessName, L"system") == 0 ||
        wcsicmp(pi->ProcessName, L"smss.exe") == 0) {
        firstproc_time = pi->CreateTime.QuadPart;
    }

    generate_process_name(pi, qual_name, BUFFER_SIZE_ELEMENTS(qual_name));

    /* single line best so can line up columns and process output easily */
    fprintf(fp,
            "%-23.23S %5d %c %5d %2d%% %3d%% %5d %3d %7zd %7zd %8zd %7zd %7zd %7zd %7d "
            "%5zd %5zd "
            "%5zd %5zd %5d",
            qual_name, pi->ProcessID, reschar, version, cpu, user, pi->HandleCount,
            pi->ThreadCount, pi->VmCounters.PeakVirtualSize / 1024,
            pi->VmCounters.VirtualSize / 1024, pi->VmCounters.PeakPagefileUsage / 1024,
            pi->VmCounters.PagefileUsage / 1024, /* aka Private */
            pi->VmCounters.PeakWorkingSetSize / 1024,
            pi->VmCounters.WorkingSetSize / 1024, pi->VmCounters.PageFaultCount,
            pi->VmCounters.QuotaPeakPagedPoolUsage / 1024,
            pi->VmCounters.QuotaPagedPoolUsage / 1024,
            pi->VmCounters.QuotaPeakNonPagedPoolUsage / 1024,
            pi->VmCounters.QuotaNonPagedPoolUsage / 1024, pi->InheritedFromProcessID);
    if (showtime) {
        /* QuadPart is in ticks 1 tick = 100 nano secs. In ms = n * 100/(1000*1000) */
        fprintf(fp, " %15I64d %15I64d %15I64d", (pi->UserTime.QuadPart / 10000),
                (pi->KernelTime.QuadPart / 10000),
                ((pi->CreateTime.QuadPart - firstproc_time) / 10000));
    }
    fprintf(fp, "\n");
}

int
main(int argc, char **argv)
{
    int nprocs, argidx = 1;

    if (argc < 2)
        usage();

    while (argidx < argc) {

        if (!strcmp(argv[argidx], "-help")) {
            help();
        } else if (!strcmp(argv[argidx], "-pid") || !strcmp(argv[argidx], "-p")) {
            if (argidx + 1 >= argc)
                usage();
            pid = atoi(argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-exe")) {
            if (argidx + 1 >= argc)
                usage();
            exe = argv[++argidx];
            _snwprintf(wexe, MAX_PATH, L"%S", exe);
            wexe[MAX_PATH - 1] = L'\0';
        } else if (!strcmp(argv[argidx], "-listall")) {
            listall = TRUE;
        } else if (!strcmp(argv[argidx], "-listdr")) {
            listdr = TRUE;
        } else if (!strcmp(argv[argidx], "-listdlls")) {
            listdlls = TRUE;
        } else if (!strcmp(argv[argidx], "-showdlls")) {
            listdlls = TRUE;
            showdlls = TRUE;
        } else if (!strcmp(argv[argidx], "-nopid")) {
            nopid = TRUE;
        } else if (!strcmp(argv[argidx], "-no32")) {
            no32 = TRUE;
        } else if (!strcmp(argv[argidx], "-onlypid")) {
            onlypid = TRUE;
        } else if (!strcmp(argv[argidx], "-out")) {
            if (argidx + 1 >= argc)
                usage();
            outf = argv[++argidx];
        } else if (!strcmp(argv[argidx], "-cmdline")) {
            cmdline = TRUE;
        } else if (!strcmp(argv[argidx], "-showstats")) {
            showstats = TRUE;
        } else if (!strcmp(argv[argidx], "-showmem")) {
            showmem = TRUE;
        } else if (!strcmp(argv[argidx], "-showtime")) {
            if (!showmem)
                usage();
            showtime = TRUE;
        } else if (!strcmp(argv[argidx], "-nobuildnum")) {
            showbuild = FALSE;
        } else if (!strcmp(argv[argidx], "-qname")) {
            if (argidx + 1 >= argc)
                usage();
            qname = TRUE;
            strip = atoi(argv[++argidx]);
        } else if (!strcmp(argv[argidx], "-noqnames")) {
            noqnames = TRUE;
        } else if (!strcmp(argv[argidx], "-hot_patch")) {
            hotp = TRUE;
        } else if (!strcmp(argv[argidx], "-s")) {
            millis = (uint)atoi(argv[argidx + 1]);
            if (millis == 0) {
                millis = 500;
            } else {
                argidx++;
            }
            sampling = TRUE;
        } else if (!strcmp(argv[argidx], "-tillidle")) {
            millis = (millis <= 0) ? 500 : millis;
            idlecpu = (idlecpu <= 0) ? 3 : idlecpu;
            showmemfreq = (showmemfreq <= 1) ? 1 : showmemfreq;
            flag_after_ms = (flag_after_ms <= 0) ? 3000 : flag_after_ms;
            tillidle = TRUE;
            sampling = TRUE;
        } else if (!strcmp(argv[argidx], "-idlecpu")) {
            if (argidx + 1 >= argc)
                usage();
            idlecpu = (uint)atoi(argv[++argidx]);
            idlecpu = (idlecpu <= 0) ? 3 : idlecpu;
            millis = (millis <= 0) ? 500 : millis;
            flag_after_ms = (flag_after_ms <= 0) ? 3000 : flag_after_ms;
            tillidle = TRUE;
            sampling = TRUE;
        } else if (!strcmp(argv[argidx], "-showmemfreq")) {
            if (argidx + 1 >= argc)
                usage();
            showmemfreq = (uint)atoi(argv[++argidx]);
            showmemfreq = (showmemfreq <= 1) ? 1 : showmemfreq;
            showmem = TRUE;
            tillidle = TRUE;
            sampling = TRUE;
        } else if (!strcmp(argv[argidx], "-idleafter")) {
            if (argidx + 1 >= argc)
                usage();
            flag_after_ms = (uint)atoi(argv[++argidx]);
            flag_after_ms = (flag_after_ms <= 0) ? 3000 : flag_after_ms;
            idlecpu = (idlecpu <= 0) ? 3 : idlecpu;
            millis = (millis <= 0) ? 500 : millis;
            sampling = TRUE;
            tillidle = TRUE;
        } else if (!strcmp(argv[argidx], "-skip")) {
            /* internal option: skip drview's contribution to total scheduled time */
            skip = TRUE;
        } else if (!strcmp(argv[argidx], "-v")) {
#if defined(BUILD_NUMBER) && defined(VERSION_NUMBER)
            printf("drview.exe version %s -- build %d\n", STRINGIFY(VERSION_NUMBER),
                   BUILD_NUMBER);
#elif defined(BUILD_NUMBER)
            printf("drview.exe custom build %d -- %s\n", BUILD_NUMBER, __DATE__);
#else
            printf("drview.exe custom build -- %s, %s\n", __DATE__, __TIME__);
#endif
            exit(0);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[argidx]);
            usage();
        }
        argidx++;
    }

    if (listdlls && !pid && !exe && !listall && !listdr) {
        fprintf(stderr,
                "-listdlls option should be combined with a specific -pid, -exe, -listdr "
                "or -listall option\n");
        usage();
    }

    if (outf) {
        fp = fopen(outf, "w");
        if (fp == NULL) {
            fprintf(stderr, "Error opening %s for output.\n", outf);
            exit(-1);
        }
    } else
        fp = stdout;

    /* Invoke DLL routine when -listdlls combined with either -listall or -pid or -exe */
    if (listdlls && (listall || pid || exe || listdr)) {
        dllwalk();
    } else {
        do {
            nprocs = procwalk();
            fflush(fp);
            if (sampling)
                Sleep(millis);
            /* FIXME: looping infinitely, make sure we are not leaking anything */
        } while (sampling && !idle);
    }

    if (outf)
        fclose(fp);

    return nprocs;
}

int count;

/* returns FALSE if status is effectively disabled, otherwise returns
 *  TRUE (indicating status should be displayed. */
BOOL
get_status_string(char *buf, UINT maxchars, hotp_inject_status_t status,
                  hotp_policy_mode_t mode)
{
    char *statptr = NULL;
    char *modeptr = NULL;

    if (status == HOTP_INJECT_NO_MATCH && mode == HOTP_MODE_OFF)
        return FALSE;

    switch (status) {
    case HOTP_INJECT_ERROR: statptr = "Inject Error!"; break;
    case HOTP_INJECT_PROTECT: statptr = "Injected Protector"; break;
    case HOTP_INJECT_DETECT: statptr = "Injected Detector"; break;
    case HOTP_INJECT_IN_PROGRESS: statptr = "Injection in progress"; break;
    case HOTP_INJECT_PENDING: statptr = "Inject point not yet executed"; break;
    case HOTP_INJECT_NO_MATCH: statptr = "Not matched"; break;
    case HOTP_INJECT_OFF:
        /* not currently used. */
    default: statptr = NULL;
    }

    switch (mode) {
    case HOTP_MODE_OFF: modeptr = "Off"; break;
    case HOTP_MODE_DETECT: modeptr = "Detect"; break;
    case HOTP_MODE_PROTECT: modeptr = "Protect"; break;
    default: modeptr = NULL;
    }

    if (statptr == NULL || modeptr == NULL) {
        _snprintf(buf, maxchars, "[Status ERROR: Status=%d, Mode=%d]", status, mode);
    } else {
        _snprintf(buf, maxchars, "%s [%s]", modeptr, statptr);
    }

    buf[maxchars - 1] = '\0';

    return TRUE;
}

BOOL
pw_callback(process_info_t *pi, void **param)
{
    char *resstr;
    char reschar;
    int res;
    WCHAR buf[MAX_CMDLINE];
    DWORD version;
    BOOL under_dr;

    WCHAR qual_name[MAX_CMDLINE];
    if (exe)
        generate_process_name(pi, qual_name, BUFFER_SIZE_ELEMENTS(qual_name));

    if ((pid && pi->ProcessID == pid) ||
        (exe && (!wcsicmp(wexe, pi->ProcessName) || !wcsicmp(wexe, qual_name))) ||
        listall || listdr) {
        version = -1;
        res = under_dynamorio_ex(pi->ProcessID, &version);
        switch (res) {
        case DLL_PROFILE:
            resstr = NAME " profile";
            reschar = 'P';
            break;
        case DLL_RELEASE:
            resstr = NAME " release";
            reschar = 'R';
            break;
        case DLL_DEBUG:
            resstr = NAME " debug";
            reschar = 'D';
            break;
        case DLL_CUSTOM:
            resstr = NAME " custom";
            reschar = 'C';
            break;
        case DLL_NONE:
            resstr = "native";
            reschar = 'N';
            break;
        case DLL_UNKNOWN:
        default: resstr = "<error>"; reschar = '?';
        }

        under_dr = !(res == DLL_NONE || res == DLL_UNKNOWN);

        if (!listdr || under_dr) {
            if (!nopid && !showmem) {
                if (onlypid)
                    fprintf(fp, "%d\n", (DWORD)pi->ProcessID);
                else
                    fprintf(fp, "PID %d, ", (DWORD)pi->ProcessID);
            }
            if (!showmem && !onlypid) {
                WCHAR qual_name[MAX_CMDLINE];
                WCHAR *name_to_use = pi->ProcessName;
#ifdef X64
                HANDLE hproc =
                    OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pi->ProcessID);
                if (is_wow64(hproc)) {
                    if (!no32)
                        fprintf(fp, "32-bit, ");
                    /* FIXME: currently x64 process can't see 32-bit
                     * drmarker
                     */
                    resstr = "<unknown>";
                }
                CloseHandle(hproc);
#endif
                if (!noqnames) {
                    generate_process_name(pi, qual_name, BUFFER_SIZE_ELEMENTS(qual_name));
                    name_to_use = qual_name;
                }
                fprintf(fp, "Process %S, ", name_to_use);
                if (version == -1 || !showbuild)
                    fprintf(fp, "running %s\n", resstr);
                else
                    fprintf(fp, "running %s (build %d)\n", resstr, version);
            }
            if (cmdline) {
                res = get_process_cmdline(pi->ProcessID, buf, BUFFER_SIZE_ELEMENTS(buf));
                NULL_TERMINATE_BUFFER(buf);
                if (res == ERROR_SUCCESS) {
                    fprintf(fp, "\tCmdline: %S\n", buf);
                } else {
                    /* acquiring SeDebugPrivilege requires being admin */
                    if (res == ERROR_NOT_ALL_ASSIGNED)
                        fprintf(fp, "\t<Re-run as administrator for cmdline>\n");
                    else
                        fprintf(fp, "\t<Cmdline err %d>\n", res);
                }
            }
            if (qname) {
                WCHAR cmdline[MAX_CMDLINE];
                res = get_process_cmdline(pi->ProcessID, cmdline,
                                          BUFFER_SIZE_ELEMENTS(cmdline));
                NULL_TERMINATE_BUFFER(cmdline);
                if (res == ERROR_SUCCESS) {
                    if (!get_commandline_qualifier(cmdline, buf,
                                                   BUFFER_SIZE_ELEMENTS(buf), !strip))
                        buf[0] = L'\0'; /* no args */
                    NULL_TERMINATE_BUFFER(buf);
                }
                if (res == ERROR_SUCCESS)
                    fprintf(fp, "\tQname: %S%s%S\n", pi->ProcessName,
                            buf[0] == L'\0' ? "" : "-", buf);
                else
                    fprintf(fp, "\t<Qname err %d>\n", res);
            }
            if (under_dr && hotp) {
                hotp_policy_status_table_t *status_tbl = NULL;
                res = get_hotp_status(pi->ProcessID, &status_tbl);
                if (res == ERROR_SUCCESS) {
                    uint j;
                    hotp_policy_status_t *cur;
                    fprintf(fp, "\tHotpatching:\n");
                    for (j = 0; j < status_tbl->num_policies; j++) {
                        char status_buf[MAX_PATH];
                        cur = &(status_tbl->policy_status_array[j]);
                        if (get_status_string(status_buf, MAX_PATH, cur->inject_status,
                                              cur->mode))
                            fprintf(fp, "\t  Patch %s: %s\n", cur->policy_id, status_buf);
                    }
                } else if (res == ERROR_DRMARKER_ERROR) {
                    fprintf(fp, "\tHot Patching Not Enabled\n");
                } else {
                    fprintf(fp, "\t<Hotpatch Query Error %d>\n", res);
                }
            }
            if (under_dr && showstats) {
                dr_statistics_t *stats = get_dynamorio_stats(pi->ProcessID);
                if (stats != NULL) {
                    uint i;
                    fprintf(fp, "\t%.*s\n",
                            (int)BUFFER_SIZE_ELEMENTS(stats->process_name),
                            stats->process_name);
                    for (i = 0; i < stats->num_stats; i++) {
                        fprintf(fp, "\t%*.*s :%9zd\n",
                                (int)BUFFER_SIZE_ELEMENTS(stats->stats[i].name),
                                (int)BUFFER_SIZE_ELEMENTS(stats->stats[i].name),
                                stats->stats[i].name, stats->stats[i].value);
                    }
                }
                free_dynamorio_stats(stats);
            }
            if (showmem) {
                print_mem_stats(pi, reschar, version);
            }
            count++;
        }
    }
    return TRUE;
}

BOOL
dllw_callback(module_info_t *mi, void **param)
{
    char *resstr;
    char reschar;
    int res;
    DWORD version;
    BOOL exe_match = (exe && wcsstr(mi->BaseDllName, wexe)) ? TRUE : FALSE;
    BOOL exe_present =
        (wcsstr(mi->BaseDllName, L".exe") || wcsstr(mi->BaseDllName, L".EXE")) ? TRUE
                                                                               : FALSE;

    /* -listdlls option is set && any of below 3 conditions hold true.
       (1) -pid value is set and current threadID matches.
       (2) -exe is set and all DLLs with matching threadIDs (a fake comparison) since
       WINDOWS provides no other handle. (3) -listall option is set
    */

    if (listdlls &&
        ((pid && mi->ProcessID == pid) ||
         ((exe_match ||
           (exe && save_pid == mi->ProcessID &&
            (!exe_present || wcscmp(save_module, mi->BaseDllName) == 0)))) ||
         (listall || listdr))) {
        version = -1;
        res = under_dynamorio_ex(mi->ProcessID, &version);
        switch (res) {
        case DLL_PROFILE:
            resstr = NAME " profile";
            reschar = 'P';
            break;
        case DLL_RELEASE:
            resstr = NAME " release";
            reschar = 'R';
            break;
        case DLL_DEBUG:
            resstr = NAME " debug";
            reschar = 'D';
            break;
        case DLL_CUSTOM:
            resstr = NAME " custom";
            reschar = 'C';
            break;
        case DLL_NONE:
            resstr = "native";
            reschar = 'N';
            break;
        case DLL_UNKNOWN:
        default: resstr = "<error>"; reschar = '?';
        }

        save_pid = mi->ProcessID;

        if (!(listdr && (res == DLL_NONE || res == DLL_UNKNOWN))) {
            if (wcsstr(mi->BaseDllName, L".exe") || wcsstr(mi->BaseDllName, L".EXE")) {
                wcsncpy(save_module, mi->BaseDllName,
                        sizeof(save_module) / sizeof(save_module[0]));
                save_module[(sizeof(save_module) / sizeof(save_module[0])) - 1] = L'\0';
                if (!nopid && !showmem)
                    fprintf(fp, "\n\nPID " PIDFMT, mi->ProcessID);
                if (!showmem) {
                    if (version == -1 || !showbuild) {
                        fprintf(fp, "\t\tProcess %S, running %s\n", mi->BaseDllName,
                                resstr);
                    } else {
                        fprintf(fp, "\t\tProcess %S, running %s (build %d)\n",
                                mi->BaseDllName, resstr, version);
                    }
                }
                count = 0;
            } else {
                if (!showmem) {
                    if (showdlls) {
                        /* long format */
                        fprintf(fp, "  %p-%p  %-16S Stamp=%x Count=%d\n    %S\n",
                                mi->BaseAddress,
                                (char *)mi->BaseAddress + mi->SizeOfImage,
                                mi->BaseDllName, mi->TimeDateStamp, mi->LoadCount,
                                mi->FullDllName);
                    } else {
                        fprintf(fp, "\t%-16S", mi->BaseDllName);
                        count++;
                        if ((count % 3) == 0) {
                            fprintf(fp, "\n");
                            count = 0;
                        }
                    }
                }
            }
        }
    }

    return TRUE;
}

int
procwalk()
{
    static int pwalk_per = 0;
    uint system_load = get_system_load(sampling);
    if (tillidle) {
        static uint idle_for_ms = 0;
        idle_for_ms = (system_load < idlecpu) ? idle_for_ms + millis : 0;
        idle = (idle_for_ms >= flag_after_ms);
    }

    count = 0;
    if (showmem) {
        SYSTEM_PERFORMANCE_INFORMATION sperf_info;
        if (get_system_performance_info(&sperf_info)) {
            fprintf(fp, "System committed memory (KB): %d / %d peak %d\n",
                    /* in pages so x4==KB */
                    4 * sperf_info.TotalCommittedPages, 4 * sperf_info.TotalCommitLimit,
                    4 * sperf_info.PeakCommitment);
            /* FIXME: add physical memory, kernel memory, etc. */
        }
        fprintf(fp, "System load: %d%%\t\tUptime: %lu ms\n", system_load, get_uptime());

        /* %user reaches 100% so we give it the extra column over %cpu */
        fprintf(fp,
                "%-23s %5s %7s %3s %4s %5s %3s %7s %7s %8s %7s %7s %7s %7s %5s %5s %5s "
                "%5s %5s",
                "Name-Qualification", "PID", "DR  Bld", "CPU", "User", "Hndl", "Thr",
                "PVSz", "VSz", "PPriv", "Priv", "PWSS", "WSS", "Fault", "PPage", "Page",
                "PNonP", "NonP", "PPID");
        if (showtime)
            fprintf(fp, " %15s %15s %15s", "User Time(ms)", "Kernel Time(ms)",
                    "Create Time(ms)");
        fprintf(fp, "\n");
    }
    if (pwalk_per == 0 || pwalk_per % showmemfreq == 0) {
        process_walk(&pw_callback, NULL);
        if (!count)
            fprintf(fp, "No such process found.\n");
        else {
            if (showmem && showtime) {
                fprintf(fp, "Total scheduled user/kernel time: %I64d/%I64d ms\n",
                        total_user / 10000, total_kernel / 10000);
            }
        }
    }
    pwalk_per++;

    total_user = 0;
    total_kernel = 0;

    return count;
}

void
dllwalk()
{
    count = 0;
    dll_walk_all(&dllw_callback, NULL);
}
