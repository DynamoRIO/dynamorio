/* **********************************************************
 * Copyright (c) 2017-2022 Google, Inc.  All rights reserved.
 * Copyright (c) 2003-2009 VMware, Inc.  All rights reserved.
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

/* eventlog.c - Windows specific event logging issues */

#include "../globals.h"

#include <windows.h>
#include <tchar.h>
#include "../utils.h"

#include "events.h" /* generated from message file  */
#include "ntdll.h"

/* Types for Named pipe communication to the Event Log  */
#define NONCE_LENGTH 20

#define MAX_MESSAGE_SIZE 1024 /* total message size communicated to the eventlog */

/* Connection state */
typedef struct eventlog_state_t {
    HANDLE eventlog_pipe;
    HANDLE eventlog_completion; /* used for synchronization */
    uint message_seq;           /* message sequence number */
    char nonce[NONCE_LENGTH];   /* nonce received from server on handshake */
    mutex_t eventlog_mutex;     /* sync persistent thread shared logging connection */
    /* place buffers here to save stack space, used by [de]register and report
     * all of whom protect them with the above lock, this structure is single
     * instance static anyways so not wasting much memory doing it this way */
    char outbuf[MAX_MESSAGE_SIZE];
    size_t outlen;
    char buf[MAX_MESSAGE_SIZE];
    int request_length;
} eventlog_state_t;

/* writes a message to the Windows Event Log */
static void
os_eventlog(syslog_event_type_t priority, uint message_id, uint substitutions_num,
            char **arguments, size_t size_data, char *raw_data);

/* Custom logfile key and properties.
 * This enables administrators to control the size of the log file,
 * and we can attach SACLs for security purposes, without affecting other applications.
 */

// Make sure the registry key is all set up, maybe better done in the installer?
// The minimum we need:

/* addsource.reg:
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Eventlog\Araksha]

[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\Eventlog\Araksha\DynamoRIO]
"TypesSupported"=dword:00000007
"EventMessageFile"="C:\\cygwin\\home\\vlk\\exports\\x86_win32_dbg\\dynamorio.dll"
*/

/* sets the values for the already existing event source key */
static int
set_event_source_registry_values()
{
    int res;
    HANDLE heventsource = reg_open_key(L_EVENT_SOURCE_KEY, KEY_SET_VALUE);
    /* the message file is in our main dll */
    char *message_file = get_dynamorio_library_path();
    wchar_t wide_message_file[MAXIMUM_PATH];

    if (!heventsource) {
        return 0;
    }

    ASSERT(message_file && strlen(message_file) < MAXIMUM_PATH);
    _snwprintf(wide_message_file, MAXIMUM_PATH, L"%S", message_file);
    NULL_TERMINATE_BUFFER(wide_message_file);

    // FIXME: BUFOV? is the conversion locale dependent

    res = reg_set_dword_key_value(heventsource,
                                  L"TypesSupported", // which messages can go in
                                  EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
                                      EVENTLOG_INFORMATION_TYPE);

    res |= reg_set_dword_key_value(heventsource,
                                   L"CategoryCount", // # of event categories supported
                                   0);

    /* REG_EXPAND_SZ can contain a level of "indirection" in the form of a
       system variable that can be resolved at the time of use of the
       entry. For example: EventMessageFile="%WINDIR%\\dynamorio.dll"
    */

    // FIXME: I'd rather set the full REG_EXPAND_SZ to be prepared
    res |= reg_set_key_value(heventsource, L"EventMessageFile",
                             /* should be the name of our DLL (or RLL if we put in
                              * a separate file) */
                             wide_message_file);

    res |= reg_set_key_value(heventsource, L"CategoryMessageFile", wide_message_file);

    /* we don't use these
       DisplayNameFile
       DisplayNameID
       ParameterMessageFile
    */

    reg_close_key(heventsource);

    return 1;
}

