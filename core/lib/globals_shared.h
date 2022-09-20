/* **********************************************************
 * Copyright (c) 2011-2022 Google, Inc.  All rights reserved.
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

/* Copyright (c) 2003-2007 Determina Corp. */

/*
 * globals_shared.h
 *
 * definitions to be shared with core-external modules
 *
 */

#ifndef _GLOBALS_SHARED_H_
#define _GLOBALS_SHARED_H_ 1

/* if not Unix, we assume Windows */
#ifndef UNIX
#    ifndef WINDOWS
#        define WINDOWS
#    endif
#endif

#ifdef DR_NO_FAST_IR
#    undef DR_FAST_IR
#    undef INSTR_INLINE
#else
#    define DR_FAST_IR 1
#endif

/* Internally, ensure these defines are set */
#if defined(X86) && !defined(X64) && !defined(X86_32)
#    define X86_32
#endif
#if defined(X86) && defined(X64) && !defined(X86_64)
#    define X86_64
#endif
#if defined(AARCHXX) && !defined(X64) && !defined(ARM_32)
#    define ARM_32
#endif
#if defined(AARCHXX) && defined(X64) && !defined(ARM_64)
#    define ARM_64
#endif
#if defined(RISCV64) && defined(X64) && !defined(RISCV_64)
#    define RISCV_64
#endif

#include "globals_api.h"

#include <limits.h> /* for USHRT_MAX */
#ifdef UNIX
#    include <signal.h>
#endif

#include "c_defines.h"

#ifdef X64
#    define POINTER_MAX ULLONG_MAX
#    ifndef SSIZE_T_MAX
#        define SSIZE_T_MAX LLONG_MAX
#    endif
#    define POINTER_MAX_32BIT ((ptr_uint_t)UINT_MAX)
#else
#    define POINTER_MAX UINT_MAX
#    ifndef SSIZE_T_MAX
#        define SSIZE_T_MAX INT_MAX
#    endif
#endif

#define MAX_CLIENT_LIBS 16

/* FIXME: these macros double-evaluate their args -- if we can't trust
 * compiler to do CSE, should we replace w/ inline functions?  would need
 * separate signed and unsigned versions
 */
#ifndef DR_DO_NOT_DEFINE_MAX_MIN
#    define MAX(x, y) ((x) >= (y) ? (x) : (y))
#    define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif
#define PTR_UINT_ABS(x) ((x) < 0 ? -(x) : (x))

/* check if all bits in mask are set in var */
#define TESTALL(mask, var) (((mask) & (var)) == (mask))
/* check if any bit in mask is set in var */
#define TESTANY(mask, var) (((mask) & (var)) != 0)
/* check if a single bit is set in var */
#define TEST TESTANY

#define BOOLS_MATCH(a, b) (!!(a) == !!(b))

/* macros to make conditional compilation look prettier */
#ifdef DEBUG
#    define IF_DEBUG(x) x
#    define _IF_DEBUG(x) , x
#    define IF_DEBUG_(x) x,
#    define IF_DEBUG_ELSE(x, y) x
#    define IF_DEBUG_ELSE_0(x) (x)
#    define IF_DEBUG_ELSE_NULL(x) (x)
#else
#    define IF_DEBUG(x)
#    define _IF_DEBUG(x)
#    define IF_DEBUG_(x)
#    define IF_DEBUG_ELSE(x, y) y
#    define IF_DEBUG_ELSE_0(x) 0
#    define IF_DEBUG_ELSE_NULL(x) (NULL)
#endif

#ifdef INTERNAL
#    define IF_INTERNAL(x) x
#    define IF_INTERNAL_ELSE(x, y) x
#else
#    define IF_INTERNAL(x)
#    define IF_INTERNAL_ELSE(x, y) y
#endif

#ifdef WINDOWS
#    define IF_WINDOWS(x) x
#    define IF_WINDOWS_(x) x,
#    define _IF_WINDOWS(x) , x
#    ifdef X64
#        define _IF_WINDOWS_X64(x) , x
#    else
#        define _IF_WINDOWS_X64(x)
#    endif
#    define IF_WINDOWS_ELSE_0(x) (x)
#    define IF_WINDOWS_ELSE(x, y) (x)
#    define IF_WINDOWS_ELSE_NP(x, y) x
#    define IF_UNIX(x)
#    define IF_UNIX_ELSE(x, y) y
#    define IF_UNIX_(x)
#    define _IF_UNIX(x)
#else
#    define IF_WINDOWS(x)
#    define IF_WINDOWS_(x)
#    define _IF_WINDOWS(x)
#    define _IF_WINDOWS_X64(x)
#    define IF_WINDOWS_ELSE_0(x) (0)
#    define IF_WINDOWS_ELSE(x, y) (y)
#    define IF_WINDOWS_ELSE_NP(x, y) y
#    define IF_UNIX(x) x
#    define IF_UNIX_ELSE(x, y) x
#    define IF_UNIX_(x) x,
#    define _IF_UNIX(x) , x
#endif

#ifdef LINUX
#    define IF_LINUX(x) x
#    define IF_LINUX_ELSE(x, y) x
#    define IF_LINUX_(x) x,
#    define _IF_LINUX(x) , x
#else
#    define IF_LINUX(x)
#    define IF_LINUX_ELSE(x, y) y
#    define IF_LINUX_(x)
#    define _IF_LINUX(x)
#endif

#ifdef MACOS
#    define IF_MACOS(x) x
#    define IF_MACOS_ELSE(x, y) x
#    define IF_MACOS_(x) x,
#else
#    define IF_MACOS(x)
#    define IF_MACOS_ELSE(x, y) y
#    define IF_MACOS_(x)
#endif

#ifdef MACOS64
#    define IF_MACOS64(x) x
#    define IF_MACOS64_ELSE(x, y) x
#else
#    define IF_MACOS64(x)
#    define IF_MACOS64_ELSE(x, y) y
#endif

#if defined(MACOS) && defined(AARCH64)
#    define IF_MACOSA64_ELSE(x, y) x
#else
#    define IF_MACOSA64_ELSE(x, y) y
#endif

#ifdef HAVE_MEMINFO_QUERY
#    define IF_MEMQUERY(x) x
#    define IF_MEMQUERY_(x) x,
#    define IF_MEMQUERY_ELSE(x, y) x
#    define IF_NO_MEMQUERY(x)
#    define IF_NO_MEMQUERY_(x)
#else
#    define IF_MEMQUERY(x)
#    define IF_MEMQUERY_(x)
#    define IF_MEMQUERY_ELSE(x, y) y
#    define IF_NO_MEMQUERY(x) x
#    define IF_NO_MEMQUERY_(x) x,
#endif

