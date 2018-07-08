/* **********************************************************
 * Copyright (c) 2014 Google, Inc.  All rights reserved.
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

// DynamoRIO.h : main header file for the DYNAMORIO application
//

#if !defined(AFX_DYNAMORIO_H__23EBE600_BBF9_4F81_B49E_8CA6E806E7F8__INCLUDED_)
#    define AFX_DYNAMORIO_H__23EBE600_BBF9_4F81_B49E_8CA6E806E7F8__INCLUDED_

#    include "configure.h"

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000

#    ifndef __AFXWIN_H__
#        error include 'stdafx.h' before including this file for PCH
#    endif

#    include "resource.h" // main symbols
#    include "MainFrm.h"  // Added by ClassView
#    include "DynamoRIODoc.h"
#    include "DynamoRIOView.h"

#    define MYMBFLAGS MB_TOPMOST | MB_ICONEXCLAMATION

#    ifdef UNICODE
#        define ASCII_PRINTF _T("S")
#        define UNICODE_PRINTF _T("s")
#    else
#        define ASCII_PRINTF _T("s")
#        define UNICODE_PRINTF _T("S")
#    endif

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOApp:
// See DynamoRIO.cpp for the implementation of this class
//

class CDynamoRIOApp : public CWinApp {
public:
#    ifndef DRSTATS_DEMO
    BOOL
    ConfigureForNewUser();
    void
    SetEnvVarPermanently(TCHAR *var, TCHAR *val);
    void
    PreExit();
    BOOL
    RunNewApp(LPCTSTR lpszFileName);
#    endif
    CDocument *
    OpenDocumentFile(LPCTSTR lpszFileName);
    CDynamoRIOApp();

    // global function for TimerProc inside view
    static CDynamoRIOView *
    GetActiveView();
    // global for sharing with the view
    static BOOL
    CDynamoRIOApp::GetWindowsVersion(OSVERSIONINFOW *version);
#    ifndef DRSTATS_DEMO
    // global function for systemwide status
    static BOOL
    SystemwideSet();
    // global function for current library to use
    static TCHAR *
    GetDllPath();
    // global function for current library type
    static DLL_TYPE
    GetDllType();
    // global function for registry setting
    static void
    SetSystemwideSetting(int val);
    // global function for clean up at exit (InstanceExit is too late!)
    static void
    AboutToExit();
#    endif
    // global function for setting status bar text
    static void
    SetStatusbarText(TCHAR *txt);

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CDynamoRIOApp)
public:
    virtual BOOL
    InitInstance();
    virtual int
    ExitInstance();
    //}}AFX_VIRTUAL

    // Implementation
    //{{AFX_MSG(CDynamoRIOApp)
    afx_msg void
    OnAppAbout();
    afx_msg void
    OnAppExit();
#    ifndef DRSTATS_DEMO
    afx_msg void
    OnEditOptions();
    afx_msg void
    OnLibraryRelease();
    afx_msg void
    OnLibraryProfile();
    afx_msg void
    OnLibraryDebug();
    afx_msg void
    OnFileRun();
    afx_msg void
    OnFileSystemwide();
    afx_msg void
    OnHelpHelp();
    afx_msg void
    OnEditIgnorelist();
#    endif
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()

protected:
    BOOL
    CheckWindowsVersion(BOOL &windows_NT);
    CMainFrame *m_pMainFrame;
#    ifndef DRSTATS_DEMO
    void
    DisableMissingLibraries(BOOL notify);
    void
    DisableSystemwideInject();
    BOOL m_bSystemwideAllowed;
    BOOL
    SetSystemwideInject(TCHAR *val);
    BOOL m_bInjectAll;
    BOOL
    SwitchLibraries(TCHAR *newdllpath, BOOL notify);
    TCHAR m_dynamorio_home[_MAX_DIR];
    TCHAR m_inject_all_value[MAX_PATH];
    TCHAR m_dll_path[MAX_PATH];
    DLL_TYPE m_dll_type;
#    endif
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_DYNAMORIO_H__23EBE600_BBF9_4F81_B49E_8CA6E806E7F8__INCLUDED_)
