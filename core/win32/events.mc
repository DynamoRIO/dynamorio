;// **********************************************************
;// Copyright (c) 2012-2021 Google, Inc.  All rights reserved.
;// Copyright (c) 2003-2010 VMware, Inc.  All rights reserved.
;// **********************************************************

;// Redistribution and use in source and binary forms, with or without
;// modification, are permitted provided that the following conditions are met:
;//
;// * Redistributions of source code must retain the above copyright notice,
;//   this list of conditions and the following disclaimer.
;//
;// * Redistributions in binary form must reproduce the above copyright notice,
;//   this list of conditions and the following disclaimer in the documentation
;//   and/or other materials provided with the distribution.
;//
;// * Neither the name of VMware, Inc. nor the names of its contributors may be
;//   used to endorse or promote products derived from this software without
;//   specific prior written permission.
;//
;// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;// ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
;// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
;// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
;// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
;// DAMAGE.

;// Copyright (c) 2011 Google, Inc.
;// Copyright (c) 2003-2007 Determina Corp.

;// events.mc messages for Event logging

;// FIXME: I don't seem to get the !d! format specifiers on Win2k - will need to rework this

;// CHECK: is there a buffer overflow possibility in these facilities?
;// CHECK: should we always have the * specifier for the width component of a %!s!

;// TODO: We have to figure how to get all of this export something nice for Linux syslog

;// DO not use TABS or COMMAS (,) in the messages
;// ADD NEW MESSAGES AT THE END

MessageIdTypedef=uint

;// defaults
SeverityNames=(
    Success=0x0
    Informational=0x1
    Warning=0x2
    Error=0x3
    )

;// FIXME: unclear on can we override these..
FacilityNames=(
    DRCore     =0x0FF   ; core messages
    Security   =0x7FF   ; security violations
)

;// default
;//FIXME: can't override - I still want to change the name of the .BIN files
;//LanguageNames=(English=1:EVMSG001)

;//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;// Categories
;// unused: we may be happy with message types for now
;// FIXME: if the FacilityName thing works, otherwise we'll use these
MessageId=0x1
Severity=Success
SymbolicName=MSG_CATEGORY_SECURITY
Language=English
Security
.

;//;;;;;;;;;;;;;;;;;;; Core
;// Info messages
MessageId = +1000
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_PRODUCT_VERSION
Language=English
%1!s! (R) %2!s! (R) %3!s! %4!s! installed.
.

;// Info messages
;// NOTE: All message regarding applications should have app name and pid as the
;// first 2 arguments - otherwise controller will break.
MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_PROCESS_START
Language=English
Starting application %1!s! (%2!s!)
MD5: %3!s!
.

;// Avoid the MD5 output for clients.
MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_PROCESS_START_CLIENT
Language=English
Starting application %1!s! (%2!s!)
.

;// Info messages
;// empty MessageId value means previous +1
MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_PROCESS_STOP
Language=English
Stopping application %1!s! (%2!s!)
.

;// Info messages
MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_DETACHING
Language=English
Detaching from application %1!s! (%2!s!)
.

;// Info messages
MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_ATTACHED
Language=English
Attached to %1!s! threads in application %2!s! (%3!s!)
.

MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INFO_RESET_IN_PROGRESS
Language=English
Resetting caches and non-persistent memory @ %1!s! fragments in application %2!s! (%3!s!).
.