#ifdef VMX86_SERVER
#    define IF_VMX86(x) x
#    define IF_VMX86_ELSE(x, y) x
#    define _IF_VMX86(x) , x
#    define IF_NOT_VMX86(x)
#else
#    define IF_VMX86(x)
#    define IF_VMX86_ELSE(x, y) y
#    define _IF_VMX86(x)
#    define IF_NOT_VMX86(x) x
#endif

#ifdef UNIX
#    ifdef HAVE_TLS
#        define IF_HAVE_TLS_ELSE(x, y) x
#        define IF_NOT_HAVE_TLS(x)
#    else
#        define IF_HAVE_TLS_ELSE(x, y) y
#        define IF_NOT_HAVE_TLS(x) x
#    endif
#else
/* Windows always has TLS.  Better to generally define HAVE_TLS instead? */
#    define IF_HAVE_TLS_ELSE(x, y) x
#    define IF_NOT_HAVE_TLS(x)
#endif

#if defined(WINDOWS) && !defined(NOT_DYNAMORIO_CORE)
#    define IF_WINDOWS_AND_CORE(x) x
#else
#    define IF_WINDOWS_AND_CORE(x)
#endif

#ifdef PROGRAM_SHEPHERDING
#    define IF_PROG_SHEP(x) x
#else
#    define IF_PROG_SHEP(x)
#endif

#if defined(PROGRAM_SHEPHERDING) && defined(RCT_IND_BRANCH)
#    define IF_RCT_IND_BRANCH(x) x
#else
#    define IF_RCT_IND_BRANCH(x)
#endif

#if defined(PROGRAM_SHEPHERDING) && defined(RETURN_AFTER_CALL)
#    define IF_RETURN_AFTER_CALL(x) x
#    define IF_RETURN_AFTER_CALL_ELSE(x, y) x
#else
#    define IF_RETURN_AFTER_CALL(x)
#    define IF_RETURN_AFTER_CALL_ELSE(x, y) y
#endif

#ifdef HOT_PATCHING_INTERFACE
#    define IF_HOTP(x) x
#else
#    define IF_HOTP(x)
#endif

#ifdef DR_APP_EXPORTS
#    define IF_APP_EXPORTS(x) x
#else
#    define IF_APP_EXPORTS(x)
#endif

#ifdef GBOP
#    define IF_GBOP(x) x
#else
#    define IF_GBOP(x)
#endif

#ifdef PROCESS_CONTROL
#    define IF_PROC_CTL(x) x
#else
#    define IF_PROC_CTL(x)
#endif

#ifdef KSTATS
#    define IF_KSTATS(x) x
#else
#    define IF_KSTATS(x)
#endif

#ifdef STATIC_LIBRARY
#    define IF_STATIC_LIBRARY_ELSE(x, y) x
#else
#    define IF_STATIC_LIBRARY_ELSE(x, y) y
#endif

#ifdef STANDALONE_UNIT_TEST
#    define IF_UNIT_TEST_ELSE(x, y) x
#else
#    define IF_UNIT_TEST_ELSE(x, y) y
#endif

#ifdef AUTOMATED_TESTING
#    define IF_AUTOMATED_ELSE(x, y) x
#else
#    define IF_AUTOMATED_ELSE(x, y) y
#endif

#ifdef DR_HOST_X86
#    define IF_HOST_X86_ELSE(x, y) x
#else
#    define IF_HOST_X86_ELSE(x, y) y
#endif
#ifdef DR_HOST_X64
#    define IF_HOST_X64_ELSE(x, y) x
#else
#    define IF_HOST_X64_ELSE(x, y) y
#endif

typedef enum {
    SYSLOG_INFORMATION = 0x1,
    SYSLOG_WARNING = 0x2,
    SYSLOG_ERROR = 0x4,
    SYSLOG_CRITICAL = 0x8,
    SYSLOG_VERBOSE = 0x10,
    SYSLOG_NONE = 0x0,
    SYSLOG_ALL_NOVERBOSE =
        (SYSLOG_INFORMATION | SYSLOG_WARNING | SYSLOG_ERROR | SYSLOG_CRITICAL),
    SYSLOG_ALL = (SYSLOG_VERBOSE | SYSLOG_INFORMATION | SYSLOG_WARNING | SYSLOG_ERROR |
                  SYSLOG_CRITICAL),
} syslog_event_type_t;

#define DYNAMO_OPTION(opt) \
    (ASSERT_OWN_READWRITE_LOCK(IS_OPTION_STRING(opt), &options_lock), dynamo_options.opt)
/* For use where we don't want ASSERT defines. Currently only used in FATAL_USAGE_ERROR
 * so it can be used in files that require all asserts to be client asserts. */
#define DYNAMO_OPTION_NOT_STRING(opt) (dynamo_options.opt)

#define EXPOSE_INTERNAL_OPTIONS

#ifdef EXPOSE_INTERNAL_OPTIONS
/* Use only for experimental non-release options */
/* Internal option value can be set on the command line only in INTERNAL builds */
/* We should remove the ASSERT if we convert an internal option into non-internal */
#    define INTERNAL_OPTION(opt)                                                     \
        ((IS_OPTION_INTERNAL(opt))                                                   \
             ? (DYNAMO_OPTION(opt))                                                  \
             : (ASSERT_MESSAGE(CHKLVL_ASSERTS, "non-internal option argument " #opt, \
                               false),                                               \
                DYNAMO_OPTION(opt)))
#else
/* Use only for experimental non-release options,
   default value is assumed and command line options are ignored */
/* We could use IS_OPTION_INTERNAL(opt) ? to determine whether an
 * option is defined as INTERNAL in optionsx.h and have that be the
 * only place to modify to transition between internal and external
 * options.  The compiler should be able to eliminate the
 * inappropriate part of the constant expression, if the specific
 * option is no longer defined as internal so we don't have to
 * modify the code.  Still I'd rather have explicit uses of
 * DYNAMO_OPTION or INTERNAL_OPTION for now.
 */
#    define INTERNAL_OPTION(opt) DEFAULT_INTERNAL_OPTION_VALUE(opt)
#endif /* EXPOSE_INTERNAL_OPTIONS */

#ifdef UNIX
#    ifndef DR_DO_NOT_DEFINE_uint32
typedef unsigned int uint32;
#    endif
/* Note: Linux paths are longer than the 260 limit on Windows,
   yet we can't afford the 4K of PATH_MAX from <limits.h> */
typedef uint64 timestamp_t;
#    define NAKED
#    define ZHEX32_FORMAT_STRING "%08x"
#    define HEX32_FORMAT_STRING "%x"

#else                    /* WINDOWS */
/* We no longer include windows.h here, in order to more easily include
 * this file for types like reg_t in hotpatch_interface.h and not force
 * hotpatch compilation to point at windows include directories.
 * By not including it here we must define MAX_PATH and include windows.h
 * for NOT_DYNAMORIO_CORE in options.c and drmarker.c for share/ compilation.
 */