/* returns 1 on successfully setting up the registry variables for an event source */
static int
init_registry_source(void)
{
    static int initialized = 0;
    /* FIXME: we assume no one should have access to modify after we check */
    /* FIXME: may want to register for notifications .. */
    /* FIXME: if we fail we'll do this over and over for each event .. */

    /* use our registry routines to avoid Win32 reentrancy issues */

    /* FIXME: let's do it with access rights only as needed- I got fed up at one point*/
    if (!initialized) {
        /* first make sure all keys are created */
        HANDLE heventsource = reg_open_key(L_EVENT_SOURCE_KEY, KEY_READ | KEY_WRITE);
        HANDLE heventlogroot = 0;
        HANDLE heventlog = 0;

        if (!heventsource) { /* we're not in */
            /* test and create eventlog key  */
            heventlog = reg_open_key(L_EVENT_LOG_KEY, KEY_READ | KEY_WRITE);
            // KEY_READ == KEY_QUERY_VALUE | KEY_NOTIFY |
            //             KEY_ENUMERATE_SUB_KEYS seems too strong

            if (!heventlog) {
                heventlogroot =
                    reg_open_key(L_EVENTLOG_REGISTRY_KEY, KEY_READ | KEY_WRITE);
                if (!heventlogroot) {
                    LOG(GLOBAL, LOG_TOP, 1,
                        "WARNING: Registration failure.  Could not open root %ls.",
                        L_EVENTLOG_REGISTRY_KEY);
                    return 0;
                }
                heventlog =
                    reg_create_key(heventlogroot, L_EVENT_LOG_NAME, KEY_ALL_ACCESS);
            }
            if (!heventlog) {
                LOG(GLOBAL, LOG_TOP, 1, "WARNING: Could not create event log key %s.",
                    EVENTLOG_NAME);
                return 0;
            }

            /* obviously we'll need SET_VALUE later but to keep the
             * logic simple we take minimal here
             */
            heventsource =
                reg_create_key(heventlog, L_EVENT_SOURCE_NAME, KEY_QUERY_VALUE);
            if (heventlog) {
                reg_close_key(heventlog);
            }
            if (heventlogroot) {
                reg_close_key(heventlogroot);
            }
        }
        if (!heventsource) {
            LOG(GLOBAL, LOG_TOP, 1, "WARNING: Could not create event source key %s.",
                EVENTSOURCE_NAME);
            return 0;
        }

        reg_close_key(heventsource);
        initialized = set_event_source_registry_values();
    }

    return initialized;
}

#define MAX_SYSLOG_ARGS 6 /* increase if necessary */
/* collect arguments in an array and pass along */
void
os_syslog(syslog_event_type_t priority, uint message_id, uint substitutions_num,
          va_list vargs)
{
    uint arg;
    char *arg_arr[MAX_SYSLOG_ARGS];

    // pointer to raw data TODO: SYSLOG_DATA entry point that also adds data arguments
    char *other_data = "";
    size_t size_data = strlen(other_data); /* 0 - for no data */

    ASSERT(substitutions_num < MAX_SYSLOG_ARGS);

    for (arg = 0; arg < substitutions_num; arg++) {
        arg_arr[arg] = va_arg(vargs, char *);
    }

    /* don't need to check syslog, mask, caller is responsible for
     * checking the mask and synchronizing the options
     */
    os_eventlog(priority, message_id, substitutions_num, arg_arr, size_data, other_data);
}

/* Here starts the gross hack for direct message passing to the EventLog service */

/* Macros for adding variable length fields to a message buffer
   p should point to the current position in the string appended to
   pend points to the first location after the end of the buffer  */
/* Modifies p */
#define FIELD(p, pend, type, val)                \
    do {                                         \
        if (!(p) || (p) + sizeof(type) > (pend)) \
            return 0;                            \
        *(type *)(p) = (type)(val);              \
        (p) += sizeof(type);                     \
    } while (0)

/* pass &p */
#define VARFIELD(pp, pend, val, len)              \
    do {                                          \
        if (!(*(pp)) || (*(pp)) + (len) > (pend)) \
            return 0;                             \
        memcpy((*(pp)), (val), (len));            \
        *(pp) += (len);                           \
    } while (0)

/* The advapi functions don't actually zero out the padding */
/* Modifies p (i.e. *pp) */
#define PADDING(pp, pend, len, boundary)           \
    do {                                           \
        int skip = (int)PAD((len), (boundary));    \
        if (!(*(pp)) || (*(pp)) + (skip) > (pend)) \
            return 0;                              \
        while (skip--)                             \
            *(*(pp))++ = '\0';                     \
    } while (0)

/* Encodes an ASCIIZ string into some weird format */
/* Example "\011\0\012\0""03B\0""\12\0\0\0DynamoRio\0\0\0"
   CHECK: Is this a documented M$ string representation?
   It seems like there is a lot of redundancy in the encoding,
   but it maybe useful in later reincarnations of the message.
   This is in fact a plain UNICODE_STRING.
*/
static inline char *
append_string(char **pp /* INOUT */, char *pend, char *str)
{
    size_t length = strlen(str) + 1;
    WORD len;

    ASSERT_TRUNCATE(len, ushort, length); /* ushort == WORD */
    len = (WORD)length;

    FIELD(*pp, pend, WORD, len - 1);
    FIELD(*pp, pend, WORD, len);
    FIELD(*pp, pend, void *, str);
    FIELD(*pp, pend, DWORD, len);
    VARFIELD(pp, pend, str, len);
    PADDING(pp, pend, len, sizeof(DWORD));
    return *pp;
}