;//;;;;;;;;;;;;;;;;;;; Security

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_SEC_VIOLATION_TERMINATED
Language=English
A security violation was intercepted in application %1!s! (%2!s!).
Threat ID: %3!s!.
Program terminated.
.

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_SEC_VIOLATION_CONTINUE
Language=English
A security violation was intercepted in application %1!s! (%2!s!).
Threat ID: %3!s!.
Running in test mode - program continuing.
Your system may have been compromised.
.

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_SEC_VIOLATION_THREAD
Language=English
A security violation was intercepted in application %1!s! (%2!s!).
Threat ID: %3!s!.
Program continuing after terminating faulty thread.
Some application functionality may be lost.
.

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_SEC_VIOLATION_EXCEPTION
Language=English
A security violation was intercepted in application %1!s! (%2!s!).
Threat ID: %3!s!.
Program continuing after throwing an exception.
Application may terminate.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_INTERNAL_SYSLOG_CRITICAL
Language=English
Application %1!s! (%2!s!).  Internal Critical Error: %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_INTERNAL_SYSLOG_ERROR
Language=English
Application %1!s! (%2!s!).  Internal Error: %3!s!
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_INTERNAL_SYSLOG_WARNING
Language=English
Application %1!s! (%2!s!).  Internal Warning: %3!s!
.

MessageId =
Severity = Informational
Facility = DRCore
SymbolicName = MSG_INTERNAL_SYSLOG_INFORMATION
Language=English
Application %1!s! (%2!s!).  Internal Information: %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_EXCEPTION
Language=English
Application %1!s! (%2!s!).  %3!s! %4!s! at PC %5!s!.  Please report this at %6!s!.  Program aborted.%7!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CLIENT_EXCEPTION
Language=English
Application %1!s! (%2!s!).  %3!s! %4!s! at PC %5!s!.  Please report this at %6!s!.  Program aborted.%7!s!
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_INVOKING_APP_HANDLER
Language=English
Invoking fault handler for application %1!s! (%2!s!).
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_APP_EXCEPTION
Language=English
Application %1!s! (%2!s!).  Application exception at PC %3!s!.  %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_OUT_OF_MEMORY
Language=English
Application %1!s! (%2!s!).  Out of memory.  Program aborted.  Source %3!s!, type %3!s!, code %4!s!.
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_MAX_PENDING_SIGNALS
Language=English
Application %1!s! (%2!s!).  Reached the soft maximum number (%3!s!) of pending signals: further accumulation prior to delivery may cause problems.  Consider raising -max_pending_signals.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_UNSUPPORTED_APPLICATION
Language=English
Application %1!s! (%2!s!) is not supported due to dll %3!s!.  Program aborted.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_WSL_UNSUPPORTED_FATAL
Language=English
Application %1!s! (%2!s!).  The Windows Subsystem for Linux is not yet supported due to missing segment support from the kernel.  Program aborted.
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_WARNING_REPORT_THRESHOLD
Language=English
Application %1!s! (%2!s!) has reached its report threshold.  No more violations will be logged.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_UNABLE_TO_CREATE_BASEDIR
Language=English
Application %1!s! (%2!s!) unable to create directory %3!s! for forensics file
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_WARNING_EMPTY_OR_NONEXISTENT_LOGDIR_KEY
Language=English
Application %1!s! (%2!s!) has no directory specified for forensics files
.

MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_UNSUPPORTED_OS_VERSION
Language=English
Application %1!s! (%2!s!) is running on an unsupported operating system (%3!s!)
.

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_SEC_FORENSICS
Language=English
Forensics file for security violation in application %1!s! (%2!s!) created at %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_OPTION_TOO_LONG_TO_PARSE
Language=English
Application %1!s! (%2!s!) option %3!s! is too long to parse. %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_OPTION_BAD_NUMBER_FORMAT
Language=English
Application %1!s! (%2!s!). Option parsing error : unrecognized number format %3!s!. %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_OPTION_UNKNOWN_SIZE_SPECIFIER
Language=English
Application %1!s! (%2!s!). Option parsing error : unrecognized size factor %3!s!. %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_OPTION_UNKNOWN_TIME_SPECIFIER
Language=English
Application %1!s! (%2!s!). Option parsing error : unrecognized time factor %3!s!. %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_OPTION_UNKNOWN
Language=English
Application %1!s! (%2!s!). Option parsing error : unknown option %3!s!. %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_CONFIG_FILE_INVALID
Language=English
Application %1!s! (%2!s!). Config file parsing error : invalid line %3!s!.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_BAD_OS_VERSION
Language=English
Application %1!s! (%2!s!) %3!s! does not run on %4!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_OPTION_VERIFICATION_RECURSION
Language=English
Application %1!s! (%2!s!) bad option string, unable to continue.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_TAMPERED_NTDLL
Language=English
Application %1!s! (%2!s!). System library ntdll.dll has been tampered with, unable to continue.
.