#    define MAX_PATH 260 /* from winbase.h */
#    ifndef DR_DO_NOT_DEFINE_uint
typedef unsigned int uint;
#    endif
#    ifndef DR_DO_NOT_DEFINE_uint32
typedef unsigned int uint32;
#    endif
/* NOTE : unsigned __int64 not convertible to double */
typedef unsigned __int64 uint64;
typedef __int64 int64;
#    ifdef X64
typedef int64 ssize_t;
#    else
typedef int ssize_t;
#    endif
/* VC6 doesn't define the standard ULLONG_MAX */
#    if _MSC_VER <= 1200 && !defined(ULLONG_MAX)
#        define ULLONG_MAX _UI64_MAX
#    endif
typedef uint64 timestamp_t;
#    define NAKED __declspec(naked)
#endif

#define FIXED_TIMESTAMP_FORMAT "%10" INT64_FORMAT "u"

/* Statistics are 64-bit for x64, 32-bit for x86, so we don't have overflow
 * for pointer-sized stats.
 * TODO PR 227994: use per-statistic types for variable bitwidths.
 */
#ifdef X64
typedef int64 stats_int_t;
#else
typedef int stats_int_t;
#endif

#define L_UINT64_FORMAT_STRING L"%" L_EXPAND_LEVEL(UINT64_FORMAT_CODE)
#define L_PFMT L"%016" L_EXPAND_LEVEL(INT64_FORMAT) L"x"

/* Maximum length of any registry parameter. Note that some are further
 * restricted to MAXIMUM_PATH from their usage. */
#define MAX_REGISTRY_PARAMETER 512
#ifdef PARAMS_IN_REGISTRY
/* Maximum length of option string in the registry */
#    define MAX_OPTIONS_STRING 512
#    define MAX_CONFIG_VALUE MAX_REGISTRY_PARAMETER
#else
/* Maximum length of option string from config file.
 * For clients we need more than 512 bytes to fit multiple options
 * w/ paths.  However, we have stack buffers in config.c and options.c
 * (look for MAX_OPTION_LENGTH there), so we can't make this too big
 * unless we increase the default -stack_size even further.
 * N.B.: there is a separate define in dr_config.h, DR_MAX_OPTIONS_LENGTH.
 */
#    define MAX_OPTIONS_STRING 2048
#    define MAX_CONFIG_VALUE MAX_OPTIONS_STRING
#endif
/* Maximum length of any individual list option's string */
#define MAX_LIST_OPTION_LENGTH MAX_OPTIONS_STRING
/* Maximum length of the path secified by a path option */
#define MAX_PATH_OPTION_LENGTH MAXIMUM_PATH
/* Maximum length of any individual option. */
#define MAX_OPTION_LENGTH MAX_OPTIONS_STRING

#define MAX_PARAMNAME_LENGTH 64

/* Arbitrary debugging-only module name length maximum */
#define MAX_MODNAME_INTERNAL 64

/* Maximum string representation of a DWORD:
 * 0x80000000 = -2147483648 -> 11 characters + 1 for null termination
 */
#define MAX_DWORD_STRING_LENGTH 12

typedef char pathstring_t[MAX_PATH_OPTION_LENGTH];
/* The liststring_t type is assumed to contain ;-separated values that are
 * appended to if multiple option instances are specified
 */
typedef char liststring_t[MAX_LIST_OPTION_LENGTH];

/* convenience macros for secure string buffer operations */
#define BUFFER_SIZE_BYTES(buf) sizeof(buf)
#define BUFFER_SIZE_ELEMENTS(buf) (BUFFER_SIZE_BYTES(buf) / sizeof(buf[0]))
#define BUFFER_LAST_ELEMENT(buf) buf[BUFFER_SIZE_ELEMENTS(buf) - 1]
#define NULL_TERMINATE_BUFFER(buf) BUFFER_LAST_ELEMENT(buf) = 0

#define BUFFER_ROOM_LEFT_W(wbuf) (BUFFER_SIZE_ELEMENTS(wbuf) - wcslen(wbuf) - 1)
#define BUFFER_ROOM_LEFT(abuf) (BUFFER_SIZE_ELEMENTS(abuf) - strlen(abuf) - 1)

/* strncat() calls require termination after each to be safe, especially if
 * cating repeatedly.  For example see hotpatch.c:hotp_load_hotp_dlls().
 */
#define CAT_AND_TERMINATE(buf, str)               \
    do {                                          \
        strncat(buf, str, BUFFER_ROOM_LEFT(buf)); \
        NULL_TERMINATE_BUFFER(buf);               \
    } while (0)

/* platform-independent */

#define EXPANDSTR(x) #x
#define STRINGIFY(x) EXPANDSTR(x)

/* Allow custom builds to specify the product name */
#ifdef CUSTOM_PRODUCT_NAME
#    define PRODUCT_NAME STRINGIFY(CUSTOM_PRODUCT_NAME)
#else
#    define PRODUCT_NAME "DynamoRIO"
#endif
/* PR 323321: split eventlog from reg name so we can use longer name in reg.
 * Jim's original comment here about "alphanum only" and memories
 * of issues w/ eventlog Araksha => Determin[a] have made us cautious
 * (though in my tests on win2k the longer name worked fine for eventlog).
 */
/* i#80: I thought about flattening the reg key but simpler to just
 * have two DynamoRIO levels
 */
#define COMPANY_NAME "DynamoRIO"          /* used for reg key */
#define COMPANY_NAME_EVENTLOG "DynamoRIO" /* used for event log */
/* used in (c) stmt in log file and in resources */
#define COMPANY_LONG_NAME "DynamoRIO developers"
#define BUG_REPORT_URL "http://dynamorio.org/issues/"

#ifdef BUILD_NUMBER
#    define BUILD_NUMBER_STRING "build " STRINGIFY(BUILD_NUMBER)
#else
#    define BUILD_NUMBER_STRING "custom build"
#    define BUILD_NUMBER (0)
#endif

#ifdef VERSION_NUMBER
#    define VERSION_NUMBER_STRING "version " STRINGIFY(VERSION_NUMBER)
#else
#    define VERSION_NUMBER_STRING "internal version"
#    define VERSION_NUMBER (0.0)
#endif

#ifdef HOT_PATCHING_INTERFACE
#    define HOT_PATCHING_DLL_CACHE_PATH "\\lib\\hotp\\"
#    define HOTP_MODES_FILENAME "ls-modes.cfg"
#    define HOTP_POLICIES_FILENAME "ls-defs.cfg"
#endif