#define HEADER_SIZE 24
#define HEADER_OFFSET 28

static inline char *
prepend_header(char *p, char *pend, char *header, int length, int sequence, DWORD unknown)
{
    if (!p)
        return NULL;
    VARFIELD(&p, pend, header, 8);
    FIELD(p, pend, DWORD, length);
    FIELD(p, pend, DWORD, sequence);
    FIELD(p, pend, DWORD, length - HEADER_SIZE);
    FIELD(p, pend, DWORD, unknown);
    FIELD(p, pend, DWORD, 0);
    return p;
}

/* FIXME: this value needs to be decoded using Ethereal too. See case 5655. */
#define EVENTLOG "\20\0\0\0" /* always 16 */

/* The first byte of the hello_message string should be \x05, but this
 * triggers a false positive in McAfee. That's why that byte is set to
 * RPC_VERSION_BOGUS and replaced with RPC_VERSION_5 before it is used.
 * See case 5002 for more details.*/

#define RPC_VERSION_BOGUS "\xFF" /* must be a string */
#define RPC_VERSION_5 '\x05'     /* must be a character */

/* advapi sends this message for several days with different computer names,
   if I break the protocol then its hello request starts with H\0\0\0\5... */
static char hello_message[] = /* DCE RPC request, decoded by Ethereal */
    RPC_VERSION_BOGUS         /* Version: Should be 5, but we set it to a bogus value
                               * because of a false positive in McAfee. See case 5002 */
    "\x00"                    /* Version (minor): 0 */
    "\x0B"                    /* Packet type: Bind (11) */
    "\x03"                    /* Packet Flags: 0x03 */

    "\x10\x00\x00\x00" /* Data Representation: 10000000 */
    "\x48\x00"         /* Frag Length: 72 */
    "\x00\x00"         /* Auth Length: 0 */
    "\x01\x00\x00\x00" /* Call ID: 1 */
    "\xB8\x10"         /* Max Xmit Frag: 4280 */
    "\xB8\x10"         /* Max Recv Frag: 4280 */

    "\x00\x00\x00\x00"                 /* Assoc Group: 0x00000000 */
    "\x01\x00\x00\x00"                 /* Num Ctx Items: 1 */
    "\x00\x00"                         /* Context ID: 0 */
    "\x01\x00"                         /* Num Trans Items: 1 */
    "\xDC\x3F\x27\x82\x2A\xE3\xC3\x18" /* Interface UUID: */
    "\x3F\x78\x82\x79\x29\xDC\x23\xEA" /*   82273fdc-e32a-18c3-3f78-827929dc23ea */
    "\x00\x00"                         /* Interface Ver: 0 */
    "\x00\x00"                         /* Interface Ver Minor: 0 */

    "\x04\x5D\x88\x8A\xEB\x1C\xC9\x11" /* Transfer Syntax: */
    "\x9F\xE8\x08\x00\x2B\x10\x48\x60" /*   8a885d04-1ceb-11c9-9fe8-08002b104860 */
    "\x02\x00\x00\x00";

/* We ignore this response to the hello message:
 *
 *                      // DCE RPC response, decoded by Ethereal
 *  "\x05"              // Version: 5
 *  "\x00"              // Version (minor): 0
 *  "\x0C"              // Packet type: Bind_ack (12)
 *  "\x03"              // Packet Flags: 0x03
 *  "\x10\x00\x00\x00"  // Data Representation: 10000000
 *  "\x44\x00"          // Frag Length: 68
 *  "\x00\x00"          // Auth Length: 0
 *  "\x01\x00\x00\x00"  // Call ID: 1
 *  "\xB8\x10"          // Max Xmit Frag: 4280
 *  "\xB8\x10"          // Max Recv Frag: 4280
 *  "\x54\x13\x01\x00"  // Assoc Group: 0x00011354 (may vary)
 *  "\x0D\x00"          // Scndry Addr len: 13
 *  "\\PIPE\\ntsvcs\x00"    // Scndry Addr: \PIPE\ntsvcs
 *  "\x00"              // for alignment? (gets to 4 byte alignment)
 *  "\x01\x00\x00\x00"  // Num results: 1
 *  "\x00\x00\x00\x00"  // Ack result: Acceptance (0)
 *  "\x04\x5D\x88\x8A\xEB\x1C\xC9\x11" // Transfer Syntax:
 *  "\x9F\xE8\x08\x00\x2B\x10\x48\x60" //   8a885d04-1ceb-11c9-9fe8-08002b104860
 *  "\x02\x00\x00\x00"
 *
 * Note that the hello response has changed slightly in Vista (from hand
 * comparison) :
 * ...
 * ! "\x0F\x00"          // Scndry Addr len: 15
 * ! "\\PIPE\\eventlog\x00"    // Scndry Addr: \PIPE\eventlog
 * ! "\x00\x73\x00"      // for alignment? (gets to 4 byte alignment)
 * ...
 */

