/* **********************************************************
 * Copyright (c) 2010-2014 Google, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 VMware, Inc.  All rights reserved.
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
/* Copyright (c) 2002-2003 Massachusetts Institute of Technology */
/* Copyright (c) 2002 Hewlett-Packard Company */

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

// Bumped up to 0x0500 to support building with VS2010.
// Bailing on NT4 support for the GUI since no longer needed to launch
// apps: merely a stats viewer.
#if _MSC_VER >= 1800 /* VS2013+ */
// Our VS2013 build already does not support Win2K (i#1376)
#    define _WIN32_WINNT 0x0501 // VS2013 MFC requires at least this
#    define _WIN32_IE 0x0501
#else
#    define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K /* NTDDI_VERSION is set from this */
#    define _WIN32_IE 0x0500    /* _WIN32_IE_IE40: MFC requires at least this */
#endif

// use _s secure versions
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
// somehow the above define doesn't apply to DynamoRIOView.cpp,
// so we use both of these to avoid the warnings:
#define _CRT_SECURE_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#if !defined(AFX_STDAFX_H__E6657A4C_7B90_457B_A64A_935DB0D375FE__INCLUDED_)
#    define AFX_STDAFX_H__E6657A4C_7B90_457B_A64A_935DB0D375FE__INCLUDED_

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000

#    define VC_EXTRALEAN // Exclude rarely-used stuff from Windows headers

#    include <afxwin.h>   // MFC core and standard components
#    include <afxext.h>   // MFC extensions
#    include <afxdtctl.h> // MFC support for Internet Explorer 4 Common Controls
#    ifndef _AFX_NO_AFXCMN_SUPPORT
#        include <afxcmn.h> // MFC support for Windows Common Controls
#    endif                  // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_STDAFX_H__E6657A4C_7B90_457B_A64A_935DB0D375FE__INCLUDED_)
