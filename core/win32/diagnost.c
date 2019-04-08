/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
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

/* diagnost.c - maintains information about modules (dll or executable images) */

#include "../globals.h"

#include "ntdll.h"
#include "../fragment.h"
#include "../link.h"
#include "../utils.h"
#include "diagnost.h"
#include "os_private.h"
#include "../hotpatch.h"
#include "../moduledb.h"
#include "../module_shared.h"

/* Declared globally to reduce recursion overhead */
/* Not persistent across code cache execution, so not protected */
DECLARE_NEVERPROT_VAR(static DIAGNOSTICS_KEY_NAME_INFORMATION diagnostic_key_info, { 0 });
DECLARE_NEVERPROT_VAR(static DIAGNOSTICS_KEY_VALUE_FULL_INFORMATION diagnostic_value_info,
                      { 0 });
DECLARE_NEVERPROT_VAR(static char keyinfo_name[DIAGNOSTICS_MAX_KEY_NAME_SIZE], { 0 });
DECLARE_NEVERPROT_VAR(static char keyinfo_data[DIAGNOSTICS_MAX_NAME_AND_DATA_SIZE],
                      { 0 });
DECLARE_NEVERPROT_VAR(static wchar_t diagnostic_keyname[DIAGNOSTICS_MAX_KEY_NAME_SIZE],
                      { 0 });
DECLARE_NEVERPROT_VAR(static char optstring_buf[MAX_OPTIONS_STRING], { 0 });

/* Enforces unique access to shared reg data structures */
DECLARE_CXTSWPROT_VAR(static mutex_t reg_mutex, INIT_LOCK_FREE(diagnost_reg_mutex));

static const char *const separator =
    "-----------------------------------------------------------------------\n";

/* FIXME: The following is a list of relevant registry key entries as
 * reported by autoruns-8.53.  Since autorunsc -a only shows non-empty
 * keys, I've compiled this list by aggregating the output on several
 * different machines. Some keys may be missing...
 */
static const wchar_t *const HKLM_entries[] = {
    L"\\Registry\\Machine\\SOFTWARE\\Classes\\Folder\\Shellex\\ColumnHandlers",
    L"\\Registry\\Machine\\SOFTWARE\\Classes\\Protocols\\Filter",
    L"\\Registry\\Machine\\SOFTWARE\\Classes\\Protocols\\Handler",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Active Setup\\Installed Components",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Internet Explorer\\Extensions",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Internet Explorer\\Toolbar",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows\\"
    L"Appinit_Dlls",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
    L"Browser Helper Objects",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
    L"SharedTaskScheduler",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
    L"ShellExecuteHooks",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\"
    L"Explorer\\Run",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\"
    L"System",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnceEx",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
    L"Shell Extensions\\Approved",
    L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
    L"ShellServiceObjectDelayLoad",
    L"\\Registry\\Machine\\SOFTWARE\\Policies\\Microsoft\\Windows\\System\\Scripts\\"
    L"Logon",
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Lsa",
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors",
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Session Manager",
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Terminal Server\\Wds\\"
    L"rdpwd",
    L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services",
};

/* These are prefixed with \\Registry\\User\\<SSID>\\ in
 * report_autostart_programs()
 */
static const wchar_t *const HKCU_entries[] = {
    L"Control Panel\\Desktop",
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Desktop\\Components",
    L"SOFTWARE\\Microsoft\\Internet Explorer\\UrlSearchHooks",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows\\Load",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows\\Run",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System\\Shell",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
    L"SOFTWARE\\Policies\\Microsoft\\Windows\\System\\Scripts\\Logon",
};

/* Extracts and logs the data field of DIAGNOSTIC_KEY_VALUE_FULL_INFORMATION.
 * Only REG_SZ, REG_EXPAND_SZ (w/o expanding), REG_MULTI_SZ (just the first
 * string reported), REG_DWORD and REG_BINARY are currently supported.
 */
static void
diagnostics_log_data(IN file_t diagnostics_file, IN uint log_mask)
{
    uint i;

    ASSERT_OWN_MUTEX(true, &reg_mutex);
    if (log_mask & DIAGNOSTICS_REG_NAME) {
        /* there is no trailing null after name so don't print to far */
        snprintf(keyinfo_name, BUFFER_SIZE_ELEMENTS(keyinfo_name), "%.*S\n",
                 diagnostic_value_info.NameLength / sizeof(wchar_t),
                 (wchar_t *)&diagnostic_value_info.NameAndData);
        NULL_TERMINATE_BUFFER(keyinfo_name);
        print_xml_cdata(diagnostics_file, keyinfo_name);
    }

    if (log_mask & DIAGNOSTICS_REG_DATA) {
        if ((diagnostic_value_info.Type == REG_SZ) ||
            (diagnostic_value_info.Type == REG_EXPAND_SZ) ||
            (diagnostic_value_info.Type == REG_MULTI_SZ)) {
            snprintf(keyinfo_data, BUFFER_SIZE_ELEMENTS(keyinfo_data), "%S\n",
                     diagnostic_value_info.NameAndData +
                         diagnostic_value_info.DataOffset - DECREMENT_FOR_DATA_OFFSET);
            NULL_TERMINATE_BUFFER(keyinfo_data);
            print_xml_cdata(diagnostics_file, keyinfo_data);
        } else if (diagnostic_value_info.Type == REG_DWORD) {
            print_file(diagnostics_file, "0x%.8x\n",
                       *(uint *)(diagnostic_value_info.NameAndData +
                                 diagnostic_value_info.DataOffset -
                                 DECREMENT_FOR_DATA_OFFSET));
        } else if (diagnostic_value_info.Type == REG_BINARY) {
            for (i = diagnostic_value_info.DataOffset - DECREMENT_FOR_DATA_OFFSET;
                 (i < diagnostic_value_info.DataOffset - DECREMENT_FOR_DATA_OFFSET +
                      diagnostic_value_info.DataLength) &&
                 (i < DIAGNOSTICS_MAX_NAME_AND_DATA_SIZE);
                 i++) {
                print_file(diagnostics_file, "%.2x ",
                           diagnostic_value_info.NameAndData[i]);
                if ((i % DIAGNOSTICS_BYTES_PER_LINE) == 0)
                    print_file(diagnostics_file, "\n");
            }
            print_file(diagnostics_file, "\n");
        }
    }
}

/* Determines the location of the logging directory and create a new log file
 * that is sequentially higher than the previous log file.  The logging
 * directory is obtained from the produt settings in the registry, and the
 * file name is obtained by opening existing files until one is not found.
 */
static void
open_diagnostics_file(IN file_t *file, OUT char *buf, IN uint maxlen)
{
    const char *file_extension = DIAGNOSTICS_FILE_XML_EXTENSION;
    get_unique_logfile(file_extension, buf, maxlen, false, file);
}

/* flags */
#define PRINT_MEM_BUF_BYTE 0x1 /* print as byte values, default dword */

#define PRINT_MEM_BUF_START 0x2 /* print region starting at address */
/* default is to print region centered at address */

#define PRINT_MEM_BUF_NO_ALIGN 0x4 /* print exact region */
/* default extends to nice alignments */

#define PRINT_MEM_BUF_ASCII 0x8 /* print ASCII characters for each line */
/* applicable to either DWORD or BYTE mode */

/* Prints [-length/2, +length/2] from memory address (if readable) or, for
 * PRINT_MEM_BUF_START, prints [0, length] from memory address (if readable) */