#define DYNAMORIO_VAR_CONFIGDIR_ID DYNAMORIO_CONFIGDIR
#define DYNAMORIO_VAR_HOME_ID DYNAMORIO_HOME
#define DYNAMORIO_VAR_LOGDIR_ID DYNAMORIO_LOGDIR
#define DYNAMORIO_VAR_OPTIONS_ID DYNAMORIO_OPTIONS
#define DYNAMORIO_VAR_AUTOINJECT_ID DYNAMORIO_AUTOINJECT
#define DYNAMORIO_VAR_ALTINJECT_ID DYNAMORIO_ALTINJECT
#define DYNAMORIO_VAR_UNSUPPORTED_ID DYNAMORIO_UNSUPPORTED
#define DYNAMORIO_VAR_RUNUNDER_ID DYNAMORIO_RUNUNDER
#define DYNAMORIO_VAR_CMDLINE_ID DYNAMORIO_CMDLINE
#define DYNAMORIO_VAR_ONCRASH_ID DYNAMORIO_ONCRASH
#define DYNAMORIO_VAR_SAFEMARKER_ID DYNAMORIO_SAFEMARKER
/* NT only, value should be all CAPS and specifies a boot option to match */

#define DYNAMORIO_VAR_CACHE_ROOT_ID DYNAMORIO_CACHE_ROOT
/* we have to create our own properly secured directory, that allows
 * only trusted producers to create DLLs, and all publishers to read
 * them.  Note that per-user directories may also be created by the trusted component
 * allowing users to safely use their own private caches.
 */
#define DYNAMORIO_VAR_CACHE_SHARED_ID DYNAMORIO_CACHE_SHARED
/* a directory giving full write privileges to Everyone.  Therefore
 * none of its contents can be trusted without explicit verification.
 * Expected to be a subdirectory of DYNAMORIO_CACHE_ROOT.
 */

/* Location for persisted caches; FIXME: currently the same as the ASLR sharing dir */
#define DYNAMORIO_VAR_PERSCACHE_ROOT_ID DYNAMORIO_CACHE_ROOT
/* FIXME case 9651: security model, etc. */
#define DYNAMORIO_VAR_PERSCACHE_SHARED_ID DYNAMORIO_CACHE_SHARED
/* case 10255: use a suffix to distinguish from ASLR files in same dir
 * DR persisted cache => "dpc"
 */
#define PERSCACHE_FILE_SUFFIX "dpc"

#ifdef HOT_PATCHING_INTERFACE
#    define DYNAMORIO_VAR_HOT_PATCH_POLICES_ID DYNAMORIO_HOT_PATCH_POLICIES
#    define DYNAMORIO_VAR_HOT_PATCH_MODES_ID DYNAMORIO_HOT_PATCH_MODES
#endif
#ifdef PROCESS_CONTROL
#    define DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST_ID DYNAMORIO_APP_PROCESS_ALLOWLIST
#    define DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST_ID DYNAMORIO_ANON_PROCESS_ALLOWLIST

#    define DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST_ID DYNAMORIO_APP_PROCESS_BLOCKLIST
#    define DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST_ID DYNAMORIO_ANON_PROCESS_BLOCKLIST
#endif

#define DYNAMORIO_VAR_CONFIGDIR STRINGIFY(DYNAMORIO_VAR_CONFIGDIR_ID)
#define DYNAMORIO_VAR_HOME STRINGIFY(DYNAMORIO_VAR_HOME_ID)
#define DYNAMORIO_VAR_LOGDIR STRINGIFY(DYNAMORIO_VAR_LOGDIR_ID)
#define DYNAMORIO_VAR_OPTIONS STRINGIFY(DYNAMORIO_VAR_OPTIONS_ID)
#define DYNAMORIO_VAR_AUTOINJECT STRINGIFY(DYNAMORIO_VAR_AUTOINJECT_ID)
#define DYNAMORIO_VAR_ALTINJECT STRINGIFY(DYNAMORIO_VAR_ALTINJECT_ID)
#define DYNAMORIO_VAR_UNSUPPORTED STRINGIFY(DYNAMORIO_VAR_UNSUPPORTED_ID)
#define DYNAMORIO_VAR_RUNUNDER STRINGIFY(DYNAMORIO_VAR_RUNUNDER_ID)
#define DYNAMORIO_VAR_CMDLINE STRINGIFY(DYNAMORIO_VAR_CMDLINE_ID)
#define DYNAMORIO_VAR_ONCRASH STRINGIFY(DYNAMORIO_VAR_ONCRASH_ID)
#define DYNAMORIO_VAR_SAFEMARKER STRINGIFY(DYNAMORIO_VAR_SAFEMARKER_ID)
#define DYNAMORIO_VAR_CACHE_ROOT STRINGIFY(DYNAMORIO_VAR_CACHE_ROOT_ID)
#define DYNAMORIO_VAR_CACHE_SHARED STRINGIFY(DYNAMORIO_VAR_CACHE_SHARED_ID)
#define DYNAMORIO_VAR_PERSCACHE_ROOT STRINGIFY(DYNAMORIO_VAR_PERSCACHE_ROOT_ID)
#define DYNAMORIO_VAR_PERSCACHE_SHARED STRINGIFY(DYNAMORIO_VAR_PERSCACHE_SHARED_ID)
#ifdef HOT_PATCHING_INTERFACE
#    define DYNAMORIO_VAR_HOT_PATCH_POLICIES STRINGIFY(DYNAMORIO_VAR_HOT_PATCH_POLICES_ID)
#    define DYNAMORIO_VAR_HOT_PATCH_MODES STRINGIFY(DYNAMORIO_VAR_HOT_PATCH_MODES_ID)
#endif
#ifdef PROCESS_CONTROL
#    define DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST \
        STRINGIFY(DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST_ID)
#    define DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST \
        STRINGIFY(DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST_ID)

#    define DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST \
        STRINGIFY(DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST_ID)
#    define DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST \
        STRINGIFY(DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST_ID)
#endif

#ifdef UNIX
#    define DYNAMORIO_VAR_EXE_PATH "DYNAMORIO_EXE_PATH"
#    define DYNAMORIO_VAR_EXECVE "DYNAMORIO_POST_EXECVE"
#    define DYNAMORIO_VAR_EXECVE_LOGDIR "DYNAMORIO_EXECVE_LOGDIR"
#    define DYNAMORIO_VAR_NO_EMULATE_BRK "DYNAMORIO_NO_EMULATE_BRK"
#    define L_IF_WIN(x) x

#else /* WINDOWS */

#    define EXPAND_LEVEL(str) str /* expand one level */
#    define L_EXPAND_LEVEL(str) L(str)
#    define L(str) L##str
#    define LCONCAT(wstr, str) EXPAND_LEVEL(wstr) L("\\") L(str)
#    define L_IF_WIN(x) L_EXPAND_LEVEL(x)

