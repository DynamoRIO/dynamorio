/* **********************************************************
 * Copyright (c) 2011-2019 Google, Inc.  All rights reserved.
 * Copyright (c) 2005-2008 VMware, Inc.  All rights reserved.
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

/* Copyright (c) 2005-2007 Determina Corp. */

/*
 * drmarker.h
 *
 * shared between core/gui nodemgr, functions to tell if a process is running
 * under dr and pass information out of the running process
 */

#ifndef _DRMARKER_H_
#define _DRMARKER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define L_DR_MARKER_HOOKED_DLL L"ntdll.dll"
#ifdef UNICODE
#    define DR_MARKER_HOOKED_DLL L_DR_MARKER_HOOKED_DLL
#else
#    define DR_MARKER_HOOKED_DLL "ntdll.dll"
#endif

#define DR_MARKER_HOOKED_FUNCTION KiUserCallbackDispatcher
#define DR_MARKER_HOOKED_FUNCTION_ARGS \
    (IN PVOID Unknown1, IN PVOID Unknown2, IN PVOID Unknown3)
#define DR_MARKER_HOOKED_FUNCTION_STRING STRINGIFY(DR_MARKER_HOOKED_FUNCTION)

enum {
    DR_MARKER_RELEASE_BUILD = 0x1,
    DR_MARKER_DEBUG_BUILD = 0x2,
    DR_MARKER_PROFILE_BUILD = 0x4,
    DR_MARKER_BUILD_TYPES =
        DR_MARKER_RELEASE_BUILD | DR_MARKER_DEBUG_BUILD | DR_MARKER_PROFILE_BUILD
};

#define DR_MARKER_VERSION_1 1
#define DR_MARKER_VERSION_2 2

/* The dr_marker_t struct must be <4096 for the PAGE_START assumptions of the
 * marker location code to work.
 */
#define WINDBG_CMD_MAX_LEN 3072

/* CAUTION: This structure is shared across processes, so any changes to it
 *          should only be field addtions.  NO DELETIONS ALLOWED; to obsolete
 *          fields fill it with an invalid value.  Also, each change should
 *          result in the DR_MARKER_VERSION_CURRENT being increased.
 * FIXME: use size to denote a newer structures in future; same issue needs to
 *        be handled for hotp_policy_status_table.
 */
struct _dr_statistics_t;
typedef struct _dr_marker_t {
    uint magic1;
    uint magic2;
    uint magic3;
    uint magic4;
    uint flags;
    uint build_num;
    uint dr_marker_version; /* this offset cannot be changed */
    void *dr_base_addr;
    void *dr_generic_nudge_target;
    void *dr_hotp_policy_status_table;
    struct _dr_statistics_t *stats;
    /* For auto-loading private lib symbols (i#522).
     * tools/windbg-scripts/load_syms hardcodes the offset of this field.
     */
    char windbg_cmds[WINDBG_CMD_MAX_LEN];
    /* future fields */
    /* NOTE: rct_known_targets_init needs to be updated if new targets
     * into DR are added */
} dr_marker_t;

enum { DR_MARKER_FOUND = 0, DR_MARKER_NOT_FOUND = 1, DR_MARKER_ERROR = 2 };

/* process must grant PROCESS_VM_READ */
int
read_and_verify_dr_marker(HANDLE process, dr_marker_t *marker);

#ifndef X64
/* 32-bit code to check for 64-bit marker. process must grant PROCESS_VM_READ */
int
read_and_verify_dr_marker_64(HANDLE process, dr_marker_t *marker);
#endif

/* up to includer to ensure "bool" is defined as a char-sized type */

bool
dr_marker_verify(HANDLE process, dr_marker_t *marker);

#if !defined(NOT_DYNAMORIO_CORE) && !defined(NOT_DYNAMORIO_CORE_PROPER)
void
init_dr_marker(dr_marker_t *marker);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _DRMARKER_H_ */
