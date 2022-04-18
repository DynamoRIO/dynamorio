/* **********************************************************
 * Copyright (c) 2011-2015 Google, Inc.  All rights reserved.
 * Copyright (c) 2000-2010 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2001-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2000-2001 Hewlett-Packard Company */

/*
 * os_public.h - Win32 defines shared with tests
 */

#ifndef _OS_PUBLIC_H_
#define _OS_PUBLIC_H_ 1

/* CONTEXT field name changes: before including arch_exports.h, for cxt_seg_t! */
#ifdef X64
typedef ushort cxt_seg_t;
#    define CXT_XIP Rip
#    define CXT_XAX Rax
#    define CXT_XCX Rcx
#    define CXT_XDX Rdx
#    define CXT_XBX Rbx
#    define CXT_XSP Rsp
#    define CXT_XBP Rbp
#    define CXT_XSI Rsi
#    define CXT_XDI Rdi
/* It looks like both CONTEXT.Xmm0 and CONTEXT.FltSave.XmmRegisters[0] are filled
 * in. We use the latter so that we don't have to hardcode the index.
 */
#    define CXT_XMM(cxt, idx) ((dr_xmm_t *)&((cxt)->FltSave.XmmRegisters[idx]))
/* FIXME i#437: need CXT_YMM */
/* they kept the 32-bit EFlags field; sure, the upper 32 bits of Rflags
 * are undefined right now, but doesn't seem very forward-thinking. */
#    define CXT_XFLAGS EFlags
#else
typedef DWORD cxt_seg_t;
#    define CXT_XIP Eip
#    define CXT_XAX Eax
#    define CXT_XCX Ecx
#    define CXT_XDX Edx
#    define CXT_XBX Ebx
#    define CXT_XSP Esp
#    define CXT_XBP Ebp
#    define CXT_XSI Esi
#    define CXT_XDI Edi
#    define CXT_XFLAGS EFlags
/* This is not documented, but CONTEXT.ExtendedRegisters looks like fxsave layout.
 * Presumably there are no processors that have SSE but not FXSR
 * (we ASSERT on that in proc_init()).
 */
#    define FXSAVE_XMM0_OFFSET 160
#    define CXT_XMM(cxt, idx) \
        ((dr_xmm_t *)&((cxt)->ExtendedRegisters[FXSAVE_XMM0_OFFSET + (idx)*16]))
#endif

#endif /* _OS_PUBLIC_H_ */