/* unicode versions of shared names*/
#    define L_DYNAMORIO_VAR_CONFIGDIR L_EXPAND_LEVEL(DYNAMORIO_VAR_CONFIGDIR)
#    define L_DYNAMORIO_VAR_HOME L_EXPAND_LEVEL(DYNAMORIO_VAR_HOME)
#    define L_DYNAMORIO_VAR_LOGDIR L_EXPAND_LEVEL(DYNAMORIO_VAR_LOGDIR)
#    define L_DYNAMORIO_VAR_OPTIONS L_EXPAND_LEVEL(DYNAMORIO_VAR_OPTIONS)
#    define L_DYNAMORIO_VAR_AUTOINJECT L_EXPAND_LEVEL(DYNAMORIO_VAR_AUTOINJECT)
#    define L_DYNAMORIO_VAR_ALTINJECT L_EXPAND_LEVEL(DYNAMORIO_VAR_ALTINJECT)
#    define L_DYNAMORIO_VAR_UNSUPPORTED L_EXPAND_LEVEL(DYNAMORIO_VAR_UNSUPPORTED)
#    define L_DYNAMORIO_VAR_RUNUNDER L_EXPAND_LEVEL(DYNAMORIO_VAR_RUNUNDER)
#    define L_DYNAMORIO_VAR_CMDLINE L_EXPAND_LEVEL(DYNAMORIO_VAR_CMDLINE)
#    define L_DYNAMORIO_VAR_ONCRASH L_EXPAND_LEVEL(DYNAMORIO_VAR_ONCRASH)
#    define L_DYNAMORIO_VAR_SAFEMARKER L_EXPAND_LEVEL(DYNAMORIO_VAR_SAFEMARKER)
#    define L_DYNAMORIO_VAR_CACHE_ROOT L_EXPAND_LEVEL(DYNAMORIO_VAR_CACHE_ROOT)
#    define L_DYNAMORIO_VAR_CACHE_SHARED L_EXPAND_LEVEL(DYNAMORIO_VAR_CACHE_SHARED)
#    ifdef HOT_PATCHING_INTERFACE
#        define L_DYNAMORIO_VAR_HOT_PATCH_POLICIES \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_HOT_PATCH_POLICIES)
#        define L_DYNAMORIO_VAR_HOT_PATCH_MODES \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_HOT_PATCH_MODES)
#    endif
#    ifdef PROCESS_CONTROL
#        define L_DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_APP_PROCESS_ALLOWLIST)
#        define L_DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_ANON_PROCESS_ALLOWLIST)

#        define L_DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_APP_PROCESS_BLOCKLIST)
#        define L_DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST \
            L_EXPAND_LEVEL(DYNAMORIO_VAR_ANON_PROCESS_BLOCKLIST)
#    endif

#    define L_PRODUCT_NAME L_EXPAND_LEVEL(PRODUCT_NAME)
#    define L_COMPANY_NAME L_EXPAND_LEVEL(COMPANY_NAME)
#    define L_COMPANY_LONG_NAME L_EXPAND_LEVEL(COMPANY_LONG_NAME)

/* event log registry keys */
#    define EVENTLOG_HIVE HKEY_LOCAL_MACHINE
#    define EVENTLOG_NAME COMPANY_NAME_EVENTLOG
#    define EVENTSOURCE_NAME                                  \
        PRODUCT_NAME /* should be different than logfile name \
                      */

#    define EVENTLOG_REGISTRY_SUBKEY "System\\CurrentControlSet\\Services\\EventLog"
#    define L_EVENTLOG_REGISTRY_SUBKEY L_EXPAND_LEVEL(EVENTLOG_REGISTRY_SUBKEY)
#    define L_EVENTLOG_REGISTRY_KEY \
        L"\\Registry\\Machine\\" L_EXPAND_LEVEL(EVENTLOG_REGISTRY_SUBKEY)
#    define L_EVENT_LOG_KEY LCONCAT(L_EVENTLOG_REGISTRY_KEY, EVENTLOG_NAME)
#    define L_EVENT_LOG_SUBKEY LCONCAT(L_EVENTLOG_REGISTRY_SUBKEY, EVENTLOG_NAME)
#    define L_EVENT_LOG_NAME L_EXPAND_LEVEL(EVENTLOG_NAME)
#    define L_EVENT_SOURCE_NAME L_EXPAND_LEVEL(EVENTSOURCE_NAME)
#    define L_EVENT_SOURCE_KEY LCONCAT(L_EVENT_LOG_KEY, EVENTSOURCE_NAME)
#    define L_EVENT_SOURCE_SUBKEY LCONCAT(L_EVENT_LOG_SUBKEY, EVENTSOURCE_NAME)

#    define EVENT_LOG_KEY LCONCAT(L_EXPAND_LEVEL(EVENTLOG_REGISTRY_SUBKEY), EVENTLOG_NAME)
#    define EVENT_SOURCE_KEY LCONCAT(EVENT_LOG_KEY, EVENTSOURCE_NAME)
/* Log key values (NOTE the values here are the values our installer uses,
 * not sure what all of them mean).  FIXME would be nice if these were
 * shared with the installer config file. Only used by DRcontrol (via
 * share/config.c) to set up new eventlogs (mainly for vista where our
 * installer doesn't work yet xref case 8482).*/
#    define L_EVENT_FILE_VALUE_NAME L"File"
#    define L_EVENT_FILE_NAME_PRE_VISTA \
        L"%SystemRoot%\\system32\\config\\" L_EXPAND_LEVEL(EVENTLOG_NAME) L".evt"
#    define L_EVENT_FILE_NAME_VISTA \
        L"%SystemRoot%\\system32\\winevt\\logs\\" L_EXPAND_LEVEL(EVENTLOG_NAME) L".elf"
#    define L_EVENT_MAX_SIZE_NAME L"MaxSize"
#    define EVENT_MAX_SIZE 0x500000
#    define L_EVENT_RETENTION_NAME L"Retention"
#    define EVENT_RETENTION 0
/* Src key values */
#    define L_EVENT_TYPES_SUPPORTED_NAME L"TypesSupported"
#    define EVENT_TYPES_SUPPORTED 0x7 /* info | warning | error */
#    define L_EVENT_CATEGORY_COUNT_NAME L"CategoryCount"
#    define EVENT_CATEGORY_COUNT 0
#    define L_EVENT_CATEGORY_FILE_NAME L"CategoryMessageFile"
#    define L_EVENT_MESSAGE_FILE L"EventMessageFile"

