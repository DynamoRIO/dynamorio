/* **********************************************************
 * Copyright (c) 2003-2007 VMware, Inc.  All rights reserved.
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
 *
 * elm.h
 *
 * TODO:
 *  -allow reporting of only "recent" events (eg after a given time)
 */

#ifndef _DETERMINA_ELM_H_
#define _DETERMINA_ELM_H_

/*
 *
 * BEWARE! from the msdn docs:

" When an event is written to the logfile specified by hEventLog, the
system uses the PulseEvent function to set the event specified by the
hEvent parameter to the signaled state. If the thread is not waiting
on the event when the system calls PulseEvent, the thread will not
receive the notification. Therefore, you should create a separate
thread to wait for notifications.

Note that the system calls PulseEvent no more than once every five
seconds. Therefore, even if more than one event log change occurs
within a five-second interval, you will only receive one
notification."

 *
 * to this end, the 'MINIPULSE' define below sets the polling
 *  interval, in ms
 */

#define MINIPULSE 100

HANDLE
get_elm_thread_handle();

const WCHAR *
get_message_strings(EVENTLOGRECORD *pevlr);

/* For a MSG_SEC_FORENSICS eventlog record, returns the filename of the forensics file
 * generated. */
const WCHAR *
get_forensics_filename(EVENTLOGRECORD *pevlr);

#endif