static void
print_memory_buffer(file_t diagnostics_file, byte *address, uint length,
                    const char *label, uint flags)
{
    byte *start, *end;

    if (TEST(PRINT_MEM_BUF_START, flags)) {
        start = address;
        end = address + length;
    } else {
        start = address - length / 2;
        end = address + length / 2;
    }

    if (!TEST(PRINT_MEM_BUF_NO_ALIGN, flags)) {
        /* Align macros require power of 2 */
        ASSERT((DUMP_PER_LINE_DEFAULT & (DUMP_PER_LINE_DEFAULT - 1)) == 0);
        start = (byte *)ALIGN_BACKWARD(start, DUMP_PER_LINE_DEFAULT);
        end = (byte *)ALIGN_FORWARD(end, DUMP_PER_LINE_DEFAULT);
    }

    print_file(diagnostics_file, "%s: 0x%.8x\n", label, address);
    print_file(diagnostics_file, "<![CDATA[\n");
    while (start < end) {
        byte *cur_end = (byte *)MIN(ALIGN_FORWARD(start + 1, PAGE_SIZE), (ptr_uint_t)end);
        if (is_readable_without_exception(start, cur_end - start)) {
            dump_buffer_as_bytes(
                diagnostics_file, start, cur_end - start,
                DUMP_RAW | DUMP_ADDRESS |
                    (TEST(PRINT_MEM_BUF_BYTE, flags) ? 0 : DUMP_DWORD) |
                    (TEST(PRINT_MEM_BUF_ASCII, flags) ? DUMP_APPEND_ASCII : 0));
            print_file(diagnostics_file, "\n");
        } else {
            print_file(diagnostics_file, "Can\'t print 0x%.8x-0x%.8x (unreadable)\n",
                       start, cur_end);
        }
        start = cur_end;
    }
    print_file(diagnostics_file, "]]>\n");
}

static void
report_addr_info(file_t diagnostics_file, app_pc addr, const char *tag)
{
    /* FIXME: Add closest exported function from Vlad's code, when ready */
    /* This is only used for violations so safe to allocate memory, and
     * we need the full name so we can't just stick w/ the buffer
     */
    char modname_buf[MAX_MODNAME_INTERNAL];
    const char *mod_name = os_get_module_name_buf_strdup(
        addr, modname_buf, BUFFER_SIZE_ELEMENTS(modname_buf) HEAPACCT(ACCT_OTHER));
    print_file(diagnostics_file,
               "\t\taddress=             \"0x%.8x\"\n"
               "\t\tmodule=              \"%s\"\n"
               "\t\tin_IAT=              \"%s\"\n"
               "\t\tpreferred_base=      \"" PFX "\"\n",
               addr, mod_name == NULL ? "(none)" : mod_name,
               is_in_IAT(addr) ? "yes" : "no", get_module_preferred_base(addr));
    if (mod_name != NULL && mod_name != modname_buf)
        dr_strfree(mod_name HEAPACCT(ACCT_OTHER));
    mod_name = NULL;

    print_module_section_info(diagnostics_file, addr);

    /* Dump memory permission and region information */
    dump_mbi_addr(diagnostics_file, addr, DUMP_XML);

    /* Also dump 1 page around address */
    print_file(diagnostics_file, "\t\t><content>\n");
    print_memory_buffer(diagnostics_file, addr, (uint)PAGE_SIZE, tag,
                        PRINT_MEM_BUF_BYTE | PRINT_MEM_BUF_ASCII);
    print_file(diagnostics_file, "\t\t</content>\n");
}

static void
report_src_info(file_t diagnostics_file, dcontext_t *dcontext)
{
    const char *name = "source";
    fragment_t *f = dcontext->last_fragment;

    /* Note: last_fragment may span across different dlls because
     * DR's basic blocks span across unconditional direct branches.  If
     * recreate_app_pc() isn't used, current module may be wrong. Case 2152.
     * Make sure to check for LINKSTUB_FAKE() before recreating.
     * Note that last_exit can be NULL if DR is still initializing -- but if
     * we crash then we should not come here (will print conservative info only).
     */
    ASSERT(dcontext->last_exit != NULL);

    print_file(diagnostics_file, "\t<%s-properties \n", name);

    report_addr_info(diagnostics_file, f->tag, name);

    /* Dump src fragment information: flags, bb_tags, cache dump etc. */
    print_file(diagnostics_file,
               "\t\t<cache-content\n"
               "\t\t\tflags= \"0x%0x\"",
               f->flags);
    if (TEST(FRAG_IS_TRACE, f->flags)) {
        uint i = 0;
        trace_only_t *t = TRACE_FIELDS(f);
        if (t != NULL) {
            print_file(diagnostics_file, "\n\t\t\ttags=  \"");
            /* FIXME: all app tags are printed in one line, if it proves to
             * create unreadably long lines, change this
             */
            for (i = 0; i < t->num_bbs; i++)
                print_file(diagnostics_file, PFX " ", t->bbs[i].tag);
            print_file(diagnostics_file, "\"");
        } else
            ASSERT_CURIOSITY(false && "frag is trace, but no trace specific data?");
    }
    print_file(diagnostics_file, ">\n");

    /* For fake link stubs start_pc is null, for all other cases dump the
     * bb/trace
     */
    if (!LINKSTUB_FAKE(dcontext->last_exit)) {
        /* Print as raw bytes, just to be obscure and to not allocate any
         * memory (decoding does), from last_fragment start_pc to size
         */
        print_memory_buffer(diagnostics_file, dcontext->last_fragment->start_pc,
                            dcontext->last_fragment->size, "pc",
                            PRINT_MEM_BUF_BYTE | PRINT_MEM_BUF_START |
                                PRINT_MEM_BUF_NO_ALIGN);
    }

    print_file(diagnostics_file, "\t\t</cache-content>\n\t</%s-properties>\n", name);
}

static void
report_target_info(file_t diagnostics_file, dcontext_t *dcontext)
{
    const char *name = "target";
    print_file(diagnostics_file, "\t<%s-properties \n", name);
    report_addr_info(diagnostics_file, dcontext->next_tag, name);
    print_file(diagnostics_file, "\t</%s-properties>\n", name);
}

/* applies to any violation though in most cases expected to provide
 * extra information for covering RCT (.C .E .F) or .R failures.
 * (Note we don't report on other preferred targets in DLLs
 * rebased due to other conflicts)
 */
static void
report_preferred_target_info(file_t diagnostics_file, dcontext_t *dcontext)
{
    const char *name = "preferred-target";
    app_pc aslr_preferred_address = aslr_possible_preferred_address(dcontext->next_tag);

    /* no report if we don't have a preferred address */
    if (aslr_preferred_address == NULL) {
        return;
    }

    print_file(diagnostics_file, "\t<%s-properties \n", name);
    report_addr_info(diagnostics_file, aslr_preferred_address, name);
    print_file(diagnostics_file, "\t</%s-properties>\n", name);
}

static void
report_vm_counters(file_t diagnostics_file, VM_COUNTERS *vmc)
{
    print_file(diagnostics_file,
               "<vm-counters>\n"
               "%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
               "%.10d %.10d %.10d %.10d\n"
               "</vm-counters>\n",
               vmc->PeakVirtualSize, vmc->VirtualSize, vmc->PageFaultCount,
               vmc->PeakWorkingSetSize, vmc->WorkingSetSize, vmc->QuotaPeakPagedPoolUsage,
               vmc->QuotaPagedPoolUsage, vmc->QuotaPeakNonPagedPoolUsage,
               vmc->QuotaNonPagedPoolUsage, vmc->PagefileUsage, vmc->PeakPagefileUsage);
}

/* Prints out dcontext information.  If the dcontext is from the current thread,
 * additional information is reported.
 * The conservative flag indicates we may have come here from a crash.  Print
 * information that do not need any allocations etc.
 */