#define REPORT "\5\0\0\3"

#define REGISTER_UNKNOWN_HEADER *(int *)"\0\0\17\0"
#define REPORT_UNKNOWN_HEADER *(int *)"\0\0\22\0"
#define DEREGISTER_UNKNOWN_HEADER *(int *)"\0\0\3\0"

/* TODO: The client can talk to a named pipe server on a remote machine,
   then we will be able to get messages out even before the local services are started! */

#define EVENTLOG_NAMED_PIPE L"\\??\\PIPE\\EVENTLOG"

// debugging facility

#ifdef DEBUG
#    define PRINT(form, arg) LOG(GLOBAL, LOG_TOP, 3, form, arg)
static void
print_buffer_as_bytes(unsigned char *buf, size_t len)
{
    size_t i;
    int nonprint = 0;
    PRINT("%s", "\"");
    for (i = 0; i < len; i++) {
        if (isdigit(buf[i]) && nonprint) {
            PRINT("%s", "\"\""); // to make \01 into \0""1
        }
        if (buf[i] == '\\')
            PRINT("%s", "\\");

        if (isprint(buf[i])) {
            PRINT("%c", buf[i]);
            nonprint = 0;
        } else {
            PRINT("\\%o", buf[i]);
            nonprint = 1;
        }
    }
    PRINT("%s", "\"");
    PRINT("%s", ";\n");
}
#    undef PRINT
#endif /* DEBUG */

/* see comments above, the response message length changed in Vista */
#define HELLO_RESPONSE_LENGTH (get_os_version() < WINDOWS_VERSION_VISTA ? 68U : 72U)
#define REGISTER_RESPONSE_LENGTH 48
#define REPORT_RESPONSE_LENGTH 36

/* returns 0 on failure  */
/* must hold the eventlog mutex */
int
eventlog_register(eventlog_state_t *evconnection)
{
    char *p;

    ASSERT(evconnection != NULL);
    ASSERT_OWN_MUTEX(true, &evconnection->eventlog_mutex);

    evconnection->eventlog_completion = create_iocompletion();
    evconnection->eventlog_pipe =
        open_pipe(EVENTLOG_NAMED_PIPE, evconnection->eventlog_completion);

    if (!evconnection->eventlog_pipe) {
        LOG(GLOBAL, LOG_TOP, 1, "Couldn't open EVENTLOG\n");
        goto failed_to_register;
    }

    /* we can't do this in eventlog_init() b/c sometimes eventlog_register()
     * is called before eventlog_init()
     */
    SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
    /* Restore the first byte of hello_message before it is used. It was set
     * to a bogus value to avoid a McAfee false positive. See case 5002. */
    hello_message[0] = RPC_VERSION_5;
    SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);

    evconnection->request_length = sizeof(hello_message) - 1;
    evconnection->outlen =
        nt_pipe_transceive(evconnection->eventlog_pipe, hello_message,
                           evconnection->request_length, evconnection->outbuf,
                           sizeof(evconnection->outbuf), DYNAMO_OPTION(eventlog_timeout));
    DOLOG(2, LOG_TOP, {
        LOG(GLOBAL, LOG_TOP, 3, "inlen= %d; outlen = " SZFMT "\"\n",
            evconnection->request_length, evconnection->outlen);
        LOG(GLOBAL, LOG_TOP, 3, "char hello[] = ");
        print_buffer_as_bytes((byte *)hello_message, evconnection->request_length);
        LOG(GLOBAL, LOG_TOP, 3, "char hello_resp[] = ");
        print_buffer_as_bytes((byte *)evconnection->outbuf, evconnection->outlen);
    });

    if (evconnection->outlen != HELLO_RESPONSE_LENGTH) {
        LOG(GLOBAL, LOG_TOP, 1,
            "eventlog_register: Mismatch on HELLO_RESPONSE outlen=" SZFMT "\n",
            evconnection->outlen);
        goto failed_to_register;
    }

    /* the only expected message length, we're lenient on contents */

    evconnection->message_seq = 1; /* we start counting from source registration */
    p = evconnection->buf + HEADER_OFFSET;
    append_string(&p, evconnection->buf + sizeof(evconnection->buf), EVENTSOURCE_NAME);

