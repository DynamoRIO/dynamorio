/* **********************************************************
 * Copyright (c) 2021-2022 Google, Inc.  All rights reserved.
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

#ifndef _MODULEDB_H_
#define _MODULEDB_H_

/* Module database section flags -
 * These control the module section specific relaxations and are used in 2-bit
 * fields in the module policy flags. */
enum {
    SECTION_NO_CHG = 0,
    SECTION_IF_RX = 1,
    SECTION_IF_X = 2,
    SECTION_ALLOW = 3,
};

/* Module database policy flags -
 * these specify the module specific actions to take when a module is loaded */
enum {
    MODULEDB_ALL_SECTIONS_BITS = 0x00000003, /* value from SECTION enum above */
    MODULEDB_ALL_SECTIONS_SHIFT = 0,         /* >> necessary to read ALL_SECTIONS_BITS */
    MODULEDB_RCT_EXEMPT_TO = 0x00000004,
    MODULEDB_REPORT_ON_LOAD = 0x00000008,
    MODULEDB_DLL2HEAP = 0x00000010,
    MODULEDB_DLL2STACK = 0x00000020,
};

/* Used to specify an exemption list for moduledb_check_exempt_list */
typedef enum {
    MODULEDB_EXEMPT_RCT = 0,
    MODULEDB_EXEMPT_IMAGE = 1,
    MODULEDB_EXEMPT_DLL2HEAP = 2,
    MODULEDB_EXEMPT_DLL2STACK = 3,
    MODULEDB_EXEMPT_NUM_LISTS = 4,
} moduledb_exempt_list_t;

void
moduledb_init(void);

void
moduledb_exit(void);

void
moduledb_process_image(const char *name, app_pc base, bool adding);

void
moduledb_report_exemption(const char *fmt, app_pc addr1, app_pc addr2, const char *name);

/* faster then check_exempt_list below as doesn't require grabbing a lock */
bool
moduledb_exempt_list_empty(moduledb_exempt_list_t list);

/* checks to see if module name is on the moduledb exempt list pointed
 * to by list */
bool
moduledb_check_exempt_list(moduledb_exempt_list_t list, const char *name);

void
print_moduledb_exempt_lists(file_t file);

#ifdef PROCESS_CONTROL
#    define PROCESS_CONTROL_MODE_OFF 0x0
#    define PROCESS_CONTROL_MODE_ALLOWLIST 0x1
#    define PROCESS_CONTROL_MODE_BLOCKLIST 0x2

/* This mode is identical to allowlist mode, but requires that the user specify
 * an exe by name and its hashes; no anonymous hashes or exe names with no
 * hashes.  Case 10969. */
#    define PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY 0x4

#    define IS_PROCESS_CONTROL_MODE_ALLOWLIST() \
        (TEST(PROCESS_CONTROL_MODE_ALLOWLIST, DYNAMO_OPTION(process_control)))
#    define IS_PROCESS_CONTROL_MODE_BLOCKLIST() \
        (TEST(PROCESS_CONTROL_MODE_BLOCKLIST, DYNAMO_OPTION(process_control)))
#    define IS_PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY() \
        (TEST(PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY, DYNAMO_OPTION(process_control)))
#    define IS_PROCESS_CONTROL_ON()                                                    \
        (IS_PROCESS_CONTROL_MODE_ALLOWLIST() || IS_PROCESS_CONTROL_MODE_BLOCKLIST() || \
         IS_PROCESS_CONTROL_MODE_ALLOWLIST_INTEGRITY())

void
process_control(void);
void
process_control_init(void);
#endif

#endif /* _MODULEDB_H_ */