static void
report_dcontext_info(IN file_t diagnostics_file, IN dcontext_t *dcontext,
                     IN bool conservative)
{

    byte *start_ptr = NULL;

    if (dcontext == NULL) { /* see case 8830 - dcontext can be NULL! */
        print_file(diagnostics_file, "\tNo thread specific data available\n");
        return;
    }

    print_file(diagnostics_file, "\t<whereami> %d </whereami>\n", dcontext->whereami);
    dump_mcontext(get_mcontext(dcontext), diagnostics_file, DUMP_XML);
    dump_callstack(NULL, (app_pc)get_mcontext(dcontext)->xbp, diagnostics_file, DUMP_XML);

    if (dcontext == get_thread_private_dcontext()) {
        if (!conservative) {
            /* Print out additional current thread information */
            report_src_info(diagnostics_file, dcontext);
            report_target_info(diagnostics_file, dcontext);
            /* We print both really targeted address and the contents of the
             * module potentially targeted by an attack thwarted by ASLR.
             */
            report_preferred_target_info(diagnostics_file, dcontext);
        }

        /* Dump 1 page before/after ESP, making no assumptions about EBP. */
        /* Verify beginning and end of region (which span at most 2 pages) */
        print_file(diagnostics_file, "\t<stack>\n\t\t<content>\n",
                   (byte *)get_mcontext(dcontext)->xsp);
        print_memory_buffer(diagnostics_file, (byte *)get_mcontext(dcontext)->xsp,
                            (uint)PAGE_SIZE, "Current Stack", PRINT_MEM_BUF_ASCII);
        print_file(diagnostics_file, "\t\t</content>\n\t</stack>\n");
    } else {
        /* Dump a mini-stack */
        print_file(diagnostics_file, "\t<stack>\n\t\t<content>\n",
                   (byte *)get_mcontext(dcontext)->xsp);
        print_memory_buffer(diagnostics_file, (byte *)get_mcontext(dcontext)->xsp,
                            DIAGNOSTICS_MINI_DUMP_SIZE, "Stack", PRINT_MEM_BUF_ASCII);
        print_file(diagnostics_file, "\t\t</content>\n\t</stack>\n");
    }
}

/* Collects and displays all DynamoRIO data structures that provide useful
 * diagnostic information.
 * violation_type of NO_VIOLATION_* is diagnostics; other is forensics.
 */
static void
report_internal_data_structures(IN file_t diagnostics_file,
                                IN security_violation_t violation_type)
{
    dcontext_t *dcontext;

    print_file(diagnostics_file,
               "<internal-data-structures>\n"
               "automatic_startup  : %d\ncontrol_all_threads: %d\n"
               "dynamo_initialized : %d\ndynamo_exited      : %d\n"
               "num_threads        : %d\ndynamorio.dll      = " PFX "\n",
               automatic_startup, control_all_threads, dynamo_initialized, dynamo_exited,
               d_r_get_num_threads(), get_dynamorio_dll_start());

    /* skip for non-attack calls to avoid risk of any global locks */
    if (violation_type != NO_VIOLATION_BAD_INTERNAL_STATE) {
        print_vmm_heap_data(diagnostics_file);
        if (dynamo_initialized && !DYNAMO_OPTION(thin_client)) { /* case 8830 */
            print_file(diagnostics_file, "Exec areas:\n");
            print_executable_areas(diagnostics_file);
#ifdef PROGRAM_SHEPHERDING
            print_file(diagnostics_file, "Future exec areas:\n");
            print_futureexec_areas(diagnostics_file);
#endif
            print_moduledb_exempt_lists(diagnostics_file);
        }
    }

    print_last_deallocated(diagnostics_file);

    /* case 5442: always dump dcontext */
    dcontext = get_thread_private_dcontext();
    if (dcontext != NULL) {
        print_memory_buffer(diagnostics_file, (byte *)dcontext, sizeof(dcontext_t),
                            "current dcontext",
                            PRINT_MEM_BUF_START | PRINT_MEM_BUF_NO_ALIGN);
    }

    /* Include mini-call stacks for non-attack calls */
    if (violation_type == NO_VIOLATION_BAD_INTERNAL_STATE ||
        violation_type == NO_VIOLATION_OK_INTERNAL_STATE) {
        app_pc our_esp = NULL, our_ebp = NULL;
        print_file(diagnostics_file, "\nCall stack for DR:\n");
        dump_dr_callstack(diagnostics_file);
        GET_STACK_PTR(our_esp);
        GET_FRAME_PTR(our_ebp);
        /* dump whole stack */
        print_file(diagnostics_file, "ebp=" PFX " esp=" PFX "\n", our_ebp, our_esp);
        print_memory_buffer(diagnostics_file, (byte *)ALIGN_BACKWARD(our_esp, PAGE_SIZE),
                            3 * (uint)PAGE_SIZE, "DR Stack", PRINT_MEM_BUF_START);
        /* application call stack is printed by report_dcontext_info */
    }
#ifdef HOT_PATCHING_INTERFACE
    /* As long as hotp_diagnostics is turned on dump diagnostics about
     * hotpatches.
     */
    if (DYNAMO_OPTION(hotp_diagnostics)) {
        hotp_print_diagnostics(diagnostics_file);
    }
#endif

    d_r_mutex_lock(&reg_mutex);
    get_dynamo_options_string(&dynamo_options, optstring_buf,
                              BUFFER_SIZE_ELEMENTS(optstring_buf), true);
    NULL_TERMINATE_BUFFER(optstring_buf);
    print_file(diagnostics_file, "option string = \"%s\"\n", optstring_buf);
    d_r_mutex_unlock(&reg_mutex);

    DOLOG(1, LOG_ALL, {
        uchar test_buf[UCHAR_MAX + 2];
        uint i;
        print_file(diagnostics_file, "<debug_xml_encoding_test>\n<![CDATA[\n");
        /* test CDATA escaping routines */
        print_xml_cdata(diagnostics_file, "testing premature ending ]]> for cdata\n");
        /* test encoding */
        for (i = 0; i <= UCHAR_MAX; i++) {
            test_buf[i] = (uchar)i;
            if (!is_valid_xml_char((char)i)) {
                ASSERT(i < 0x20 && i != '\n' && i != '\r' && i != '\t');
                test_buf[i] = 'a';
            }
        }
        BUFFER_LAST_ELEMENT(test_buf) = '\n';
        os_write(diagnostics_file, test_buf, sizeof(test_buf));
        for (test_buf[0] = 'a', i = 1; i <= UCHAR_MAX; i++)
            test_buf[i] = (uchar)i;
        NULL_TERMINATE_BUFFER(test_buf);
        print_xml_cdata(diagnostics_file, (const char *)test_buf);
        print_file(diagnostics_file, "\n]]>\n</debug_xml_encoding_test>\n");
    });

    print_file(diagnostics_file, "</internal-data-structures>\n");
}

/* Collects and displays information about NTDLL.DLL.  Finds the SystemRoot
 * registry key & adds "System32" to it to find NTDLL.DLL.
 */
static void
report_ntdll_info(IN file_t diagnostics_file)
{

    reg_query_value_result_t value_result;
    FILE_NETWORK_OPEN_INFORMATION file_info;
    wchar_t filename[MAXIMUM_PATH + 1];

    print_file(diagnostics_file, "<ntdll-file-information><![CDATA[\n");

    memset(&file_info, 0, sizeof(file_info));
    value_result = reg_query_value(DIAGNOSTICS_OS_REG_KEY, DIAGNOSTICS_SYSTEMROOT_REG_KEY,
                                   KeyValueFullInformation, &diagnostic_value_info,
                                   sizeof(diagnostic_value_info), 0);
    if (value_result == REG_QUERY_SUCCESS) {
        _snwprintf(filename, BUFFER_SIZE_ELEMENTS(filename), L"\\??\\%s\\%s",
                   (wchar_t *)(diagnostic_value_info.NameAndData +
                               diagnostic_value_info.DataOffset -
                               DECREMENT_FOR_DATA_OFFSET),
                   DIAGNOSTICS_NTDLL_DLL_LOCATION);
        NULL_TERMINATE_BUFFER(filename);
        print_file(diagnostics_file, "%S\n", filename);

        if (query_full_attributes_file(filename, &file_info)) {
            print_file(diagnostics_file,
                       "0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n"
                       "0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n"
                       "0x%.8x\n",
                       file_info.CreationTime.HighPart, file_info.CreationTime.LowPart,
                       file_info.LastAccessTime.HighPart,
                       file_info.LastAccessTime.LowPart, file_info.LastWriteTime.HighPart,
                       file_info.LastWriteTime.LowPart, file_info.ChangeTime.HighPart,
                       file_info.ChangeTime.LowPart, file_info.AllocationSize.HighPart,
                       file_info.AllocationSize.LowPart, file_info.EndOfFile.HighPart,
                       file_info.EndOfFile.LowPart, file_info.FileAttributes);
        }
    }

    print_file(diagnostics_file, "]]></ntdll-file-information>\n\n");
}

