/* **********************************************************
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

//
// -wrapping event log ids
// -what happens when log is cleared
//

#include "share.h"
#include "elm.h"
#include "events.h"

#ifndef UNIT_TEST

#    define BUFFER_SIZE 8192
#    define MAX_MSG_STRINGS 16

int control;
HANDLE elm_thread = NULL;

/* if true, the formatted callback is used; otherwise the raw callback */
BOOL format_messages;

DWORD WINAPI
eventLogMonitorThreadProc(LPVOID elm_info_param);

typedef struct eventlogmon_info_ {
    eventlog_formatted_callback cb_format;
    eventlog_raw_callback cb_raw;
    eventlog_error_callback cb_err;

    /* this is passed to start_eventlog_monitor; nodemanager keeps
     *  track of it in the registry. */
    DWORD next_record;
} eventlogmon_info;

void
stop_eventlog_monitor()
{
    BOOL wait = TRUE;
    control = 1;
    if (wait) {
        WaitForSingleObject(elm_thread, 5000);
    }
    CloseHandle(elm_thread);
    elm_thread = NULL;
}

/* returns a handle to the thread, which can be waited on */
HANDLE
get_elm_thread_handle()
{
    return elm_thread;
}

DWORD
start_eventlog_monitor(BOOL use_formatted_callback, eventlog_formatted_callback cb_format,
                       eventlog_raw_callback cb_raw, eventlog_error_callback cb_err,
                       DWORD next_eventlog_record)
{
    DWORD tid;
    eventlogmon_info *elmi;

    if ((use_formatted_callback && cb_format == NULL) ||
        (!use_formatted_callback && cb_raw == NULL))
        return ERROR_INVALID_PARAMETER;

    /* make sure there isn't already an elm running */
    if (elm_thread != NULL)
        return ERROR_ALREADY_INITIALIZED;

    elmi = (eventlogmon_info *)malloc(sizeof(eventlogmon_info));
    elmi->cb_format = cb_format;
    elmi->cb_raw = cb_raw;
    elmi->cb_err = cb_err;
    elmi->next_record = next_eventlog_record;
    format_messages = use_formatted_callback;

    elm_thread = CreateThread(NULL, 0, &eventLogMonitorThreadProc, elmi, 0, &tid);
    if (elm_thread == NULL)
        return GetLastError();

    return ERROR_SUCCESS;
}

#    define MAX_EVENTLOG_SOURCES 8

BOOL eventsources_loaded = FALSE;

DWORD
get_formatted_message(EVENTLOGRECORD *pevlr, WCHAR *buf, DWORD maxchars)
{
    static int num_eventlog_sources = 0;
    static HMODULE hm[MAX_EVENTLOG_SOURCES];
    HKEY hes = NULL, hel = NULL;
    DWORD len;
    WCHAR message_file_buf[MAX_PATH];
    LPBYTE msgarray[MAX_MSG_STRINGS];
    int i = 0;
    DWORD res;

    /* load sources upon first request */
    if (num_eventlog_sources == 0) {

        /* We don't use product_key_flags() (PR 244206) b/c it's a pain to link
         * with utils.obj, and we don't need the flags for HKLM\System
         */
        res = RegOpenKey(HKEY_LOCAL_MACHINE, EVENT_LOG_KEY, &hel);

        if (res != ERROR_SUCCESS) {
            return res;
            goto formatted_msg_error;
        }

        for (;;) {

            if (num_eventlog_sources >= MAX_EVENTLOG_SOURCES) {
                res = ERROR_INSUFFICIENT_BUFFER;
                goto formatted_msg_error;
            }

            len = MAX_PATH;
            res = RegEnumKeyEx(hel, i++, message_file_buf, &len, 0, NULL, NULL, NULL);
            if (res == ERROR_NO_MORE_ITEMS) {
                res = ERROR_SUCCESS;
                break;
            } else if (res != ERROR_SUCCESS) {
                goto formatted_msg_error;
            }

            /* We don't use product_key_flags() (PR 244206) b/c it's a pain to link
             * with utils.obj, and we don't need the flags for HKLM\System
             */
            res = RegOpenKey(hel, message_file_buf, &hes);
            if (res == ERROR_SUCCESS) {
                len = MAX_PATH;
                res = RegQueryValueEx(hes, L"EventMessageFile", NULL, NULL,
                                      (LPBYTE)message_file_buf, &len);
                if (res == ERROR_SUCCESS) {
                    /* load the module for the message resources */
                    hm[num_eventlog_sources++] =
                        LoadLibraryEx(message_file_buf, NULL, LOAD_LIBRARY_AS_DATAFILE);
                }
            }
            if (hes != NULL) {
                RegCloseKey(hes);
                hes = NULL;
            }
        }
    }

    /* now, format */
    if (pevlr->NumStrings > MAX_MSG_STRINGS) {
        res = ERROR_INSUFFICIENT_BUFFER;
        goto formatted_msg_error;
    }

    msgarray[0] = (LPBYTE)pevlr + pevlr->StringOffset;

    for (i = 1; i < pevlr->NumStrings; ++i)
        msgarray[i] =
            msgarray[i - 1] + sizeof(WCHAR) * (1 + wcslen((LPTSTR)msgarray[i - 1]));

    /* try each event source, returning only when FormatMessage succeeds.
     * this is a little slutty as we could probably look up the event
     *  source based from the event itself, but it's not too bad. */
    len = 0;
    for (i = 0; i < num_eventlog_sources; i++) {
        if (hm[i] != NULL) {
            len =
                FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                              hm[i], pevlr->EventID, 0, buf, maxchars, (char **)msgarray);
            if (len > 0)
                break;
        }
    }

    if (len == 0) {
        res = ERROR_PARSE_ERROR;
        goto formatted_msg_error;
    } else {
        buf[maxchars - 1] = L'\0';
        res = ERROR_SUCCESS;
    }