#define REPORT_IN_LOG "Application"
    /* CHECK: I don't quite get how the log name here matters for Event Viewer,
       since the source is registered only under EVENTLOG_NAME subtree,
       TODO: yet we may want to have our own event file, and it may matter then.*/
    append_string(&p, evconnection->buf + sizeof(evconnection->buf), REPORT_IN_LOG);
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), "\1\0\0\0\1\0\0\0",
             8); /*UNKNOWN*/
    ASSERT(p);   // our buffer should be large enough

    IF_X64(ASSERT_TRUNCATE(evconnection->request_length, int, p - evconnection->buf));
    evconnection->request_length = (int)(p - evconnection->buf);
    p = prepend_header(evconnection->buf, evconnection->buf + sizeof(evconnection->buf),
                       REPORT EVENTLOG, evconnection->request_length,
                       evconnection->message_seq, REGISTER_UNKNOWN_HEADER);
    ASSERT(p);
    evconnection->message_seq++;

    evconnection->outlen =
        nt_pipe_transceive(evconnection->eventlog_pipe, evconnection->buf,
                           evconnection->request_length, evconnection->outbuf,
                           sizeof(evconnection->outbuf), DYNAMO_OPTION(eventlog_timeout));
    DOLOG(2, LOG_TOP, {
        LOG(GLOBAL, LOG_TOP, 3, "inlen= %d; outlen = " SZFMT "\n",
            evconnection->request_length, evconnection->outlen);
        LOG(GLOBAL, LOG_TOP, 3, "char reg[] = ");
        print_buffer_as_bytes((byte *)evconnection->buf, evconnection->request_length);
        LOG(GLOBAL, LOG_TOP, 3, "char reg_resp[] = ");
        print_buffer_as_bytes((byte *)evconnection->outbuf, evconnection->outlen);
    });

    /* the only expected message length, we're lenient on contents */
    if (evconnection->outlen != REGISTER_RESPONSE_LENGTH) {
        LOG(GLOBAL, LOG_TOP, 1,
            "eventlog_register: Mismatch on REGISTER_RESPONSE outlen=" SZFMT "\n",
            evconnection->outlen);
        goto failed_to_register;
    }

    // we can parse the output to verify its contents, yet we care only about the nonce
    memcpy(evconnection->nonce, &evconnection->outbuf[HEADER_OFFSET], NONCE_LENGTH);

    return 1;
    /* on error */
failed_to_register:
    if (evconnection->eventlog_completion) {
        close_handle(evconnection->eventlog_completion);
        evconnection->eventlog_completion = 0;
    }
    if (evconnection->eventlog_pipe) {
        close_file(evconnection->eventlog_pipe);
        evconnection->eventlog_pipe = 0;
    }
    return 0;
}

/* get computer name, (cached) */
char *
get_computer_name()
{
    static char computer_name[MAX_COMPUTERNAME_LENGTH + 5]; /* 15 + 5 */

    /* returns the Windows name of the current computer just like GetComputerName() */
    if (!computer_name[0]) {
        char buf[sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                 sizeof(wchar_t) * (MAX_COMPUTERNAME_LENGTH + 1)]; // wide
        KEY_VALUE_PARTIAL_INFORMATION *kvpi = (KEY_VALUE_PARTIAL_INFORMATION *)buf;

        if (reg_query_value(L"\\Registry\\Machine\\System\\CurrentControlSet"
                            L"\\Control\\ComputerName\\ActiveComputerName",
                            L"ComputerName", KeyValuePartialInformation, kvpi,
                            sizeof(buf), 0) == REG_QUERY_SUCCESS) {
            /* Case 8185: this reg key may not be set until after winlogon
             * starts up, and our first event may be post-init as well once
             * the eventlog service is up.  So we may need to unprotect
             * .data here.
             */
            if (dynamo_initialized) {
                ASSERT(check_should_be_protected(DATASEC_RARELY_PROT));
                SELF_UNPROTECT_DATASEC(DATASEC_RARELY_PROT);
            } else
                ASSERT(!DATASEC_PROTECTED(DATASEC_RARELY_PROT));
            snprintf(computer_name, sizeof(computer_name) - 1, "%*ls",
                     kvpi->DataLength / sizeof(wchar_t) - 1, (wchar_t *)kvpi->Data);
            /* computer_name is static so last element is zeroed */
            if (dynamo_initialized)
                SELF_PROTECT_DATASEC(DATASEC_RARELY_PROT);
        }
    }
    return computer_name;
}

