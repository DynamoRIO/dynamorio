/* **********************************************************
 * Copyright (c) 2003 VMware, Inc.  All rights reserved.
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

#define _WIN32_WINNT 0x0400

#include <Windows.h>
#include <stdio.h>
#include <Tlhelp32.h>
#include <tchar.h>
#include <pdh.h>

// a.s.
#include <stdlib.h>

#define MAXPATH 80
#define NCOUNTERS 27
#define MCOUNTERS 3

TCHAR *counters[NCOUNTERS] = {
    _T("Thread Count"),
    _T("Working Set"),
    _T("Page Faults/sec"),
    _T("Page File Bytes"),
    _T("% User Time"),
    _T("% Privileged Time"),
    _T("% Processor Time"),
    _T("Creating Process ID"),
    _T("Elapsed Time"),
    _T("Handle Count"),
    _T("ID Process"),
    _T("IO Data Bytes/sec"),
    _T("IO Data Operations/sec"),
    _T("IO Other Bytes/sec"),
    _T("IO Other Operations/sec"),
    _T("IO Read Bytes/sec"),
    _T("IO Read Operations/sec"),
    _T("IO Write Bytes/sec"),
    _T("IO Write Operations/sec"),
    _T("Page File Bytes Peak"),
    _T("Pool Nonpaged Bytes"),
    _T("Pool Paged Bytes"),
    _T("Priority Base"),
    _T("Private Bytes"),
    _T("Virtual Bytes"),
    _T("Virtual Bytes Peak"),
    _T("Working Set Peak"),
};

TCHAR *shortnames[NCOUNTERS] = {
    _T("tc"),      _T("wss"),     _T("pgflts"),  _T("pgfileK"), _T("utimes"),
    _T("ktimes"),  _T("ttimes"),  _T("ppid"),    _T("realtim"), _T("handles"),
    _T("pid"),     _T("IOdataK"), _T("IOdataO"), _T("IOothrK"), _T("IOothrO"),
    _T("IOreadK"), _T("IOreadO"), _T("IOwritK"), _T("IOwritO"), _T("pgfpeak"),
    _T("poolnpK"), _T("poolpK"),  _T("priorty"), _T("privK"),   _T("vmK"),
    _T("vmpeak"),  _T("wsspeak"),
};

TCHAR *formatstrings[NCOUNTERS] = {
    _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.2f\t"), _T("%.2f\t"),
    _T("%.2f\t"), _T("%.0f\t"), _T("%.2f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"),
    _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"),
    _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"),
    _T("%.0f\t"), _T("%.0f\t"), _T("%.0f\t"),
};

BOOL use_kb[NCOUNTERS] = {
    0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1,
};

int
waitforprocess(TCHAR *name)
{
    HANDLE processes;
    PROCESSENTRY32 pe = { sizeof(pe) };
    BOOL fOK;
    while (1) {
        processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        fOK = Process32First(processes, &pe);
        for (; fOK; fOK = Process32Next(processes, &pe)) {
            if (!_tcsicmp(pe.szExeFile, name))
                return pe.th32ProcessID;
        }
        CloseHandle(processes);
        Sleep(181);
    }
}

// a.s.
void
GetProcNameFromId(int pid, TCHAR *pName)
{
    HANDLE processes;
    PROCESSENTRY32 pe = { sizeof(pe) };
    BOOL fOK;
    while (1) {
        processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        fOK = Process32First(processes, &pe);
        for (; fOK; fOK = Process32Next(processes, &pe)) {
            if ((int)pe.th32ProcessID == pid) {
                _tcscpy(pName, pe.szExeFile);
                return;
            }
        }
        CloseHandle(processes);
        Sleep(181);
    }
}
// eof a.s.

int __cdecl _tmain(int argc, TCHAR **argv)
{
    HQUERY hQuery;
    HCOUNTER *pCounterHandle;
    PDH_STATUS pdhStatus;
    PDH_FMT_COUNTERVALUE fmtValue;
    DWORD ctrType;
    TCHAR szPathBuffer[MAXPATH];
    int i, samples = 0;
    int NUM_SAMPLES = 1000, INTERVAL_MS = 1000;
    int pid;
    FILE *fp = stdout;
    char fn[MAXPATH];
    TCHAR basename[MAXPATH];
    HANDLE htimer;
    LARGE_INTEGER duetime;

    if (argc < 2) {
        _ftprintf(stderr,
                  _T("Usage: %s [<exeName> | <pid> | all] [num_samples] [interval] ")
                  _T("[outputfile]\n"),
                  argv[0]);
        return -1;
    }

    if (_tcscmp(argv[1], _T("all"))) {
        if (isalpha(*argv[1])) {
            printf("waiting...\n");
            pid = waitforprocess(argv[1]);
            // remove .exe
            _tcscpy(basename, argv[1]);
            basename[_tcslen(argv[1]) - 4] = _T('\0');
        } else {
            pid = _ttoi(argv[1]);
            GetProcNameFromId(pid, basename);
            basename[_tcslen(basename) - 4] = _T('\0');
        }
        // eof a.s.
    } else {
        pid = 0;
        _tcscpy(basename, _T("_Total"));
    }

    if (argc > 2) {
        NUM_SAMPLES = _ttoi(argv[2]);
        if (argc > 3)
            INTERVAL_MS = _ttoi(argv[3]);
        if (argc > 4) {
            sprintf(fn, "%S", argv[4]);
            fp = fopen(fn, "w");
            if (fp == NULL) {
                _ftprintf(stderr, _T("error opening output file %s\n"), argv[4]);
                return -1;
            }
        }
    }

    _ftprintf(stderr, _T("Monitoring %s, pid=%d. Using %d samples at %dms interval\n"),
              basename, pid, NUM_SAMPLES, INTERVAL_MS);

    pdhStatus = PdhOpenQuery(0, 0, &hQuery);
    pCounterHandle = (HCOUNTER *)malloc(NCOUNTERS * sizeof(HCOUNTER));

    for (i = 0; i < NCOUNTERS; ++i) {
        _stprintf(szPathBuffer, _T("\\Process(%s)\\%s"), basename, counters[i]);
        pdhStatus = PdhAddCounter(hQuery, szPathBuffer, 0, &pCounterHandle[i]);
    }

    // "Prime" counters that need two values to display a formatted value.
    pdhStatus = PdhCollectQueryData(hQuery);

    // disp titles
    for (i = 0; i < NCOUNTERS; ++i)
        _ftprintf(fp, _T("%s\t"), shortnames[i]);
    _ftprintf(fp, _T("\n"));

    htimer = CreateWaitableTimer(NULL, FALSE, NULL);
    duetime.LowPart = 0;
    duetime.HighPart = 0;
    SetWaitableTimer(htimer, &duetime, INTERVAL_MS, NULL, NULL, FALSE);

    while (samples++ < NUM_SAMPLES) {

        // first wait is instantaneous
        WaitForSingleObject(htimer, INTERVAL_MS * 2);

        pdhStatus = PdhCollectQueryData(hQuery);

        for (i = 0; i < NCOUNTERS; ++i) {
            // Get the current value of this counter.
            pdhStatus = PdhGetFormattedCounterValue(pCounterHandle[i], PDH_FMT_DOUBLE,
                                                    &ctrType, &fmtValue);

            if (pdhStatus == ERROR_SUCCESS) {
                _ftprintf(fp, formatstrings[i],
                          use_kb[i] ? fmtValue.doubleValue / 1024 : fmtValue.doubleValue);
            } else {
                goto processgone;
            }
        }
        _ftprintf(fp, _T("\n"));
    }

processgone:

    pdhStatus = PdhCloseQuery(hQuery);

    if (fp != stdout)
        fclose(fp);

    free(pCounterHandle);

    return 0;
}
