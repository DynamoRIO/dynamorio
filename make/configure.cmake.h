/* **********************************************************
 * Copyright (c) 2011-2017 Google, Inc.  All rights reserved.
 * Copyright (c) 2009-2010 VMware, Inc.  All rights reserved.
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

/* configure.cmake.h
 * processed by cmake to contain all configuration defines
 */
#ifndef _CONFIGURE_H_
#define _CONFIGURE_H_ 1

/* exposed options */
#cmakedefine INTERNAL
#cmakedefine DEBUG
#cmakedefine DRSTATS_DEMO

/* target */
#cmakedefine X86
#cmakedefine ARM
#cmakedefine AARCH64
#cmakedefine AARCHXX
#cmakedefine X64
#cmakedefine WINDOWS
#cmakedefine LINUX
#cmakedefine VMKERNEL
#cmakedefine MACOS
#cmakedefine ANDROID
#if defined(MACOS) || defined (LINUX) || defined(VMKERNEL) || defined(ANDROID)
# define UNIX
#endif

/* set by high-level VMAP/VMSAFE/VPS configurations */
#cmakedefine PROGRAM_SHEPHERDING
#cmakedefine CLIENT_INTERFACE
#cmakedefine APP_EXPORTS
#cmakedefine STRACE_CLIENT
#cmakedefine HOT_PATCHING_INTERFACE
#cmakedefine PROCESS_CONTROL
#cmakedefine GBOP

/* i#1801 for clang build */
#cmakedefine CLANG

/* for use by developers */
#cmakedefine KSTATS
#cmakedefine CALLPROF
#ifdef CALLPROF
/* XXX: perhaps should rename CALLPROF cmake var to CALL_PROFILE */
# define CALL_PROFILE
#endif
#cmakedefine PARAMS_IN_REGISTRY

/* when packaging */
#cmakedefine VERSION_NUMBER ${VERSION_NUMBER}
#cmakedefine VERSION_COMMA_DELIMITED ${VERSION_COMMA_DELIMITED}
#cmakedefine VERSION_NUMBER_INTEGER ${VERSION_NUMBER_INTEGER}
#cmakedefine OLDEST_COMPATIBLE_VERSION ${OLDEST_COMPATIBLE_VERSION}
#cmakedefine BUILD_NUMBER ${BUILD_NUMBER}
#cmakedefine UNIQUE_BUILD_NUMBER ${UNIQUE_BUILD_NUMBER}
#cmakedefine CUSTOM_PRODUCT_NAME "${CUSTOM_PRODUCT_NAME}"

/* features */
#cmakedefine HAVE_FVISIBILITY
#cmakedefine HAVE_TYPELIMITS_CONTROL
#cmakedefine ANNOTATIONS

/* typedef conflicts */
#cmakedefine DR_DO_NOT_DEFINE_bool
#cmakedefine DR_DO_NOT_DEFINE_byte
#cmakedefine DR_DO_NOT_DEFINE_int64
#cmakedefine DR_DO_NOT_DEFINE_MAX_MIN
#cmakedefine DR_DO_NOT_DEFINE_sbyte
#cmakedefine DR_DO_NOT_DEFINE_uint
#cmakedefine DR_DO_NOT_DEFINE_uint32
#cmakedefine DR_DO_NOT_DEFINE_uint64
#cmakedefine DR_DO_NOT_DEFINE_ushort
#cmakedefine DR__Bool_EXISTS
#if defined(UNIX) && !defined(CPP2ASM)
/* i#1812: our check for these types searches sys/types.h, which we include in
 * globals_shared.h, but some tests do not include that.  To get everything to
 * work consistently we include it here.
 */
# include <sys/types.h>
#endif

/* Issue 20: we need to know lib dirs for cross-arch execve */
#define LIBDIR_X64 ${INSTALL_LIB_X64}
#define LIBDIR_X86 ${INSTALL_LIB_X86}

/* i#955: private loader search paths */
#define DR_RPATH_SUFFIX "${DR_RPATH_SUFFIX}"