/* shared object directory base */
#    define BASE_NAMED_OBJECTS L"\\BaseNamedObjects"
/* base root in global object namespace, not in BaseNamedObjects or Sessions */
#    define DYNAMORIO_SHARED_OBJECT_BASE L("\\") L_EXPAND_LEVEL(COMPANY_NAME)
/* shared object directory for shared DLL cache */
#    define DYNAMORIO_SHARED_OBJECT_DIRECTORY \
        LCONCAT(DYNAMORIO_SHARED_OBJECT_BASE, "SharedCache")

/* registry */
#    define DYNAMORIO_REGISTRY_BASE_SUBKEY "Software\\" COMPANY_NAME "\\" PRODUCT_NAME
#    define DYNAMORIO_REGISTRY_BASE                                             \
        L"\\Registry\\Machine\\Software\\" L_EXPAND_LEVEL(COMPANY_NAME) L("\\") \
            L_EXPAND_LEVEL(PRODUCT_NAME)
#    define DYNAMORIO_REGISTRY_HIVE HKEY_LOCAL_MACHINE
#    define DYNAMORIO_REGISTRY_KEY DYNAMORIO_REGISTRY_BASE_SUBKEY
#    define L_DYNAMORIO_REGISTRY_KEY \
        L"Software\\" L_EXPAND_LEVEL(COMPANY_NAME) L"\\" L_EXPAND_LEVEL(PRODUCT_NAME)

#    define INJECT_ALL_HIVE HKEY_LOCAL_MACHINE
#    define INJECT_ALL_KEY "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows"
#    define INJECT_ALL_SUBKEY "AppInit_DLLs"
/* introduced on Vista */
#    define INJECT_ALL_LOAD_SUBKEY "LoadAppInit_DLLs"
/* introduced on Windows 7/2008 R2 */
#    define INJECT_ALL_SIGN_SUBKEY "RequireSignedAppInit_DLLs"

#    define INJECT_ALL_HIVE_L L"\\Registry\\Machine\\"
#    define INJECT_ALL_KEY_L L_EXPAND_LEVEL(INJECT_ALL_KEY)
#    define INJECT_ALL_SUBKEY_L L_EXPAND_LEVEL(INJECT_ALL_SUBKEY)
#    define INJECT_ALL_LOAD_SUBKEY_L L_EXPAND_LEVEL(INJECT_ALL_LOAD_SUBKEY)
#    define INJECT_ALL_SIGN_SUBKEY_L L_EXPAND_LEVEL(INJECT_ALL_SIGN_SUBKEY)

#    define INJECT_DLL_NAME "drpreinject.dll"
#    define INJECT_DLL_8_3_NAME "DRPREI~1.DLL"

#    define INJECT_HELPER_DLL1_NAME "drearlyhelp1.dll"
#    define INJECT_HELPER_DLL2_NAME "drearlyhelp2.dll"

#    define DEBUGGER_INJECTION_HIVE HKEY_LOCAL_MACHINE
#    define DEBUGGER_INJECTION_KEY \
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"
#    define DEBUGGER_INJECTION_VALUE_NAME "Debugger"

#    define DEBUGGER_INJECTION_HIVE_L L"\\Registry\\Machine\\"
#    define DEBUGGER_INJECTION_KEY_L L_EXPAND_LEVEL(DEBUGGER_INJECTION_KEY)
#    define DEBUGGER_INJECTION_VALUE_NAME_L L_EXPAND_LEVEL(DEBUGGER_INJECTION_VALUE_NAME)

#    define DRINJECT_NAME "drinject.exe"

/* shared module uses SVCHOST_NAME and EXE_SUFFIX separately */
#    define SVCHOST_NAME "svchost"
#    define EXE_SUFFIX ".exe"
#    define L_EXE_SUFFIX L_EXPAND_LEVEL(EXE_SUFFIX)
#    define SVCHOST_EXE_NAME SVCHOST_NAME EXE_SUFFIX
#    define L_SVCHOST_EXE_NAME L_EXPAND_LEVEL(SVCHOST_NAME) L_EXPAND_LEVEL(EXE_SUFFIX)

/* for processview, etc */
#    define DYNAMORIO_LIBRARY_NAME "dynamorio.dll"
#    define DLLPATH_RELEASE "\\lib\\release\\" DYNAMORIO_LIBRARY_NAME
#    define DLLPATH_DEBUG "\\lib\\debug\\" DYNAMORIO_LIBRARY_NAME
#    define DLLPATH_PROFILE "\\lib\\profile\\" DYNAMORIO_LIBRARY_NAME

#    define L_DYNAMORIO_LIBRARY_NAME L_EXPAND_LEVEL(DYNAMORIO_LIBRARY_NAME)
#    define L_DLLPATH_RELEASE L"\\lib\\release\\" L_DYNAMORIO_LIBRARY_NAME
#    define L_DLLPATH_DEBUG L"\\lib\\debug\\" L_DYNAMORIO_LIBRARY_NAME
#    define L_DLLPATH_PROFILE L"\\lib\\profile\\" L_DYNAMORIO_LIBRARY_NAME

#    define INJECT_ALL_DLL_SUBPATH "\\lib\\" INJECT_DLL_8_3_NAME
#    define L_INJECT_ALL_DLL_SUBPATH L"\\lib\\" L_EXPAND_LEVEL(INJECT_DLL_8_3_NAME)

enum DLL_TYPE {
    DLL_NONE,
    DLL_UNKNOWN,
    DLL_RELEASE,
    DLL_DEBUG,
    DLL_PROFILE,
    DLL_CUSTOM,
    DLL_PATHHAS
};
#endif /* WINDOWS */

#ifdef STANDALONE_UNIT_TEST
#    define UNIT_TEST_EXE_NAME ("unit_tests" IF_WINDOWS(".exe"))
#endif