;#ifdef CHECK_RETURNS_SSE2
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CHECK_RETURNS_SSE2_XMM_USED
Language=English
Application %1!s! (%2!s!). Check returns using SSE2 assumption violated, app is using the xmm registers
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CHECK_RETURNS_SSE2_REQUIRES_SSE2
Language=English
Application %1!s! (%2!s!). Check returns using SSE2 requires that the processor support SSE2
.

;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_EXTERNAL_ERROR
Language=English
Application %1!s! (%2!s!) %3!s! usage error : %4!s!
.

MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_LDMP
Language=English
Core dump file for application %1!s! (%2!s!) created at %3!s!
.

;#ifdef HOT_PATCHING_INTERFACE
MessageId =
Severity = Informational
Facility = Security
SymbolicName = MSG_HOT_PATCH_VIOLATION
Language=English
A security violation was intercepted in application %1!s! (%2!s!).
Threat ID: %3!s!.
.

MessageID =
Severity = Error
Facility = DRCore
SymbolicName = MSG_HOT_PATCH_FAILURE
Language=English
A LiveShield Sentry failure was intercepted in application %1!s! (%2!s!) at address %3!s!.
.

;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_ERROR_REGISTRY_PARAMETER_TOO_LONG
Language=English
Application %1!s! (%2!s!). Error reading registry : registry parameter %3!s! exceeds maximum length.
.

;// FIXME - do we want a more cryptic error messages (such as the out of memory one) that
;// requires going back to Determina for resolution, or do we want the message to be
;// potentially actionable by the customer? FIXME - warning or error?
MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_LOW_ON_VMM_MEMORY
Language=English
Application %1!s! (%2!s!).  Potentially thrashing on low virtual memory; suggest increasing the -vm_size option for this application.
.
MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_LOW_ON_COMMITTABLE_MEMORY
Language=English
Application %1!s! (%2!s!).  Potentially thrashing on low available system memory; suggest increasing initial pagefile size or adding additional RAM to this machine.
.

;#ifdef PROCESS_CONTROL
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_PROC_CTL_HASH_LIST_TOO_LONG
Language = English
Application %1!s! (%2!s!). Process control hash list %3!s! has more than %4!s! MD5 hashes, so process control has been disable.  Try doubling -pc_num_hashes to start process control again.
.
;#endif

;#ifdef X64
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_HEAP_CONTRAINTS_UNSATISFIABLE
Language=English
Application %1!s! (%2!s!). Unable to place the heap in a manner that satisfies all 32bit displacement requirements.  Check -vm_base, -vm_offset, -heap_in_lower_4GB, and dll prefferred base addresses for issues.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_UNSUPPORTED_PROCESSOR_LAHF
Language=English
Application %1!s! (%2!s!). Unsupported processor: LAHF/SAHF instructions required in 64-bit mode.
.
;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CLIENT_LIBRARY_UNLOADABLE
Language=English
Application %1!s! (%2!s!). Unable to load client library: %3!s!%4!s!.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CLIENT_LIBRARY_WRONG_BITWIDTH
Language=English
Application %1!s! (%2!s!). Library has wrong bitwidth: %3!s!.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CLIENT_VERSION_INCOMPATIBLE
Language=English
Application %1!s! (%2!s!). Client library targets an incompatible API version and should be re-compiled.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_INSTRUMENTATION_TOO_LARGE
Language=English
Application %1!s! (%2!s!). Basic block or trace instrumentation exceeded maximum size.  Try lowering -max_bb_instrs and/or -max_trace_bbs.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_SYSENTER_NOT_SUPPORTED
Language=English
Application %1!s! (%2!s!). System calls using sysenter are not supported on this operating system.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_WAITING_FOR_DEBUGGER
Language=English
Application %1!s! (%2!s!). Waiting for debugger to attach.
.