/* conservative flag indicates we may have come here from a crash.  Print
 * information that do not need any allocations etc. */
static void
report_thread(file_t diagnostics_file, int num, thread_id_t id, dcontext_t *dcontext,
              bool conservative)
{
    print_file(diagnostics_file,
               "\n<thread id=\"%d\" current-thread=\"%s\" num=\"%d\">\n", id,
               ((dcontext == get_thread_private_dcontext()) ? "yes" : "no"), num + 1);
    report_dcontext_info(diagnostics_file, dcontext, conservative);
    print_file(diagnostics_file, "</thread>\n");
}

/* Displays process-specific information for the current process.
 * The conservative flag indicates we may have come here from a crash.  Print
 * information that does not need any allocations, etc.
 */
static void
report_current_process(IN file_t diagnostics_file, IN PSYSTEM_PROCESSES sp,
                       IN security_violation_t violation_type, IN bool conservative)
{
    PEB *peb = get_own_peb();
    thread_record_t **threads;
    int i, num_threads;
    size_t s, buffer_length;
    wchar_t *buffer;
    bool couldbelinking = false;
    bool report_thread_list = true;

    print_file(diagnostics_file, "<current-process\n");

    ASSERT(conservative || sp != NULL);

    /* FIXME: There are several in-memory depedencies on strings that could
       be used in an attack.  Risk should be assessed. */
    if (conservative) {
        print_file(diagnostics_file, "name=                    \"%s\"\n",
                   get_application_name());
    } else {
        print_file(diagnostics_file, "name=                    \"%S\"\n",
                   sp->ProcessName.Buffer);
    }
    print_file(diagnostics_file, "image-path=              \"%S\"\n",
               peb->ProcessParameters->ImagePathName.Buffer);
    print_file(diagnostics_file, "full-qualified-name=     \"%s\"\n",
               get_application_name());
    print_file(diagnostics_file, "short-qualified-name=    \"%S\"\n",
               get_own_short_qualified_name());
    print_file(diagnostics_file, "current-directory-path=  \"%S\"\n",
               peb->ProcessParameters->CurrentDirectoryPath.Buffer);
    if (conservative) {
        print_file(diagnostics_file, "process-id=              \"%s\"\n",
                   get_application_pid());
    } else {
        print_file(diagnostics_file, "process-id=              \"%d\"\n", sp->ProcessId);
    }
    print_file(diagnostics_file,
               "being-debugged=          \"%s\"\n"
               "image-base-address=      \"0x%.8x\"\n",
               peb->BeingDebugged ? "yes" : "no", peb->ImageBaseAddress);
    print_file(diagnostics_file, "shell-info=              \"%S\"\n",
               peb->ProcessParameters->ShellInfo.Buffer);
    /* This can be NULL sometimes */
    print_file(diagnostics_file, "runtime-info=            \"%S\"\n",
               peb->ProcessParameters->RuntimeData.Buffer == NULL
                   ? L"(null)"
                   : peb->ProcessParameters->RuntimeData.Buffer);
    print_file(diagnostics_file, "console-flags=           \"0x%.8x\"\n",
               peb->ProcessParameters->ConsoleFlags);
    if (!conservative) {
        print_file(diagnostics_file,
                   "thread-count=            \"%d\"\n"
                   "handle-count=            \"%d\"\n"
                   "base-priority=           \"%d\"\n"
                   "creation-time=           \"0x%.8x%.8x\"\n"
                   "user-time=               \"0x%.8x%.8x\"\n"
                   "kernel-time=             \"0x%.8x%.8x\"\n",
                   sp->ThreadCount, sp->HandleCount, sp->BasePriority,
                   sp->CreateTime.HighPart, sp->CreateTime.LowPart, sp->UserTime.HighPart,
                   sp->UserTime.LowPart, sp->KernelTime.HighPart, sp->KernelTime.LowPart);
    }

    /* note cmdline is sometimes already quoted and sometimes not, to avoid
     * problems for xml we dump it as a separate CDATA tag instead of an
     * attribute */
    print_file(diagnostics_file, "><command-line><![CDATA[ %S\n]]></command-line>\n",
               peb->ProcessParameters->CommandLine.Buffer);

    /* DllPath can get pretty large -- splitting it up here.
     * FIXME: Splitting buffers can be generalized fairly simply (1 for wide,
     * 1 for ascii) if this kind of thing happens a lot */
    /* For xml can't be done as an additional in tag field since I've seen
     * quotes in the dll-path string. */
    print_file(diagnostics_file, "<dll-path><![CDATA[      ");
    buffer = peb->ProcessParameters->DllPath.Buffer;
    buffer_length = wcslen(peb->ProcessParameters->DllPath.Buffer);
    /* MAX_LOG_LENGTH_MINUS_ONE allows a \0 to be appended without overflow */
    ASSERT(MAX_LOG_LENGTH_MINUS_ONE == MAX_LOG_LENGTH - 1);
    for (s = 0; s < buffer_length; s += MAX_LOG_LENGTH_MINUS_ONE) {
        print_file(diagnostics_file, "%." STRINGIFY(MAX_LOG_LENGTH_MINUS_ONE) "S",
                   buffer + s);
    }
    print_file(diagnostics_file, "\n]]></dll-path>\n");

    if (conservative) {
        /* Can be called while unstable, don't allocate memory for mem dynamically */
        VM_COUNTERS mem;
        if (get_process_mem_stats(NT_CURRENT_PROCESS, &mem))
            report_vm_counters(diagnostics_file, &mem);
    } else {
        report_vm_counters(diagnostics_file, &sp->VmCounters);
        print_file(diagnostics_file,
                   "<io-counters>\n"
                   "0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n"
                   "0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n"
                   "</io-counters>\n\n",
                   sp->IoCounters.ReadOperationCount.HighPart,
                   sp->IoCounters.ReadOperationCount.LowPart,
                   sp->IoCounters.WriteOperationCount.HighPart,
                   sp->IoCounters.WriteOperationCount.LowPart,
                   sp->IoCounters.OtherOperationCount.HighPart,
                   sp->IoCounters.OtherOperationCount.LowPart,
                   sp->IoCounters.ReadTransferCount.HighPart,
                   sp->IoCounters.ReadTransferCount.LowPart,
                   sp->IoCounters.WriteTransferCount.HighPart,
                   sp->IoCounters.WriteTransferCount.LowPart,
                   sp->IoCounters.OtherTransferCount.HighPart,
                   sp->IoCounters.OtherTransferCount.LowPart);
    }

    /* Print out DLL information */
    /* FIXME: walking the loader data structures at arbitrary points is dangerous
     * due to data races with other threads -- see is_module_being_initialized
     * and get_module_name
     */
    print_modules_ldrlist_and_ourlist(diagnostics_file, DUMP_XML, conservative);

    /* Print out all thread information */
    print_file(diagnostics_file, "\n<thread-list>");

    if (is_self_couldbelinking()) {
        /* case 6093: we can 3-way deadlock w/ a flusher and a thread wanting
         * the bb building lock if we come here holding it (.B/.A violation).
         * FIXME: as a short-term fix we do not print the list of all threads.
         * case 6141 covers re-enabling.
         */
        report_thread_list = false;

        if (report_thread_list) {
            /* cannot grab thread_initexit_lock if couldbelinking since could deadlock
             * with a flushing thread, so we go nolinking for the thread snapshot
             */
            enter_nolinking(get_thread_private_dcontext(), NULL,
                            false /*not a cache transition*/);
        }
        couldbelinking = true;
    }
#if defined(PROGRAM_SHEPHERDING) && defined(HOT_PATCHING_INTERFACE)
    /* case 7528: hotp violations are nolinking yet hold the read lock
     * when reporting.  our solution for now is to not list the threads,
     * which we're already not doing for other violations (case 6093).
     * case 6141 covers re-enabling.
     */
    if (violation_type == HOT_PATCH_DETECTOR_VIOLATION ||
        violation_type == HOT_PATCH_PROTECTOR_VIOLATION ||
        violation_type == HOT_PATCH_FAILURE)
        report_thread_list = false;
#endif
#if defined(PROGRAM_SHEPHERDING) && defined(GBOP) /* xref case 7960. */
    if (violation_type == GBOP_SOURCE_VIOLATION)
        report_thread_list = false;
#endif
    if (violation_type == ASLR_TARGET_VIOLATION) {
        /* we should in fact be able to report the thread list,
         * if it wasn't the ASSERT, and to keep things mostly same
         */
        report_thread_list = false;
    }
    if (violation_type == APC_THREAD_SHELLCODE_VIOLATION) {
        report_thread_list = false;
    }
#if defined(PROGRAM_SHEPHERDING) && defined(PROCESS_CONTROL)
    if (violation_type == PROCESS_CONTROL_VIOLATION)
        report_thread_list = false;
#endif

    if (conservative) {
        /* cannot call malloc, don't list all threads */
        report_thread_list = false;
    }

    /* we do not support acquiring the thread_initexit_lock for any
     * violations.  case 6141 covers re-enabling.  see also
     * FORENSICS_ACQUIRES_INITEXIT_LOCK in vmareas.c.
     */
    ASSERT(!report_thread_list || violation_type >= 0 /*non-violation*/);
    if (report_thread_list) {
        d_r_mutex_lock(&thread_initexit_lock);
        get_list_of_threads(&threads, &num_threads);
        for (i = 0; i < num_threads; i++) {
            if (threads[i]->dcontext != NULL) {
                report_thread(diagnostics_file, i, threads[i]->id, threads[i]->dcontext,
                              conservative);
            }
        }
        d_r_mutex_unlock(&thread_initexit_lock);
        if (couldbelinking) {
            enter_couldbelinking(get_thread_private_dcontext(), NULL,
                                 false /*not a cache transition*/);
        }
        global_heap_free(
            threads, num_threads * sizeof(thread_record_t *) HEAPACCT(ACCT_THREAD_MGT));
    } else {
        report_thread(diagnostics_file, 0, d_r_get_thread_id(),
                      get_thread_private_dcontext(), conservative);
    }

    print_file(diagnostics_file, "</thread-list>\n</current-process>\n\n");
}