/* must hold the eventlog mutex */
static int
eventlog_report(eventlog_state_t *evconnection, WORD severity, WORD category,
                DWORD message_id, void *pSID, uint substitutions_num,
                size_t raw_data_size, char **substitutions, char *raw_data)
{
    char *p;
    uint i;

    ULONG sec = query_time_seconds();

    ASSERT_OWN_MUTEX(true, &evconnection->eventlog_mutex);

    p = evconnection->buf + HEADER_OFFSET;
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), evconnection->nonce,
             NONCE_LENGTH);
    p -= 4; /* overwrite timestamp? */
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD, sec);
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), WORD, severity);
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), WORD, category);
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD, message_id);
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), WORD, substitutions_num);

    /* FIXME: should write this constant in hex instead - \333 is not meaningful anyways
     * with the following broken code we've been writing
     * $SG23701 DB     0dbH, 'w', 00H
     *   which is 0x77db.
     * We should either keep using the magic value that has worked,
     * or figure out what should have really been written there.
     */
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), WORD,
          *(WORD *)"\333w"); /* FIXME: ReservedFlags? */
    IF_X64(ASSERT(CHECK_TRUNCATE_TYPE_uint(raw_data_size)));
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD, (DWORD)raw_data_size);
    append_string(&p, evconnection->buf + sizeof(evconnection->buf), get_computer_name());

    /* FIXME: This used to be type DWORD: I'm guessing that it should be widened */
    IF_X64(ASSERT_NOT_TESTED());
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), void *, pSID);
    if (pSID) {
        // FIXME: dump a SID in binary format
        // FIXME: the actual structure order seems to be
        // WORD(sub_authorities_num),
        // 48 bit authority value,
        // sub_authorities_num * ( 48 bit sub-authority values)
    }

    // FIXME: these don't seem to be either offsets nor pointers
    // but are some function of the number of substitutions

    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD,
          *(DWORD *)"\230y\23\0"); /* FIXME pointer placeholder */
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD, substitutions_num);
    for (i = 0; i < substitutions_num; i++) {
        /* FIXME unknown pointer placeholder */
        FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD,
              *(DWORD *)"\210y\23\0");
    }

    for (i = 0; i < substitutions_num; i++) {
        append_string(&p, evconnection->buf + sizeof(evconnection->buf),
                      substitutions[i]);
    }

    /* just the pointer */
    /* FIXME: This used to be type DWORD: I'm guessing that it should be widened */
    IF_X64(ASSERT_NOT_TESTED());
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), char *, raw_data);
    FIELD(p, evconnection->buf + sizeof(evconnection->buf), DWORD, raw_data_size);
    LOG(GLOBAL, LOG_TOP, 3, "datalen=%d data= %*s\n", raw_data_size, raw_data_size,
        raw_data);
    if (raw_data_size) {
        VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), raw_data,
                 raw_data_size); /* now the data */
        PADDING(&p, evconnection->buf + sizeof(evconnection->buf), raw_data_size,
                sizeof(DWORD));
    }

    /* FIXME: extra padding
       It seems like the server can handle more but not less padding */
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), "\0\0\0\0", 4);
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), "\0\0\0\0", 4);
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), "\0\0\0\0", 4);

    IF_X64(ASSERT_TRUNCATE(evconnection->request_length, int, p - evconnection->buf));
    evconnection->request_length = (int)(p - evconnection->buf);
    p = prepend_header(evconnection->buf, evconnection->buf + sizeof(evconnection->buf),
                       REPORT EVENTLOG, evconnection->request_length,
                       evconnection->message_seq, REPORT_UNKNOWN_HEADER);
    ASSERT(p);

    evconnection->message_seq++;
    evconnection->outlen =
        nt_pipe_transceive(evconnection->eventlog_pipe, evconnection->buf,
                           evconnection->request_length, evconnection->outbuf,
                           sizeof(evconnection->outbuf), DYNAMO_OPTION(eventlog_timeout));

    DOLOG(2, LOG_TOP, {
        LOG(GLOBAL, LOG_TOP, 3, "inlen= %d; outlen = " SZFMT "\n",
            evconnection->request_length, evconnection->outlen);
        LOG(GLOBAL, LOG_TOP, 3, "char report[] = ");
        print_buffer_as_bytes((byte *)evconnection->buf, evconnection->request_length);
        LOG(GLOBAL, LOG_TOP, 3, "char report_resp[] = ");
        print_buffer_as_bytes((byte *)evconnection->outbuf, evconnection->outlen);
        if (evconnection->outbuf[2] == '\3') {
            LOG(GLOBAL, LOG_TOP, 2, "//5 0 3 3 is bad news\n");
        }
    });

    /* the only expected message length, we're lenient on contents */
    if (evconnection->outlen != REPORT_RESPONSE_LENGTH) {
        LOG(GLOBAL, LOG_TOP, 1,
            "WARNING: Mismatch on REPORT_RESPONSE outlen=:" SZFMT "\n",
            evconnection->outlen);
        return 0;
    }

    return 1;
}