/* dependent defines */
/*
###################################
# definitions for conditional compilation
#
# Unix variants
#    $(D)HAVE_MEMINFO - set if any memory info is available from kernel,
#      whether from /proc/self/maps or a system call query.
#      if not set: issues w/ mem queries from signal handler (PR 287309)
#    $(D)HAVE_MEMINFO_MAPS   - set if /proc/self/maps is available
#    $(D)HAVE_MEMINFO_QUERY  - set if memory query syscall is available
#    $(D)HAVE_TLS       - set if any form of ldt or gdt entry can be claimed
#      if not set: client reg spill slots won't work, and may hit asserts
#      after fork.
#    $(D)HAVE_SIGALTSTACK - set if SYS_sigaltstack is available
#    $(D)INIT_TAKE_OVER - libdynamorio.so init() takes over so no preload needed
# not supported but still in code because may be useful later
#    $(D)STEAL_REGISTER
#    $(D)DCONTEXT_IN_EDI
#    $(D)ASSUME_NORMAL_EFLAGS - (NEVER ON) (causes errors w/ Microsoft-compiled apps)
# internal studies, not for general use
#    $(D)SHARING_STUDY
#    $(D)FRAGMENT_SIZES_STUDY
#    $(D)FOOL_CPUID
#    ($(D)NATIVE_RETURN: deprecated and now removed, along with:
#       NATIVE_RETURN_CALLDEPTH, NATIVE_RETURN_RET_IN_TRACES,
#       and NATIVE_RETURN_TRY_TO_PUT_APP_RETURN_PC_ON_STACK)
#    $(D)LOAD_DYNAMO_DEBUGBREAK
# profiling
#    no longer supported: $(D)PROFILE_LINKCOUNT $(D)LINKCOUNT_64_BITS
#    $(D)PROFILE_RDTSC
#    ($(D)PAPI - now deprecated)
#    $(D)WINDOWS_PC_SAMPLE - on for all Windows builds
#    $(D)KSTATS - on for INTERNAL, DEBUG, and PROFILE builds, use KSTATS=1 for
# release builds
#    $(D)PROGRAM_SHEPHERDING -  (always ON)
#         currently turns on code origins checks and diagnostics, eventually will also
#         turn on return-after-call and other restricted control transfer features
#    $(D)RETURN_AFTER_CALL  - (always ON) return only to instructions after seen calls
#    $(D)RCT_IND_BRANCH     - (experimental) indirect branch only to address taken
#                             entry points
#    $(D)DGC_DIAGNOSTICS
#    $(D)CHECK_RETURNS_SSE2 (experimental security feature)
#    $(D)CHECK_RETURNS_SSE2_EMIT (experimental unfinished)
#    $(D)DIRECT_CALL_CHECK  (experimental unfinished)
#    $(D)SIMULATE_ATTACK    - simulate security violations
#    $(D)GBOP - generic buffer overflow prevention via hooking APIs
# optimization of application
#    $(D)SIDELINE
#    no longer supported: $(D)SIDELINE_COUNT_STUDY
#    $(D)LOAD_TO_CONST - around loadtoconst.c, $(D)LTC_STATS

# optimization of dynamo
#    $(D)AVOID_EFLAGS  (uses instructions that don't modify flags)
#                      (defines ASSUME_NORMAL_EFLAGS)
#    ($(D)RETURN_STACK: deprecated and now removed)
#    $(D)TRACE_HEAD_CACHE_INCR   (incompatible with security FIXME:?)
#    $(D)DISALLOW_CACHE_RESIZING (use as temporary hack when developing)
# transparency
#    $(D)NOLIBC = doesn't link libc on windows, currently uses ntdll.dll libc
#      functions, NOLIBC=0 causes the core to be linked against libc and kernel32.dll
# external interface
#    $(D)CLIENT_INTERFACE
#    $(D)ANNOTATIONS -- optional instrumentation of binary annotations
#                       in the target program
#    $(D)DR_APP_EXPORTS
#    $(D)CUSTOM_EXIT_STUBS -- optional part of CLIENT_INTERFACE
#      we may want it for our own internal use too, though
#    $(D)CUSTOM_TRACES -- optional part of CLIENT_INTERFACE
#      has some sub-features that are aggressive and not supported by default:
#      $(D)CUSTOM_TRACES_RET_REMOVAL = support for removing inlined rets
#      $(D)CLIENT_SIDELINE = allows adaptive interface methods to be called
#                            from other threads safetly, performance hit
#                            requires CLIENT_INTERFACE
#    $(D)UNSUPPORTED_API -- part of 0.9.4 MIT API but not supported in current API
#    $(D)NOT_DYNAMORIO_CORE - should be defined by non core components sharing our code

#    $(D)FANCY_COUNTDOWN - (NOT IMPLEMENTED) countdown messagebox

# debugging
#    $(D)DEBUG for debug builds
#    $(D)DEBUG_MEMORY (on for DEBUG)
#    $(D)STACK_GUARD_PAGE (on for DEBUG)
#    $(D)DEADLOCK_AVOIDANCE (on for DEBUG) - enforce total rank order on locks
#    $(D)MUTEX_CALLSTACK - enable collecting callstack info, requires DEADLOCK_AVOIDANCE
#    $(D)HEAP_ACCOUNTING (on for DEBUG)

#    $(D)INTERNAL for features that are not intended to reach customer hands
#    $(D)VERBOSE=1 for verbose debugging or in situations where normal DEBUG

# statistics
#    $(D)HASHTABLE_STATISTICS - IBL table statistics

# target platforms
#    $(D)WINDOWS (avoid using _WIN32 used by cl)
#    $(D)UNIX
#    note that in many cases we use the else of WINDOWS to mean UNIX and vice versa
#    we're just starting to add VMKERNEL and MACOS support
#    $(D)X86
#    $(D)X64

# support for running in x86 emulator on IA-64
#    $(D)IA32_ON_IA64

# build script provides these
#    $(D)BUILD_NUMBER (<64K == vmware's PRODUCT_BUILD_NUMBER)
#    $(D)UNIQUE_BUILD_NUMBER (== vmware's BUILD_NUMBER)
#    $(D)VERSION_NUMBER
#    $(D)VERSION_COMMA_DELIMITED

###################################
*/