/* Using the NtQuerySystemInformation() system call, the
 * SystemProcessesAndThreadsInformation structure is filled in.
 * Since the structure is of variable size, repeated calls to
 * NtQuerySystemInformation() are made until the buffer is big
 * enough to hold all the information.  This also explains why the
 * returned pointer is a byte * rather than PSYSTEM_PROCESSES.  The
 * calling function can cast the buffer to PSYSTEM_PROCESSES for
 * each process chained by the NextEntryDelta field.
 */
byte *
get_system_processes(OUT uint *info_bytes_needed)
{
    NTSTATUS result;
    byte *process_info;

    *info_bytes_needed = sizeof(SYSTEM_PROCESSES);
    /* FIXME: Not ideal to dynamically allocate memory in unstable situation. */
    process_info = (byte *)global_heap_alloc(*info_bytes_needed HEAPACCT(ACCT_OTHER));
    memset(process_info, 0, *info_bytes_needed);
    do {
        result = query_system_info(SystemProcessesAndThreadsInformation,
                                   *info_bytes_needed, process_info);
        if (result == STATUS_INFO_LENGTH_MISMATCH) {
            global_heap_free(process_info, *info_bytes_needed HEAPACCT(ACCT_OTHER));
            *info_bytes_needed = (*info_bytes_needed) * 2;
            process_info =
                (byte *)global_heap_alloc(*info_bytes_needed HEAPACCT(ACCT_OTHER));
            memset(process_info, 0, *info_bytes_needed);
        }
    } while (result == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(result))
        return NULL;

    return process_info;
}

/* Collects and displays all process information.  First displays all process
 * names.  Then displays additional information for the current process
 */
static void
report_processes(IN file_t diagnostics_file, IN security_violation_t violation_type)
{
    byte *process_info = NULL;
    byte *next_process = NULL;
    PSYSTEM_PROCESSES sp = NULL;
    uint info_bytes_needed;
    bool found_current_process;
    bool found_last_process;

    /* We use byte * for process_info because
     * SystemProcessesAndThreadsInformation is variable,
     * each entry is cast to a SYSTEM_PROCESSES prior to access */
    process_info = get_system_processes(&info_bytes_needed);

    if (process_info != NULL) {
        /* Initialize to first process */
        next_process = process_info;
        sp = (PSYSTEM_PROCESSES)next_process;
        found_last_process = false;

        print_file(diagnostics_file, "<process-list> <![CDATA[ \n");

        /* Print out all process names here */
        do {
            /* A NextEntryDelta of 0 indicates the last process in the structure */
            if (sp->NextEntryDelta == 0)
                found_last_process = true;
            if (sp->ProcessName.Buffer != NULL) {
                print_file(diagnostics_file, "%S\n", sp->ProcessName.Buffer);
            }
            next_process = (byte *)(next_process + sp->NextEntryDelta);
            sp = (PSYSTEM_PROCESSES)next_process;
        } while (found_last_process == false);

        print_file(diagnostics_file, "]]> </process-list>\n\n");

        /* Initialize to first process */
        next_process = process_info;
        sp = (PSYSTEM_PROCESSES)next_process;
        found_last_process = false;
        found_current_process = false;

        /* Print out current process info */
        do {
            /* A NextEntryDelta of 0 indicates the last process in the structure */
            if (sp->NextEntryDelta == 0)
                found_last_process = true;
            if (is_pid_me((process_id_t)sp->ProcessId)) {
                found_current_process = true;
                report_current_process(diagnostics_file, sp, violation_type,
                                       false /*not conservative*/);
            }
            next_process = (byte *)(next_process + *(uint *)next_process);
            sp = (PSYSTEM_PROCESSES)next_process;
        } while ((found_last_process == false) && (found_current_process == false));

        global_heap_free(process_info, info_bytes_needed HEAPACCT(ACCT_OTHER));
    }
}

/* forward decl */
static void
report_registry_settings_helper(file_t diagnostics_file, uint log_mask, uint *total_keys,
                                uint *recursion_level);

/* Collects and displays all diagnostic information collected from the
 * registry key 'keyname'. Recursively walk subkeys and values up to
 * DIAGNOSTICS_MAX_RECURSION_LEVEL depth. No more then
 * DIAGNOSTICS_MAX_REG_KEYS will be investigated in this way.
 */