/* must hold the eventlog mutex */
uint
eventlog_deregister(eventlog_state_t *evconnection)
{
    char *p;
    int res;

    ASSERT_OWN_MUTEX(true, &evconnection->eventlog_mutex);

    p = evconnection->buf + HEADER_OFFSET;
    VARFIELD(&p, evconnection->buf + sizeof(evconnection->buf), evconnection->nonce,
             NONCE_LENGTH);
    IF_X64(ASSERT_TRUNCATE(evconnection->request_length, int, p - evconnection->buf));
    evconnection->request_length = (int)(p - evconnection->buf);
    p = prepend_header(evconnection->buf, evconnection->buf + sizeof(evconnection->buf),
                       REPORT EVENTLOG, evconnection->request_length,
                       evconnection->message_seq, DEREGISTER_UNKNOWN_HEADER);
    ASSERT(p);

    evconnection->outlen =
        nt_pipe_transceive(evconnection->eventlog_pipe, evconnection->buf,
                           evconnection->request_length, evconnection->outbuf,
                           sizeof(evconnection->outbuf), DYNAMO_OPTION(eventlog_timeout));

    if (evconnection->outlen != /*DE*/ REGISTER_RESPONSE_LENGTH) {
        LOG(GLOBAL, LOG_TOP, 1,
            "WARNING: Mismatch on DEREGISTER_RESPONSE outlen=" SZFMT "\n",
            evconnection->outlen);
    }

    if (evconnection->eventlog_completion) {
        close_handle(evconnection->eventlog_completion);
        evconnection->eventlog_completion = 0;
    }

    ASSERT(evconnection->eventlog_pipe);
    res = close_file(evconnection->eventlog_pipe);
    evconnection->eventlog_pipe = 0;
    return res;
}

/* Getting a new handle may be not very performant, and also may fail
 * at unexpected times we cache session state across messages and across
 * threads */
static eventlog_state_t *shared_eventlog_connection;
/* we use this if we have to syslog prior to heap being initialized */
static eventlog_state_t temp_shared_eventlog_connection;

/* separate so it can be called for pre-eventlog_init() syslogs */
static void
eventlog_alloc()
{
    /* Shouldn't come in here later when would need multi-thread synch.
     * Sometimes eventlog registration fails until post-init (for lsass, e.g.)
     * but the alloc should happen during init regardless.
     */
    ASSERT(!dynamo_initialized);
    if (shared_eventlog_connection != NULL &&
        shared_eventlog_connection != &temp_shared_eventlog_connection) {
        /* an early syslog was post-heap-init and we are fully initialized */
        return;
    }
    if (!dynamo_heap_initialized) {
        /* No heap available, so we use our temp static struct.
         * eventlog_init() will call this routine again and we'll copy
         * to the heap (else condition below).
         */
        ASSERT(shared_eventlog_connection == NULL);
        shared_eventlog_connection = &temp_shared_eventlog_connection;
        ASSIGN_INIT_LOCK_FREE(shared_eventlog_connection->eventlog_mutex, eventlog_mutex);
    } else {
        eventlog_state_t *alloc =
            HEAP_TYPE_ALLOC(GLOBAL_DCONTEXT, eventlog_state_t, ACCT_OTHER, PROTECTED);
        if (shared_eventlog_connection == &temp_shared_eventlog_connection) {
            /* transfer from the temp static structure to the heap */
            memcpy(alloc, &temp_shared_eventlog_connection,
                   sizeof(temp_shared_eventlog_connection));
        } else {
            memset(alloc, 0, sizeof(*shared_eventlog_connection));
            ASSIGN_INIT_LOCK_FREE(alloc->eventlog_mutex, eventlog_mutex);
        }
        shared_eventlog_connection = alloc;
    }
}

