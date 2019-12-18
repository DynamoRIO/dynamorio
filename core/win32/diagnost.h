/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * diagnost.h - Diagnostics declarations
 */

#ifndef _DIAGNOST_H_
#define _DIAGNOST_H_ 1

/* FIXME: The key for the log directory should be in a shared file! */
#define DIAGNOSTICS_LOGDIR_KEY L"DYNAMORIO_LOGDIR"
#define DIAGNOSTICS_FILE_XML_EXTENSION ".xml"
#define DIAGNOSTICS_XML_FILE_VERSION "1.0"
#define DIAGNOSTICS_NTDLL_DLL_LOCATION L"System32\\NTDLL.DLL"
#define DIAGNOSTICS_HARDWARE_REG_KEY \
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum"
#define DIAGNOSTICS_CONTROL_REG_KEY \
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Control"
#define DIAGNOSTICS_TEST_REG_KEY L"\\Registry\\Machine\\Software"
#define DIAGNOSTICS_OS_REG_KEY \
    L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion"
#define DIAGNOSTICS_OS_HOTFIX_REG_KEY \
    L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix"
#define DIAGNOSTICS_BIOS_REG_KEY L"\\Registry\\Machine\\Hardware\\Description\\System"
#define DIAGNOSTICS_SYSTEMROOT_REG_KEY L"SystemRoot"
#define DIAGNOSTICS_DESCRIPTION_KEY L"DeviceDesc"
#define DIAGNOSTICS_MANUFACTURER_KEY L"Mfg"
#define DIAGNOSTICS_FRIENDLYNAME_KEY L"FriendlyName"

#define DIAGNOSTICS_MAX_REG_KEYS 1000          /* Arbitrary - seems sufficient */
#define DIAGNOSTICS_MAX_REG_VALUES 1000        /* Arbitrary - seems sufficient */
#define DIAGNOSTICS_MAX_RECURSION_LEVEL 5      /* Arbitrary, but should keep small */
#define DIAGNOSTICS_MAX_NAME_AND_DATA_SIZE 500 /* Arbitrary - seems sufficient */
#define DIAGNOSTICS_MAX_KEY_NAME_SIZE 257      /* From SDK, + 1 for a null */
#define DIAGNOSTICS_MAX_LOG_BUFFER_SIZE 1000
#define DIAGNOSTICS_MAX_LOG_FILES 99999999 /* Supports 8.3 */
#define DIAGNOSTICS_MINI_DUMP_SIZE 104     /* multiple of 8, so is dumped aligned */

#define DIAGNOSTICS_REG_NAME 0x00000001L       /* Log key name */
#define DIAGNOSTICS_REG_DATA 0x00000002L       /* Log key data */
#define DIAGNOSTICS_REG_HARDWARE 0x00000004L   /* Look for device keys */
#define DIAGNOSTICS_REG_ALLKEYS 0x00000008L    /* Search all keys */
#define DIAGNOSTICS_REG_ALLSUBKEYS 0x00000010L /* Search all subkeys (recursive)*/
#define DIAGNOSTICS_INITIAL_PROCESS_TOTAL 10

#define DIAGNOSTICS_BYTES_PER_LINE 32

/* The DataOffset field in KEY_VALUE_FULL_INFORMATION uses the size of the structure as
  part of the offset.  When offsetting into the NameAndData member (see below), these
  bytes are not present, and must be decremented.  The 2 WCHARs readjusts back for the
  null-terminated Name[1] (which IS included in NameAndData).  Confusing, no? */
#define DECREMENT_FOR_DATA_OFFSET \
    (sizeof(KEY_VALUE_FULL_INFORMATION) - (sizeof(WCHAR) * 2))

typedef struct _DIAGNOSTICS_INFORMATION {
    SYSTEM_BASIC_INFORMATION sbasic_info;
    SYSTEM_PROCESSOR_INFORMATION sproc_info;
    SYSTEM_PERFORMANCE_INFORMATION sperf_info;
    SYSTEM_TIME_OF_DAY_INFORMATION stime_info;
    SYSTEM_PROCESSOR_TIMES sptime_info;
    SYSTEM_GLOBAL_FLAG global_flag;
} DIAGNOSTICS_INFORMATION, *PDIAGNOSTICS_INFORMATION;

/* FIXME: combine this definition and KEY_VALUE_INFORMATION_CLASS from ntdll.h */
/* The properly-sized name & data field for KEY_VALUE_FULL_INFORMATION
   are not included in the KEY_VALUE_FULL_INFORMATION structure, but
   needed to receive the data after calling NtEnumerateValueKey().
   This structure includes a buffer (NameAndData) for the largest reasonable
   size for both variable items; anything longer will simply be discarded).
   The variable name field in NameAndData will always be null-terminated,
   and the variable data field in NameAndData will always begin at
   (DataOffset - DECREMENT_FOR_DATAOFFSET). */
typedef struct _DIAGNOSTICS_KEY_VALUE_FULL_INFORMATION {
    ULONG TitleIndex;
    ULONG Type;
    ULONG DataOffset;
    ULONG DataLength;
    ULONG NameLength; /* in BYTES (including NULL term) */
    BYTE NameAndData[DIAGNOSTICS_MAX_NAME_AND_DATA_SIZE];
} DIAGNOSTICS_KEY_VALUE_FULL_INFORMATION, *PDIAGNOSTICS_KEY_VALUE_FULL_INFORMATION;

/* Same story as above with the key basic info here */
typedef struct _DIAGNOSTICS_KEY_NAME_INFORMATION {
    LARGE_INTEGER LastWriteTime;
    ULONG TitleIndex;
    ULONG NameLength; /* in BYTES (including NULL term) */
    WCHAR Name[DIAGNOSTICS_MAX_KEY_NAME_SIZE];
} DIAGNOSTICS_KEY_NAME_INFORMATION, *PDIAGNOSTICS_KEY_NAME_INFORMATION;

#endif /* #ifndef _DIAGNOST_H_ */
