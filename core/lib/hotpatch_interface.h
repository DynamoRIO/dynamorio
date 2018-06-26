/* **********************************************************
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

#ifndef _HOTPATCH_INTERFACE_H_
#define _HOTPATCH_INTERFACE_H_ 1

/* This file defines the interface between the core and the hot patches.  Any
 * changes to this file will most likely require changes to both.
 */

#include "globals_shared.h" /* for reg_t */

/* CAUTION: The hot patch interface (i.e., engine) version should be kept in
 * synch with the hot patches (hotpatch module) any time a new engine version
 * is defined.
 */
#define HOTP_INTERFACE_VERSION 42000

/* This enum specifies the possible status codes that a hot patch routine can
 * return to convey how its execution proceeded.  This has to be a bit flag
 * because event logging requests by a protector can be combined with other
 * status codes.
 *
 * CAUTION: Changes to this enum will break hot patch code; all hot patch code
 *          has to be recompiled and hot patching engine version probably has
 *          to be upgraded.  Be sure to talk with Alex.
 */
typedef enum {
    /* Detector status codes. */
    HOTP_EXEC_EXPLOIT_DETECTED = 0x00000001,
    HOTP_EXEC_EXPLOIT_NOT_DETECTED = 0x00000002,
    HOTP_EXEC_DETECTOR_ERROR = 0x00000004,

    /* All codes below can only be returned by a protector. */
    HOTP_EXEC_EXPLOIT_PROTECTED = 0x00000008,
    HOTP_EXEC_EXPLOIT_NOT_PROTECTED = 0x00000010,
    HOTP_EXEC_PROTECTOR_ERROR = 0x00000020,

    /* These codes return both a status & request for a particular action. */
    HOTP_EXEC_EXPLOIT_KILL_THREAD = 0x00000040,
    HOTP_EXEC_EXPLOIT_KILL_PROCESS = 0x00000080,
    HOTP_EXEC_EXPLOIT_RAISE_EXCEPTION = 0x00000100,
    HOTP_EXEC_CHANGE_CONTROL_FLOW = 0x00000200,

    /* This flag is orthogonal to the ones above and can be specified with
     * any of those.  This can be used both by detectors and protectors.
     */
    HOTP_EXEC_LOG_EVENT = 0x00000400,

    /* This status shouldn't be returned by a hot patch code.  It is used to
     * identify unexpected aborts of hot patch code, mostly due to exceptions.
     */
    HOTP_EXEC_ABORTED = 0x00000800
} hotp_exec_status_t;

typedef priv_mcontext_t hotp_context_t;

/* TODO: may have to define one for detector & one for protector because their
 *       interface types will be different once the protector logging is done.
 */
typedef hotp_exec_status_t (*hotp_func_t)(hotp_context_t *app_reg_ptr);

#define APP_XDI(x) (((hotp_context_t *)(x))->xdi)
#define APP_XSI(x) (((hotp_context_t *)(x))->xsi)
#define APP_XBP(x) (((hotp_context_t *)(x))->xbp)
#define APP_XSP(x) (((hotp_context_t *)(x))->xsp)
#define APP_XBX(x) (((hotp_context_t *)(x))->xbx)
#define APP_XDX(x) (((hotp_context_t *)(x))->xdx)
#define APP_XCX(x) (((hotp_context_t *)(x))->xcx)
#define APP_XAX(x) (((hotp_context_t *)(x))->xax)
#define APP_R8(x) (((hotp_context_t *)(x))->r8)
#define APP_R9(x) (((hotp_context_t *)(x))->r9)
#define APP_R10(x) (((hotp_context_t *)(x))->r10)
#define APP_R11(x) (((hotp_context_t *)(x))->r11)
#define APP_R12(x) (((hotp_context_t *)(x))->r12)
#define APP_R13(x) (((hotp_context_t *)(x))->r13)
#define APP_R14(x) (((hotp_context_t *)(x))->r14)
#define APP_R15(x) (((hotp_context_t *)(x))->r15)
#define APP_XFLAGS(x) (((hotp_context_t *)(x))->xflags)

#ifndef X64 /* legacy support */
#    define APP_EDI APP_XDI
#    define APP_ESI APP_XSI
#    define APP_EBP APP_XBP
#    define APP_ESP APP_XSP
#    define APP_EBX APP_XBX
#    define APP_EDX APP_XDX
#    define APP_ECX APP_XCX
#    define APP_EAX APP_XAX
#endif

#endif /* _HOTPATCH_INTERFACE_H_ */