void
eventlog_init()
{
    uint res;

    /* TODO: Check a persistent (registry) counter for the current application
     whether to report to the system log on this run, decrement it if present */

    /* syslog_mask is dynamic, so even if 0 now we init in case it changes later */

    /* we call get computer name to make sure it's static buffer is initialized
     * while we are still single threaded */
    get_computer_name();

    /* FIXME: We don't actually get our own log as intended  */
    /* on error we just go in the Application EventLog */
    if (DYNAMO_OPTION(syslog_init) &&
        !init_registry_source()) { /* update registry keys */
        DOLOG_ONCE(1, LOG_TOP, {
            LOG(GLOBAL, LOG_TOP, 1,
                "WARNING: Could not add the event source registry keys."
                "Events are reported with no message files.\n");
        });
    }

    /* may have already been allocated for early syslogs */
    eventlog_alloc();

    d_r_mutex_lock(&shared_eventlog_connection->eventlog_mutex);
    if (!shared_eventlog_connection->eventlog_pipe) {
        /* initialize thread shared connection */
        res = eventlog_register(shared_eventlog_connection);
        if (!res) {
            LOG(GLOBAL, LOG_TOP, 1, "WARNING: Could not register event source.\n");
        }
    }
    d_r_mutex_unlock(&shared_eventlog_connection->eventlog_mutex);
}

void
eventlog_fast_exit(void)
{
    uint res = 1; /* maybe nothing to do */
    d_r_mutex_lock(&shared_eventlog_connection->eventlog_mutex);
    if (shared_eventlog_connection->eventlog_pipe)
        res = eventlog_deregister(shared_eventlog_connection);
    shared_eventlog_connection->eventlog_pipe = 0;
    d_r_mutex_unlock(&shared_eventlog_connection->eventlog_mutex);
    DOLOG(
        1, LOG_TOP, if (!res) {
            LOG(GLOBAL, LOG_TOP, 1, "WARNING: DeregisterEventSource failed.\n");
        });
}

void
eventlog_slow_exit()
{
    ASSERT_CURIOSITY(shared_eventlog_connection->eventlog_pipe == 0 &&
                     "call after eventlog_fast_exit");
    /* syslog_mask is dynamic, so even if 0 now we init in case it changes later */
    DELETE_LOCK(shared_eventlog_connection->eventlog_mutex);
    ASSERT(shared_eventlog_connection != NULL &&
           shared_eventlog_connection != &temp_shared_eventlog_connection);
    HEAP_TYPE_FREE(GLOBAL_DCONTEXT, shared_eventlog_connection, eventlog_state_t,
                   ACCT_OTHER, PROTECTED);
    /* try to let syslogs during later cleanup go through
     * FIXME: won't re-deregister in that case
     */
    shared_eventlog_connection = &temp_shared_eventlog_connection;
    ASSIGN_INIT_LOCK_FREE(shared_eventlog_connection->eventlog_mutex, eventlog_mutex);
}

/* writes a message to the Windows Event Log */
static void
os_eventlog(syslog_event_type_t priority, uint message_id, uint substitutions_num,
            char **arguments, size_t size_data, char *raw_data)
{
    WORD native_priority = 0;
    WORD category = 0; /* we don't use any */
    uint res = 0;

    /* check mask on event_type whether to log this type of message */
    if (!TEST(priority, dynamo_options.syslog_mask))
        return;

    switch (priority) {
    case SYSLOG_INFORMATION: native_priority = EVENTLOG_INFORMATION_TYPE; break;
    case SYSLOG_WARNING: native_priority = EVENTLOG_WARNING_TYPE; break;
    case SYSLOG_CRITICAL: /* report as error */
    case SYSLOG_ERROR: native_priority = EVENTLOG_ERROR_TYPE; break;
    default: ASSERT_NOT_REACHED();
    }

    if (shared_eventlog_connection == NULL)
        eventlog_alloc();
    d_r_mutex_lock(&shared_eventlog_connection->eventlog_mutex);
    if (!shared_eventlog_connection->eventlog_pipe) {
        /* Retry to open connection, since may have been unable
           to do that early on for system services started before EventLog */
        res = eventlog_register(shared_eventlog_connection);
        if (!res) {
            DOLOG_ONCE(1, LOG_TOP, {
                LOG(GLOBAL, LOG_TOP, 1,
                    "WARNING: Could not register event source on second attempt.\n");
            });
        } else {
            LOG(GLOBAL, LOG_TOP, 1,
                "Registered event source after program started. "
                "Events may be missing. --ok\n");
        }
    }

    if (shared_eventlog_connection->eventlog_pipe) {
        // TODO: add current user SID (thread maybe impersonated)
        res = eventlog_report(shared_eventlog_connection, (WORD)native_priority, category,
                              message_id, NULL, /* pSID */
                              (WORD)substitutions_num, size_data, arguments, raw_data);
    }
    d_r_mutex_unlock(&shared_eventlog_connection->eventlog_mutex);

    DOLOG(
        1, LOG_TOP, if (!res) {
            LOG(GLOBAL, LOG_TOP, 1, "WARNING: Could not report event 0x%x. \n",
                message_id);
        });
}