static void
report_registry_settings(IN file_t diagnostics_file, IN wchar_t *keyname,
                         IN uint log_mask)
{
    uint recursion_level = 0;
    uint total_keys = 0;
    ASSERT_OWN_MUTEX(true, &reg_mutex);
    wcsncpy(diagnostic_keyname, keyname, BUFFER_SIZE_ELEMENTS(diagnostic_keyname));
    report_registry_settings_helper(diagnostics_file, log_mask, &total_keys,
                                    &recursion_level);
}

/* static buffer diagnostic_keyname holds the registry key to output
 * information for, report_registry_settings modifies it in place and
 * recursively calls itself to walk the subkeys. total_keys and recursion_level
 * are used to bound the max number of keys walked and the max recursion depth
 * respectively. */
static void
report_registry_settings_helper(file_t diagnostics_file, uint log_mask, uint *total_keys,
                                uint *recursion_level)
{
    int current_enum_key = 0;
    int current_enum_value = 0;
    int key_result;

    if (*recursion_level == 0) {
        *total_keys = 0;
        print_file(diagnostics_file, "%sRegistry Settings\n%S\n%s", separator,
                   diagnostic_keyname, separator);
    }
    *total_keys = *total_keys + 1;

    memset(&diagnostic_value_info, 0, sizeof(diagnostic_value_info));

    if (log_mask & DIAGNOSTICS_REG_ALLKEYS) {
        int value_result;
        print_file(diagnostics_file, "%S\n\n", diagnostic_keyname);
        current_enum_value = 0;
        do {
            value_result = reg_enum_value(diagnostic_keyname, current_enum_value,
                                          KeyValueFullInformation, &diagnostic_value_info,
                                          sizeof(diagnostic_value_info));
            if (value_result) {
                diagnostics_log_data(diagnostics_file, log_mask);
                print_file(diagnostics_file, "\n");
            }
            current_enum_value++;
        } while ((value_result) && (current_enum_value < DIAGNOSTICS_MAX_REG_VALUES));
    } else if (log_mask & DIAGNOSTICS_REG_HARDWARE) {
        reg_query_value_result_t value_result;
        value_result = reg_query_value(diagnostic_keyname, DIAGNOSTICS_DESCRIPTION_KEY,
                                       KeyValueFullInformation, &diagnostic_value_info,
                                       sizeof(diagnostic_value_info), 0);
        if (value_result == REG_QUERY_SUCCESS) {
            diagnostics_log_data(diagnostics_file, log_mask);

            /* Try to get the manufacturer */
            value_result = reg_query_value(
                diagnostic_keyname, DIAGNOSTICS_MANUFACTURER_KEY, KeyValueFullInformation,
                &diagnostic_value_info, sizeof(diagnostic_value_info), 0);
            if (value_result == REG_QUERY_SUCCESS)
                diagnostics_log_data(diagnostics_file, log_mask);
            /* Try to get the friendly name */
            value_result = reg_query_value(
                diagnostic_keyname, DIAGNOSTICS_FRIENDLYNAME_KEY, KeyValueFullInformation,
                &diagnostic_value_info, sizeof(diagnostic_value_info), 0);
            if (value_result == REG_QUERY_SUCCESS)
                diagnostics_log_data(diagnostics_file, log_mask);

            print_file(diagnostics_file, "\n");
        }
    }

    /* See if there are more subkeys to recursively call */
    /* Re-using the same diagnostic_key_info structure to reduce stack overhead */
    if (log_mask & DIAGNOSTICS_REG_ALLSUBKEYS) {
        do {
            memset(&diagnostic_key_info, 0, sizeof(diagnostic_key_info));
            key_result =
                reg_enum_key(diagnostic_keyname, current_enum_key, KeyBasicInformation,
                             &diagnostic_key_info, sizeof(diagnostic_key_info));
            current_enum_key++;

            if ((*recursion_level < DIAGNOSTICS_MAX_RECURSION_LEVEL) &&
                (*total_keys < DIAGNOSTICS_MAX_REG_KEYS) && (key_result)) {
                size_t index = wcslen(diagnostic_keyname);

                /* append subkey name */
                _snwprintf(&diagnostic_keyname[index],
                           BUFFER_SIZE_ELEMENTS(diagnostic_keyname) - index, L"\\%s",
                           diagnostic_key_info.Name);
                NULL_TERMINATE_BUFFER(diagnostic_keyname);

                *recursion_level = *recursion_level + 1;
                report_registry_settings_helper(diagnostics_file, log_mask, total_keys,
                                                recursion_level);
                *recursion_level = *recursion_level - 1;

                /* remove subkey name */
                diagnostic_keyname[index] = L'\0';
            }
        } while ((*recursion_level < DIAGNOSTICS_MAX_RECURSION_LEVEL) &&
                 (*total_keys < DIAGNOSTICS_MAX_REG_KEYS) && (key_result));
    }
}