formatted_msg_error:

    if (hes != NULL)
        RegCloseKey(hes);
    if (hel != NULL)
        RegCloseKey(hel);

    return res;
}

BOOL do_once = FALSE;

DWORD WINAPI
eventLogMonitorThreadProc(LPVOID elm_info_param)
{
    EVENTLOGRECORD *pevlr;
    BYTE bBuffer[BUFFER_SIZE] = { 0 };
    DWORD dwRead, dwNeeded, res;
    DWORD reported_next_record, num_records;
    BOOL skip_first = FALSE;
    HANDLE log = NULL, event = NULL;
    WCHAR msgbuf[BUFFER_SIZE];

    eventlogmon_info *elm_info = (eventlogmon_info *)elm_info_param;

    control = 0;

    log = OpenEventLog(NULL, L_COMPANY_NAME);
    if (log == NULL) {
        (*elm_info->cb_err)(ELM_ERR_FATAL,
                            L"Could not open the " L_COMPANY_NAME L" event log.");
        goto exit_elm_thread_error;
    }

    event = CreateEvent(NULL, FALSE, FALSE, NULL);
    NotifyChangeEventLog(log, event);

    pevlr = (EVENTLOGRECORD *)&bBuffer;

    if (!GetNumberOfEventLogRecords(log, &num_records) ||
        !GetOldestEventLogRecord(log, &reported_next_record)) {
        _snwprintf(msgbuf, BUFFER_SIZE_ELEMENTS(msgbuf),
                   L"error %d getting eventlog info", GetLastError());
        (*elm_info->cb_err)(ELM_ERR_FATAL, msgbuf);
        goto exit_elm_thread_error;
    }

    /* FIXME: we don't handle the situation when the eventlog was cleared,
     *  but our pointer is less than the number of new records. for this
     *  we'll probably have to store a timestamp, and compare against the
     *  event record at next_record. */
    if (((int)elm_info->next_record) < 0) {
        elm_info->next_record = reported_next_record;
    } else if (elm_info->next_record > (reported_next_record + num_records + 1)) {
        /* looks like the eventlog was cleared since we last checked
         *  it. issue a warning and reset */
        elm_info->next_record = reported_next_record;
        (*elm_info->cb_err)(ELM_ERR_CLEARED, L"Eventlog was cleared!\n");
    } else {
        /* we need to ensure we SEEK to a valid record; but since
         * it's already been reported, don't report it again. */
        elm_info->next_record--;
        skip_first = TRUE;
    }

    /* first seek to the last record
     * EVENTLOG_FORWARDS_READ indicates we will get messages in
     *  chronological order.
     * FIXME: test to make sure that this works properly on
     *  overwrite-wrapped logs */
    if (!ReadEventLog(log, EVENTLOG_FORWARDS_READ | EVENTLOG_SEEK_READ,
                      elm_info->next_record, pevlr, BUFFER_SIZE, &dwRead, &dwNeeded)) {
        dwRead = 0;
        dwNeeded = 0;
    }

    for (;;) {
        do {
            /* case 5813: if pevlr->Length is 0, we'll have an infinite
             * loop. this could possibly happen if drive is full?
             * just abort if we detect it. */
            while (dwRead > 0 && pevlr->Length > 0) {

                if (format_messages && !skip_first) {

                    res = get_formatted_message(pevlr, msgbuf, BUFFER_SIZE);
                    if (res != ERROR_SUCCESS) {
                        _snwprintf(msgbuf, BUFFER_SIZE_ELEMENTS(msgbuf),
                                   L"FormatMessage error %d\n", res);
                        (*elm_info->cb_err)(ELM_ERR_WARN, msgbuf);
                    } else {
                        /* invoke the callback */
                        (*elm_info->cb_format)(pevlr->RecordNumber, pevlr->EventType,
                                               msgbuf, pevlr->TimeGenerated);
                    }
                } else if (!skip_first) {
                    /* xref case 3065: insurance */
                    if (pevlr->RecordNumber != 0 || pevlr->TimeGenerated != 0) {
                        /* raw callback */
                        (*elm_info->cb_raw)(pevlr);
                    }
                } else {
                    skip_first = FALSE;
                }

                dwRead -= pevlr->Length;
                pevlr = (EVENTLOGRECORD *)((LPBYTE)pevlr + pevlr->Length);
            }

            pevlr = (EVENTLOGRECORD *)&bBuffer;
        } while (ReadEventLog(log, EVENTLOG_FORWARDS_READ | EVENTLOG_SEQUENTIAL_READ, 0,
                              pevlr, BUFFER_SIZE, &dwRead, &dwNeeded));

        if ((res = GetLastError()) != ERROR_HANDLE_EOF) {
            // FIXME: assert GetLastError() is appropriate
            _snwprintf(msgbuf, BUFFER_SIZE_ELEMENTS(msgbuf),
                       L"Unexpected error %d reading event log\n", res);
            (*elm_info->cb_err)(ELM_ERR_WARN, msgbuf);
        }

        if (do_once)
            break;

        /* the event is auto-reset.
           timeout because NotifyChangeEventLog is not reliable. */
        WaitForSingleObject(event, MINIPULSE);

        if (control)
            break;
    }

exit_elm_thread_error:

    if (log != NULL)
        CloseEventLog(log);

    if (event != NULL)
        CloseHandle(event);

    free(elm_info);

    /* FIXME: need ExitThread()? */
    return 0;
}