;#ifdef VMX86_SERVER
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_DRVMKLIB_UNLOADABLE
Language=English
Application %1!s! (%2!s!). Error loading or using vmklib library: %3!s!.
.
;#endif


MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_TOO_MANY_TLS_MODS
Language=English
Max number of modules with tls variables exceeded.
.

;#ifdef UNIX
MessageId =
Severity = Warning
Facility = DRCore
SymbolicName = MSG_UNDEFINED_SYMBOL
Language=English
WARNING! symbol lookup error: %1!s! undefined symbol %2!s!
.
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_UNDEFINED_SYMBOL_REFERENCE
Language=English
ERROR: using undefined symbol!
.
;#endif

;#ifdef UNIX
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FIXED_MAP_OVERLAPS_DR
Language=English
Application %1!s! (%2!s!). A fixed memory map (%3!s!) overlaps with DynamoRIO libraries.
.
;#endif

;#ifdef UNIX
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_CANNOT_FIND_VFP_FRAME
Language=English
Application %1!s! (%2!s!). Cannot identify VFP frame offset.
.
;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_SYNCHRONIZE_THREADS
Language=English
Application %1!s! (%2!s!). Failed to synchronize with all threads when detaching.
.

;#ifdef UNIX
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_HANDLE_SIGNAL
Language=English
Application %1!s! (%2!s!). Cannot correctly handle received signal %3!s! in thread %4!s!: %5!s!.
.
;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_TAKE_OVER_THREADS
Language=English
Application %1!s! (%2!s!). Failed to take over all threads after multiple attempts.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_THREAD_TAKEOVER_TIMED_OUT
Language=English
Application %1!s! (%2!s!). Timed out attempting to take over one or more threads. %3!s!
.

;#ifdef UNIX
MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_FIND_DR_BOUNDS
Language=English
Application %1!s! (%2!s!). Failed to find DynamoRIO library bounds.
.
;#endif

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_SATISFY_W_XOR_X
Language=English
Application %1!s! (%2!s!). Failed to satisfy W^X policies.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_OUT_OF_VMM_CANNOT_USE_OS
Language=English
Application %1!s! (%2!s!). Out of contiguous memory. %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_AVX_512_SUPPORT_INCOMPLETE
Language=English
Application %1!s! (%2!s!): AVX-512 was detected at PC %3!s!. AVX-512 is not fully supported yet.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_RSEQ_BEHAVIOR_UNSUPPORTED
Language=English
Application %1!s! (%2!s!). Restartable sequence behavior is not supported: %3!s!.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FAILED_TO_ALLOCATE_TLS
Language=English
Application %1!s! (%2!s!). Unable to allocate TLS slots. %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_PRIVATE_LIBRARY_TLS_LIMIT_CROSSED
Language=English
Application %1!s! (%2!s!). Private library static TLS limit crossed: %3!s!
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_INJECTION_LIBRARY_MISSING
Language=English
Application %1!s! (%2!s!). The library %3!s! for child process injection is missing.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_FOLLOW_CHILD_FAILED
Language=English
Application %1!s! (%2!s!). Failed to follow into child process: %3!s!.
.

MessageId =
Severity = Error
Facility = DRCore
SymbolicName = MSG_STANDALONE_ALREADY
Language=English
Application %1!s! (%2!s!). Standalone mode is in progress: cannot switch to full mode.
.

;// ADD NEW MESSAGES HERE