static void
report_autostart_programs(file_t diagnostics_file)
{
    int i;
    wchar_t sid[DIAGNOSTICS_MAX_KEY_NAME_SIZE];
    NTSTATUS result;

    ASSERT_OWN_MUTEX(true, &reg_mutex);
    print_file(diagnostics_file, "<autostart-programs>\n<![CDATA[\n");

    /* HKEY_LOCAL_MACHINE entries */
    for (i = 0; i < BUFFER_SIZE_ELEMENTS(HKLM_entries); i++) {
        report_registry_settings(diagnostics_file, (wchar_t *)HKLM_entries[i],
                                 (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_ALLSUBKEYS |
                                  DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    }

    /* HKEY_CURRENT_USER entries */
    result = get_current_user_SID(sid, sizeof(sid));
    if (NT_SUCCESS(result)) {
        wchar_t entry[DIAGNOSTICS_MAX_KEY_NAME_SIZE];

        for (i = 0; i < BUFFER_SIZE_ELEMENTS(HKCU_entries); i++) {
            _snwprintf(entry, BUFFER_SIZE_ELEMENTS(entry), L"\\Registry\\User\\%s\\%s",
                       sid, HKCU_entries[i]);
            NULL_TERMINATE_BUFFER(entry);

            report_registry_settings(diagnostics_file, entry,
                                     (DIAGNOSTICS_REG_ALLKEYS |
                                      DIAGNOSTICS_REG_ALLSUBKEYS | DIAGNOSTICS_REG_NAME |
                                      DIAGNOSTICS_REG_DATA));
        }
    } else {
        ASSERT(false && "query of current user's SID failed");
    }

    print_file(diagnostics_file, "]]>\n</autostart-programs>\n\n");
}

/* Displays diagnostic intro. */
static void
report_intro(IN file_t diagnostics_file, IN const char *message,
             IN const char *name /* NULL if not a violation */)
{
    static const char *months[] = { "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    SYSTEMTIME st;
    query_system_time(&st);

    /* make sure index to months array is okay */
    if (st.wMonth < 1 || st.wMonth > 12) {
        ASSERT(false && "query_system_time() returning bad month");
        st.wMonth = 0;
    }

    print_file(diagnostics_file,
               "\n<diagnostic-report>\n"
               "<date> %s %d, %d </date>\n"
               "<time> %.2d:%.2d:%.2d.%.3d GMT </time>\n",
               months[st.wMonth], st.wDay, st.wYear, st.wHour, st.wMinute, st.wSecond,
               st.wMilliseconds);

    /* FIXME - can message be long enough that this runs into our buffer length
     * limits? Could write message as a direct file write. */
    print_file(diagnostics_file,
               "<description> <![CDATA[[ \n"
               "Generated by " PRODUCT_NAME " %s, %s\n%s\n"
               "]]> </description>\n",
               VERSION_NUMBER_STRING, BUILD_NUMBER_STRING, message);

    if (name != NULL) {
        print_file(diagnostics_file, "<threat-id> Threat ID: %s </threat-id>\n", name);
    }

    print_file(diagnostics_file, "</diagnostic-report>\n\n");
}

static void
report_processor_info(file_t diagnostics_file)
{
    features_t *features;

    print_file(diagnostics_file, "<processor-information\n");

    print_file(diagnostics_file, "Brand=        \"%s\"\n", proc_get_brand_string());

    print_file(diagnostics_file, "Type=         \"0x%x\"\n", proc_get_type());

    print_file(diagnostics_file, "Family=       \"0x%x\"\n", proc_get_family());

    print_file(diagnostics_file, "Model=        \"0x%x\"\n", proc_get_model());

    print_file(diagnostics_file, "Stepping=     \"0x%x\"\n", proc_get_stepping());

    print_file(diagnostics_file, "L1_icache=    \"%s\"\n",
               proc_get_cache_size_str(proc_get_L1_icache_size()));

    print_file(diagnostics_file, "L1_dcache=    \"%s\"\n",
               proc_get_cache_size_str(proc_get_L1_dcache_size()));

    print_file(diagnostics_file, "L2_cache=     \"%s\"\n",
               proc_get_cache_size_str(proc_get_L2_cache_size()));

    features = proc_get_all_feature_bits();
    print_file(diagnostics_file, "Feature_bits= \"%.8x %.8x %.8x %.8x\"\n",
               features->flags_edx, features->flags_ecx, features->ext_flags_edx,
               features->ext_flags_ecx);

    print_file(diagnostics_file, "/>\n");
}

/* Collects and displays all system diagnostic information. */
static void
report_system_diagnostics(IN file_t diagnostics_file)
{

    DIAGNOSTICS_INFORMATION diag_info;
    NTSTATUS result;
    /* Declare large structure here, excluding it from recursion storage */

    print_file(diagnostics_file,
               "<system-settings>\n"
               "<computer name=\"%s\" />\n",
               get_computer_name());

    report_processor_info(diagnostics_file);

    memset(&diag_info, 0, sizeof(diag_info));
    result = query_system_info(SystemBasicInformation, sizeof(SYSTEM_BASIC_INFORMATION),
                               &(diag_info.sbasic_info));
    if (NT_SUCCESS(result)) {
        print_file(diagnostics_file,
                   "<basic-information>\n"
                   "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
                   "\t0x%.8x 0x%.8x %.10d %.10d \n"
                   "</basic-information>\n",
                   diag_info.sbasic_info.Unknown, diag_info.sbasic_info.MaximumIncrement,
                   diag_info.sbasic_info.PhysicalPageSize,
                   diag_info.sbasic_info.NumberOfPhysicalPages,
                   diag_info.sbasic_info.LowestPhysicalPage,
                   diag_info.sbasic_info.HighestPhysicalPage,
                   diag_info.sbasic_info.AllocationGranularity,
                   diag_info.sbasic_info.LowestUserAddress,
                   diag_info.sbasic_info.HighestUserAddress,
                   diag_info.sbasic_info.ActiveProcessors,
                   diag_info.sbasic_info.NumberProcessors);
    }

    result = query_system_info(SystemPerformanceInformation,
                               sizeof(SYSTEM_PERFORMANCE_INFORMATION),
                               &(diag_info.sperf_info));
    if (NT_SUCCESS(result)) {
        /* FIXME: good we started with all, but we should cut most of the
         * useless ones */
        print_file(diagnostics_file,
                   "<performance-information>\n"
                   "\t0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n",
                   diag_info.sperf_info.IdleTime.HighPart,
                   diag_info.sperf_info.IdleTime.LowPart,
                   diag_info.sperf_info.ReadTransferCount.HighPart,
                   diag_info.sperf_info.ReadTransferCount.LowPart,
                   diag_info.sperf_info.WriteTransferCount.HighPart,
                   diag_info.sperf_info.WriteTransferCount.LowPart,
                   diag_info.sperf_info.OtherTransferCount.HighPart,
                   diag_info.sperf_info.OtherTransferCount.LowPart);
        print_file(
            diagnostics_file,
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n",
            diag_info.sperf_info.ReadOperationCount,
            diag_info.sperf_info.WriteOperationCount,
            diag_info.sperf_info.OtherOperationCount, diag_info.sperf_info.AvailablePages,
            diag_info.sperf_info.TotalCommittedPages,
            diag_info.sperf_info.TotalCommitLimit, diag_info.sperf_info.PeakCommitment,
            diag_info.sperf_info.PageFaults, diag_info.sperf_info.WriteCopyFaults,
            diag_info.sperf_info.TranstitionFaults, diag_info.sperf_info.Reserved1,
            diag_info.sperf_info.DemandZeroFaults, diag_info.sperf_info.PagesRead,
            diag_info.sperf_info.PageReadIos, diag_info.sperf_info.Reserved2[0],
            diag_info.sperf_info.Reserved2[1], diag_info.sperf_info.PageFilePagesWritten,
            diag_info.sperf_info.PageFilePagesWriteIos,
            diag_info.sperf_info.MappedFilePagesWritten,
            diag_info.sperf_info.PagedPoolUsage, diag_info.sperf_info.NonPagedPoolUsage,
            diag_info.sperf_info.PagedPoolAllocs, diag_info.sperf_info.PagedPoolFrees,
            diag_info.sperf_info.NonPagedPoolAllocs,
            diag_info.sperf_info.NonPagedPoolFrees,
            diag_info.sperf_info.TotalFreeSystemPtes, diag_info.sperf_info.SystemCodePage,
            diag_info.sperf_info.TotalSystemDriverPages,
            diag_info.sperf_info.TotalSystemCodePages,
            diag_info.sperf_info.SmallNonPagedLookasideListAllocateHits,
            diag_info.sperf_info.SmallPagedLookasieListAllocateHits,
            diag_info.sperf_info.Reserved3, diag_info.sperf_info.MmSystemCachePage,
            diag_info.sperf_info.PagedPoolPage, diag_info.sperf_info.SystemDriverPage);
        print_file(
            diagnostics_file,
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d %.10d\n"
            "\t%.10d %.10d %.10d %.10d %.10d %.10d\n"
            "</performance-information>\n",
            diag_info.sperf_info.FastReadNoWait, diag_info.sperf_info.FastReadWait,
            diag_info.sperf_info.FastReadResourceMiss,
            diag_info.sperf_info.FastReadNotPossible,
            diag_info.sperf_info.FastMdlReadNoWait, diag_info.sperf_info.FastMdlReadWait,
            diag_info.sperf_info.FastMdlReadResourceMiss,
            diag_info.sperf_info.FastMdlReadNotPossible,
            diag_info.sperf_info.MapDataNoWait, diag_info.sperf_info.MapDataWait,
            diag_info.sperf_info.MapDataNoWaitMiss, diag_info.sperf_info.MapDataWaitMiss,
            diag_info.sperf_info.PinMappedDataCount, diag_info.sperf_info.PinReadNoWait,
            diag_info.sperf_info.PinReadWait, diag_info.sperf_info.PinReadNoWaitMiss,
            diag_info.sperf_info.PinReadWaitMiss, diag_info.sperf_info.CopyReadNoWait,
            diag_info.sperf_info.CopyReadWait, diag_info.sperf_info.CopyReadNoWaitMiss,
            diag_info.sperf_info.CopyReadWaitMiss, diag_info.sperf_info.MdlReadNoWait,
            diag_info.sperf_info.MdlReadWait, diag_info.sperf_info.MdlReadNoWaitMiss,
            diag_info.sperf_info.MdlReadWaitMiss, diag_info.sperf_info.ReadAheadIos,
            diag_info.sperf_info.LazyWriteIos, diag_info.sperf_info.LazyWritePages,
            diag_info.sperf_info.DataFlushes, diag_info.sperf_info.DataPages,
            diag_info.sperf_info.ContextSwitches, diag_info.sperf_info.FirstLevelTbFills,
            diag_info.sperf_info.SecondLevelTbFills, diag_info.sperf_info.SystemCalls);
    }

    result = query_system_info(SystemTimeOfDayInformation,
                               sizeof(SYSTEM_TIME_OF_DAY_INFORMATION),
                               &(diag_info.stime_info));
    if (NT_SUCCESS(result)) {
        print_file(diagnostics_file,
                   "<time-of-day-information>\n"
                   "\t0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x %.10d\n"
                   "</time-of-day-information>\n",
                   diag_info.stime_info.BootTime.HighPart,
                   diag_info.stime_info.BootTime.LowPart,
                   diag_info.stime_info.CurrentTime.HighPart,
                   diag_info.stime_info.CurrentTime.LowPart,
                   diag_info.stime_info.TimeZoneBias.HighPart,
                   diag_info.stime_info.TimeZoneBias.LowPart,
                   diag_info.stime_info.CurrentTimeZoneId);
    }

    result = query_system_info(SystemProcessorTimes, sizeof(SYSTEM_PROCESSOR_TIMES),
                               &(diag_info.sptime_info));
    if (NT_SUCCESS(result)) {
        print_file(diagnostics_file,
                   "<processor-times>\n"
                   "\t0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x 0x%.11x%.8x\n"
                   "\t0x%.11x%.8x %.10d\n"
                   "</processor-times>\n",
                   diag_info.sptime_info.IdleTime.HighPart,
                   diag_info.sptime_info.IdleTime.LowPart,
                   diag_info.sptime_info.KernelTime.HighPart,
                   diag_info.sptime_info.KernelTime.LowPart,
                   diag_info.sptime_info.UserTime.HighPart,
                   diag_info.sptime_info.UserTime.LowPart,
                   diag_info.sptime_info.DpcTime.HighPart,
                   diag_info.sptime_info.DpcTime.LowPart,
                   diag_info.sptime_info.InterruptTime.HighPart,
                   diag_info.sptime_info.InterruptTime.LowPart,
                   diag_info.sptime_info.InterruptCount);
    }

    result = query_system_info(SystemGlobalFlag, sizeof(SYSTEM_GLOBAL_FLAG),
                               &(diag_info.global_flag));
    if (NT_SUCCESS(result)) {
        print_file(diagnostics_file, "<global-flag> 0x%.8x </global-flag>\n",
                   diag_info.global_flag.GlobalFlag);
    }

    print_file(diagnostics_file, "</system-settings>\n\n");
}

static void
add_diagnostics_xml_header(file_t diagnostics_file)
{
    /* FIXME - xref case 9425, iso-8859-1 encoding is chosen because all 8 bit values
     * are valid and wld.exe's library knows how to handle it.  Other choices may be
     * more appropriate in the future. */
    print_file(diagnostics_file,
               "<?xml version=\"" DIAGNOSTICS_XML_FILE_VERSION
               "\" encoding=\"iso-8859-1\" ?>\n"
               "<!--\n"
               "  =====================================================================\n"
               "  Copyright @ " COMPANY_LONG_NAME " (2007). All rights reserved\n"
               "  =====================================================================\n"
               " -->\n"
               "<forensic-report title=\"" PRODUCT_NAME " Forensic File\">\n");
}

static void
report_diagnostics_common(file_t diagnostics_file, const char *message, const char *name,
                          security_violation_t violation_type)
{
    uint total_keys = 0;
    uint recursion_level = 0;

    report_intro(diagnostics_file, message, name);

    /* Process snapshot requires memory allocation -- only use if genuine attack */
    if (violation_type == NO_VIOLATION_BAD_INTERNAL_STATE) {
        report_current_process(diagnostics_file, NULL /*no snaphot*/, violation_type,
                               true /*be conservative*/);
    } else
        report_processes(diagnostics_file, violation_type);

    report_system_diagnostics(diagnostics_file);

    d_r_mutex_lock(&reg_mutex);
    print_file(diagnostics_file, "<registry-settings>\n<![CDATA[\n");
    report_registry_settings(diagnostics_file, DYNAMORIO_REGISTRY_BASE,
                             (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_ALLSUBKEYS |
                              DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    report_registry_settings(
        diagnostics_file, DIAGNOSTICS_OS_REG_KEY,
        (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    /* delve deeper into OS reg key for our two injection method keys */
    report_registry_settings(
        diagnostics_file, INJECT_ALL_HIVE_L INJECT_ALL_KEY_L,
        (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    report_registry_settings(diagnostics_file,
                             DEBUGGER_INJECTION_HIVE_L DEBUGGER_INJECTION_KEY_L,
                             (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_ALLSUBKEYS |
                              DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    report_registry_settings(
        diagnostics_file, DIAGNOSTICS_BIOS_REG_KEY,
        (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    report_registry_settings(
        diagnostics_file, DIAGNOSTICS_HARDWARE_REG_KEY,
        (DIAGNOSTICS_REG_HARDWARE | DIAGNOSTICS_REG_ALLSUBKEYS | DIAGNOSTICS_REG_DATA));
    report_registry_settings(
        diagnostics_file, DIAGNOSTICS_CONTROL_REG_KEY,
        (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    report_registry_settings(diagnostics_file, DIAGNOSTICS_OS_HOTFIX_REG_KEY,
                             (DIAGNOSTICS_REG_ALLKEYS | DIAGNOSTICS_REG_ALLSUBKEYS |
                              DIAGNOSTICS_REG_NAME | DIAGNOSTICS_REG_DATA));
    print_file(diagnostics_file, "]]>\n</registry-settings>\n\n");
    report_ntdll_info(diagnostics_file);
    report_autostart_programs(diagnostics_file);
    d_r_mutex_unlock(&reg_mutex);

    report_internal_data_structures(diagnostics_file, violation_type);

    return;
}

/* Collects and displays all diagnostic information.
 * violation_type of NO_VIOLATION_* is diagnostics; other is forensics.
 */
void
report_diagnostics(IN const char *message,
                   IN const char *name, /* NULL if not a violation */
                   IN security_violation_t violation_type)
{

    char diagnostics_filename[MAXIMUM_PATH];
    file_t diagnostics_file = INVALID_FILE;

    /* caller is assumed to have synchronized options */
    if (!DYNAMO_OPTION(diagnostics))
        return;

    open_diagnostics_file(&diagnostics_file, diagnostics_filename,
                          BUFFER_SIZE_ELEMENTS(diagnostics_filename));

    if (diagnostics_file == INVALID_FILE)
        return;

    /* Begin file with appropriate header */
    add_diagnostics_xml_header(diagnostics_file);

    report_diagnostics_common(diagnostics_file, message, name, violation_type);

    /* End-of-file */
    print_file(diagnostics_file, "</forensic-report>\n");

    if (diagnostics_file != INVALID_FILE)
        os_close(diagnostics_file);

    /* write an event indicating the file was created */
    SYSLOG(SYSLOG_INFORMATION, SEC_FORENSICS, 3, get_application_name(),
           get_application_pid(), diagnostics_filename);
}

/* Functions similarly to report_diagnostics only appends to a supplied file instead of
 * creating a file. It also skips adding a header in case this is part of a larger xml
 * structure. */
void
append_diagnostics(file_t diagnostics_file, const char *message, const char *name,
                   security_violation_t violation_type)
{
    /* Begin Report */
    print_file(diagnostics_file,
               "<forensic-report title=\"" PRODUCT_NAME " Forensic File\" "
               "version=\"" DIAGNOSTICS_XML_FILE_VERSION "\" encoding=\"iso-8859-1\">\n");

    report_diagnostics_common(diagnostics_file, message, name, violation_type);

    /* End-of-file */
    print_file(diagnostics_file, "</forensic-report>\n");
}

void
diagnost_exit()
{
    DELETE_LOCK(reg_mutex);
}