/* insertion strings are null separated. for pevlr of type
 *  EVENTLOGRECORD, the first string is at
 * ((LPBYTE) pevlr) + pevlr->StringOffset
 *  undefined results if next_message_string is called more than
 *  NumStrings times. */
const WCHAR *
next_message_string(const WCHAR *prev_string)
{
    return prev_string + wcslen(prev_string) + 1;
}

const WCHAR *
get_message_strings(EVENTLOGRECORD *pevlr)
{
    return (WCHAR *)(((LPBYTE)pevlr) + pevlr->StringOffset);
}

const WCHAR *
get_event_exename(EVENTLOGRECORD *pevlr)
{
    /* exename is always the first msg string */
    WCHAR *exename;
    const WCHAR *exepath = get_message_strings(pevlr);
    exename = wcsrchr(exepath, L'\\');
    if (exename == NULL)
        return exepath;
    else
        return exename + 1;
}

UINT
get_event_pid(EVENTLOGRECORD *pevlr)
{
    DO_ASSERT(pevlr != NULL);

    /* PID is always the second msg string */
    return (UINT)_wtoi(next_message_string(get_message_strings(pevlr)));
}

/* For a MSG_SEC_FORENSICS eventlog record, returns the filename of the forensics file
 * generated. */
const WCHAR *
get_forensics_filename(EVENTLOGRECORD *pevlr)
{
    DO_ASSERT(pevlr != NULL && pevlr->EventID == MSG_SEC_FORENSICS);
    /* forensics file pathname will be third string */
    return next_message_string(next_message_string(get_message_strings(pevlr)));
}

BOOL
is_violation_event(DWORD eventType)
{
    switch (eventType) {
    case MSG_HOT_PATCH_VIOLATION:
    case MSG_SEC_VIOLATION_TERMINATED:
    case MSG_SEC_VIOLATION_CONTINUE:
    case MSG_SEC_VIOLATION_THREAD:
    case MSG_SEC_VIOLATION_EXCEPTION: return TRUE;
    default: return FALSE;
    }
}

const WCHAR *
get_event_threatid(EVENTLOGRECORD *pevlr)
{
    DO_ASSERT(pevlr != NULL);

    /* The Threat ID, if available, is always the 3rd parameter. */
    if (is_violation_event(pevlr->EventID)) {
        return next_message_string(next_message_string(get_message_strings(pevlr)));
    } else {
        return NULL;
    }
}

DWORD
clear_eventlog()
{
    DWORD res = ERROR_SUCCESS;
    HANDLE log = OpenEventLog(NULL, L_COMPANY_NAME);
    if (log == NULL)
        return GetLastError();
    if (!ClearEventLog(log, NULL))
        res = GetLastError();
    CloseHandle(log);
    return res;
}

#else // ifdef UNIT_TEST

int
main()
{
    // DWORD res;

    printf("All Test Passed\n");

    return 0;
}

#endif