#ifdef WINDOWS
   /* we do not support linking to libc.  we should probably remove
    * this define from the code and eliminate it altogether.
    */
# define NOLIBC
#endif

#ifdef MACOS
# define ASSEMBLE_WITH_NASM
#elif defined(UNIX)
# define ASSEMBLE_WITH_GAS
#else
# define ASSEMBLE_WITH_MASM
#endif

/* operating system */
#ifdef UNIX

# ifdef VMKERNEL
#  define VMX86_SERVER
#  define USERLEVEL
   /* PR 361894/388563: only on ESX4.1+ */
#  define HAVE_TLS
# elif defined(MACOS)
#  define MACOS
#  define HAVE_MEMINFO
#  define HAVE_MEMINFO_QUERY
#  define HAVE_TLS
#  define HAVE_SIGALTSTACK
# elif defined(LINUX)
#  define HAVE_MEMINFO
#  define HAVE_MEMINFO_MAPS
#  define HAVE_TLS
#  define HAVE_SIGALTSTACK
# else
#  error Unknown operating system
# endif

# ifdef HAVE_FVISIBILITY
#  define USE_VISIBILITY_ATTRIBUTES
# endif
#endif

#ifdef WINDOWS
# define HAVE_MEMINFO
# define HAVE_MEMINFO_QUERY
# define WINDOWS_PC_SAMPLE
/* i#1424: avoid pulling in features from recent versions to keep compatibility */
# ifndef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_NT4
# endif
#endif

#ifdef PROGRAM_SHEPHERDING
# define RETURN_AFTER_CALL
# define RCT_IND_BRANCH
#endif

#ifdef CLIENT_INTERFACE
  /* standard client interface features */
# define DYNAMORIO_IR_EXPORTS
# define CUSTOM_TRACES
# define CLIENT_SIDELINE
  /* PR 200409: not part of our current API, xref PR 215179 on -pad_jmps
   * issues with CUSTOM_EXIT_STUBS
# define CUSTOM_EXIT_STUBS
# define UNSUPPORTED_API
   */
#endif

#if defined(HOT_PATCHING_INTERFACE) && defined(CLIENT_INTERFACE)
# define PROBE_API
#endif

#if defined(PROGRAM_SHEPHERDING) && defined(CLIENT_INTERFACE)
/* used by libutil and tools */
# define MF_API
# define PROBE_API
#endif

#ifdef APP_EXPORTS
# define DR_APP_EXPORTS
#endif

/* FIXME: some GBOP hooks depend on hotp_only HOT_PATCHING_INTERFACE */

#ifdef DEBUG
   /* for bug fixing this is useful so we turn on for all debug builds */
# define DEBUG_MEMORY
# define STACK_GUARD_PAGE
# define HEAP_ACCOUNTING
# define DEADLOCK_AVOIDANCE
# define MUTEX_CALLSTACK /* requires DEADLOCK_AVOIDANCE */
  /* even though only usable in all-private config useful in default builds */
# define SHARING_STUDY
# define HASHTABLE_STATISTICS
#endif

#endif /* _CONFIGURE_H_ */