/* DYNAMORIO_RUNUNDER controls the injection technique and process naming,
 *  it is a bitmask of the values below:
 * RUNUNDER_ON:
 *  take over the app indicated by the corresponding app-specific
 *  subkey; when this is a global param, it only acts as a default for
 *  subkeys which don't explicitly set RUNUNDER. indicates current
 *  default takeover method via the preinjector / AppInit_DLLs
 * RUNUNDER_ALL:
 *  use as a global parameter only, for doing "run all". exclude with
 *  app-specific RUNUNDER_OFF.  You need to use in conjuction with ON
 *
 *
 * RUNUNDER_EXPLICIT:
 *  indicates that the app will use the alternate injection technique
 *  currently via -follow_explicit_children, but might change to drinject
 *  in the per-executable debugger registry key at some point
 *
 *
 * RUNUNDER_COMMANDLINE_MATCH:
 *  indicates that the process command line must exactly match the
 *  value in the DYNAMORIO_CMDLINE app-specific subkey, or else no
 *  takeover will be done.  Note that only a single instance of
 *  a given executable name can be controlled this way.
 *
 * RUNUNDER_COMMANDLINE_DISPATCH:
 *  marks that the processes with this executable name should be
 *  differentiated by their canonicalized commandline.  For example,
 *  different dllhost instances will get their own different subkeys,
 *  just the way the svchost.exe instances have full sets of option
 *  keys.
 *
 * RUNUNDER_COMMANDLINE_NO_STRIP:
 *  this flag is used only in conjunction with
 *  RUNUNDER_COMMANDLINE_DISPATCH.  Our default canonicalization rule
 *  strips the first argument on the commandline for historic reasons.
 *  Since that was the way we had previously hardcoded the names for
 *  say "svchost.exe-netsvcs".  In case we actually want to dispatch
 *  on a single argument - which would be the case for say msiexec.exe
 *  /v then this flag is needed.
 *
 * RUNUNDER_ONCE:
 *  is used by staging mode to specify that the executable corresponding
 *  to the current process shouldn't run under DR the next time, i.e.,
 *  turn off its RUNUNDER_ON flag after checking it for the current process.
 *  This is needed to prevent perpetual crash-&-boot cycles due to DR failure.
 *  See case 3702.
 **/
enum {
    /* FIXME: keep in mind that we only read decimal values */
    RUNUNDER_OFF = 0x00, /* 0 */
    RUNUNDER_ON = 0x01,  /* 1 */
    RUNUNDER_ALL = 0x02, /* 2 */

    /* Dummy field to keep track of which processes were RUNUNDER_EXPLICIT
     * before we moved to -follow_systemwide by default (for -early_injection)
     * (note this was the old RUNUNDER_EXPLICIT value) */
    RUNUNDER_FORMERLY_EXPLICIT = 0x04, /* 4 */

    RUNUNDER_COMMANDLINE_MATCH = 0x08,    /* 8 */
    RUNUNDER_COMMANDLINE_DISPATCH = 0x10, /* 16 */
    RUNUNDER_COMMANDLINE_NO_STRIP = 0x20, /* 32 */
    RUNUNDER_ONCE = 0x40,                 /* 64 */

    RUNUNDER_EXPLICIT = 0x80, /* 128 */
};

/* A bitmask of possible actions to take on a nudge.  Accessed via
 * NUDGE_GENERIC(name) Recommended to always pass -nudge opt so to
 * synchronize options first.  For many state transition nudge's
 * option change will trigger any other actions: e.g. start
 * protecting, simulate attack, etc..  Only pulse-like events that
 * should be acted on only once need separate handling here.  Not all
 * combinations are meaningful, and the order of execution is not
 * determined from the order in the definitions.
 */
/* To use as an iterator define NUDGE_DEF(name, comment) */

/* CAUTION: DO NOT change ordering of the nudge definitions for non-NYI, i.e.,
 *          implemented nudges.  These numbers correspond to specific masks
 *          that are used by the nodemanager/drcontrol (thus QA).  Will lead to
 *          a lot of unwanted confusion.
 */
#define NUDGE_DEFINITIONS()                                                    \
    /* Control nudges */                                                       \
    NUDGE_DEF(opt, "Synchronize dynamic options")                              \
    NUDGE_DEF(reset, "Reset code caches") /* flush & delete */                 \
    NUDGE_DEF(detach, "Detach")                                                \
    NUDGE_DEF(mode, "Liveshield mode update")                                  \
    NUDGE_DEF(policy, "Liveshield policy update")                              \
    NUDGE_DEF(lstats, "Liveshield statistics NYI")                             \
    NUDGE_DEF(process_control, "Process control nudge") /* Case 8594. */       \
    NUDGE_DEF(upgrade, "DR upgrade NYI case 4179")                             \
    NUDGE_DEF(kstats, "Dump kstats in log or kstat file NYI")                  \
    /* internal options */                                                     \
    NUDGE_DEF(stats, "Dump internal stats in logfiles NYI")                    \
    NUDGE_DEF(invalidate, "Invalidate code caches NYI") /* flush */            \
    /* stress testing */                                                       \
    NUDGE_DEF(recreate_pc, "Recreate PC NYI")                                  \
    NUDGE_DEF(recreate_state, "Recreate state NYI")                            \
    NUDGE_DEF(reattach, "Reattach - almost detach, NYI")                       \
    /* diagnostics */                                                          \
    NUDGE_DEF(diagnose, "Request diagnostic file NYI")                         \
    NUDGE_DEF(ldmp, "Dump core")                                               \
    NUDGE_DEF(freeze, "Freeze coarse units")                                   \
    NUDGE_DEF(persist, "Persist coarse units")                                 \
    /* client nudge */                                                         \
    NUDGE_DEF(client, "Client nudge")                                          \
    /* security testing */                                                     \
    NUDGE_DEF(violation, "Simulate a security violation")                      \
    /* ADD NEW NUDGE_DEFs only immediately above this line  */                 \
    /* Since these are used as a bitmask only 32 types can be supported:       \
     * but on Linux only 28.  If we want more we can simply use the client_arg \
     * field to multiplex.                                                     \
     */

typedef enum {
#define NUDGE_DEF(name, comment) NUDGE_DR_##name,
    NUDGE_DEFINITIONS()
#undef NUDGE_DEF
        NUDGE_DR_PARAMETRIZED_END
} nudge_generic_type_t;

/* note that these are bitmask values */
#define NUDGE_GENERIC(name) (1 << (NUDGE_DR_##name))

/* On Linux only 2 bits for this */
#define NUDGE_ARG_VERSION_1 1
#define NUDGE_ARG_CURRENT_VERSION NUDGE_ARG_VERSION_1

/* nudge_arg_t bitfield flags.
 * On UNIX we only have space for 2 bits for these.
 */
enum {
    NUDGE_IS_INTERNAL = 0x01, /* The nudge is internally generated. */
#ifdef UNIX
    NUDGE_IS_SUSPEND = 0x02, /* This is an internal SUSPEND_SIGNAL. */
#else
    NUDGE_NUDGER_FREE_STACK = 0x02, /* nudger will free the nudge thread's stack so the
                                     * nudge thread itself shouldn't */
    NUDGE_FREE_ARG = 0x04,          /* nudge arg is in a separate allocation and should
                                     * be freed by the nudge thread */
#endif
};

typedef struct {
#ifdef UNIX
    /* We only have room for 16 bytes that we control, 24 bytes total.
     * Note that the kernel does NOT copy the huge amount of padding
     * at the tail end of siginfo_t so we cannot use that.
     */
    int ignored1; /* siginfo_t.si_signo: kernel sets so we cannot use */
    /* These make up the siginfo_t.si_errno field.  Since version starts
     * at 1, this field will never be 0 for a nudge signal, but is always
     * 0 for a libc sigqueue()-generated signal.
     */
    uint nudge_action_mask : 28;
    uint version : 2;
    uint flags : 2;
    int ignored2; /* siginfo_t.si_code: has meaning to kernel so we avoid using */
#else
    uint version;                   /* version number for future proofing */
    uint nudge_action_mask;         /* drawn from NUDGE_DEFS above */
    uint flags;                     /* flags drawn from above enum */
#endif
    client_id_t client_id; /* unique ID identifying client */
    uint64 client_arg;     /* argument for a client nudge */
#ifdef WIN32
    /* Add future arguments for nudge actions here.
     * There is no room for more Linux arguments.
     */
#endif
} nudge_arg_t;

#ifdef UNIX
/* i#61/PR 211530: Linux nudges.
 * We pick a signal that is very unlikely to be sent asynchronously by
 * the app, and for which we can distinguish synch from asynch by
 * looking at the interrupted pc.
 */
#    define NUDGESIG_SIGNUM SIGILL
#endif

#ifdef HOT_PATCHING_INTERFACE
/* These type definitions define the hot patch interface between the core &
 * the node manager.
 * CAUTION: Any changes to this can break the hot patch interface between the
 *          core and the node manager.
 */

/* All hot patch policy IDs must be of the form XXXX.XXXX; this ID is used to
 * generate the threat ID for a given hot patch violation.
 */
#    define HOTP_POLICY_ID_LENGTH 9

#    include "probe_api.h"

typedef dr_probe_status_t hotp_inject_status_t;

/* Modes are at a policy level, not a vulnerability level, even though the
 * core organizes things at the vulnerability level.
 */
typedef enum {
    HOTP_MODE_OFF = 0,
    HOTP_MODE_DETECT = 1,
    HOTP_MODE_PROTECT = 2
} hotp_policy_mode_t;

/* This structure is used to form a table that contains the status of all
 * active policies.  This is separated out into a separate table, as opposed
 * to being part of the hotp_vul_info_t and thus part of the global
 * vulnerability table, because the node manager will be directly reading
 * this information from the core's memory.  Thus, this structure serves
 * as a container to expose only that data which will be needed by the node
 * manager from the core regarding hot patches.
 */
typedef struct {
    /* polciy_id is the same as the one in hotp_vul_t.  Duplicated because can't
     * have this pointing to the hopt_vul_t structure because the node manager
     * will have to chase the pointer for each element to read it rather than a
     * single block of memory.
     */
    char policy_id[HOTP_POLICY_ID_LENGTH + 1];
    hotp_inject_status_t inject_status;

    /* This is the same as the one in hotp_vul_t.  Duplicated for the same
     * reason policy_id (see above) was.  Can't have the hotp_vul_t structure
     * pointing here too because that struct/table needs to be initialized for
     * the policy status table to be created; catch-22.
     * Fix for case 5484, where the node manager wasn't able to tell if an
     * inject status was for a policy that was turned on or off.
     */
    hotp_policy_mode_t mode;
} hotp_policy_status_t;

/* Node manager should read either this struct first or the first two elements
 * in it.  Then it should read the rest.  Note: 'size' refers to the size of
 * both this struct and the size of the policy_status_array in bytes.
 */
typedef struct {
    /* This is the crc for the whole table, except itself, i.e., the crc
     * computation starts from 'size'.  This is needed otherwise writing the
     * crc value itself will change the crc of the table!  Can get past it
     * by doing crc twice, but it is needlessly expensive.
     *
     * CAUTION: don't not move crc and size around as it can cause both the
     * node manager & the core to break.
     */
    uint crc;
    uint size; /* Size of this struct plus the table. */
    uint num_policies;
    hotp_policy_status_t *policy_status_array;
} hotp_policy_status_table_t;

#endif /* ifdef HOT_PATCHING_INTERFACE */

/* These constants & macros are used by core, share and preinject, so this is
 * the only place they will build for win32 and linux! */
/* return codes for [gs]et_parameter style functions
 * failure == 0 for compatibility with d_r_get_parameter()
 * if GET_PARAMETER_NOAPPSPECIFIC is returned, that means the
 *  parameter returned is from the global options, because there
 *  was no app-specific key present.
 * Note: constants shouldn't be added to this enum without checking whether
 *       the macros below will work; they should if errors are all negative and
 *       valid codes are all positive.
 */
enum {
    GET_PARAMETER_BUF_TOO_SMALL = -1,
    GET_PARAMETER_FAILURE = 0,
    GET_PARAMETER_SUCCESS = 1,
    GET_PARAMETER_NOAPPSPECIFIC = 2,
    SET_PARAMETER_FAILURE = GET_PARAMETER_FAILURE,
    SET_PARAMETER_SUCCESS = GET_PARAMETER_SUCCESS
};
#define IS_GET_PARAMETER_FAILURE(x) ((x) <= 0)
#define IS_GET_PARAMETER_SUCCESS(x) ((x) > 0)

/* X86 only but no ifdef since hotp builds don't set that define */
/* User-mode machine context.
 * We have it here instead of in arch_exports.h so that we
 * can expose it to hotpatch_interface.h.
 *
 * If any field offsets are changed, or fields are added, update
 * the following:
 *   - OFFSET defines in arch/arch.h and their uses in fcache_{enter,return}
 *   - DR_MCONTEXT_SIZE and PUSH_DR_MCONTEXT in arch/x86.asm
 *   - clean call layout of dr_mcontext_t on the stack in arch/mangle.c
 *   - interception layout of dr_mcontext_t on the stack in win32/callback.c
 *   - context_to_mcontext (and vice versa) in win32/ntdll.c
 *   - sigcontext_to_mcontext (and vice versa) in unix/signal.c
 *   - dump_mcontext in arch/arch.c
 *   - inject_into_thread in win32/inject.c
 * Also, hotp_context_t exposes the dr_mcontext_t struct to hot patches,
 * so be careful when changing any field offsets.
 *
 * FIXME: remove eax-ebx-ecx-edx and use the local_state_t
 * version from within DR as well as from the ibl (case 3701).
 *
 * PR 264138: for xmm fields, we do NOT specify 16-byte alignment for
 * our own, to avoid the 8-byte padding at the end for 32-bit mode
 * that we don't want to manually insert for our hand-rolled structs
 * on the stack.
 * It's ok for the client version to have padding as dr_get_mcontext copies,
 * but we don't currently assume alignment of client-declared dr_mcontext_t,
 * so we have no alignment declarations there at the moment either.
 */

/* Internal machine context structure */
typedef struct _priv_mcontext_t {
#include "mcxtx_api.h"
} priv_mcontext_t;

#endif /* _GLOBALS_SHARED_H_ */
