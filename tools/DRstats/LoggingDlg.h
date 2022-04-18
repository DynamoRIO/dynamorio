/* **********************************************************
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

#if !defined(AFX_LOGGINGDLG_H__A05FE00B_E413_49FE_AD2F_2062AE912991__INCLUDED_)
#    define AFX_LOGGINGDLG_H__A05FE00B_E413_49FE_AD2F_2062AE912991__INCLUDED_

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000
// LoggingDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLoggingDlg dialog

class CLoggingDlg : public CDialog {
    // Construction
public:
    int
    GetMask();
    int
    GetLevel();
    BOOL
    OnInitDialog();
    CLoggingDlg(int level, int mask, CWnd *pParent = NULL);

    // Dialog Data
    //{{AFX_DATA(CLoggingDlg)
    enum { IDD = IDD_LOGGING };
    CButton m_OKButton;
    CComboBox m_Verbosity;
    CString m_Mask;
    //}}AFX_DATA

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CLoggingDlg)
protected:
    virtual void
    DoDataExchange(CDataExchange *pDX); // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    CLoggingDlg(CWnd *pParent = NULL); // standard constructor
    BOOL
    SetMaskValue(int mask);
    int
    GetMaskValue();
    void
    VerifyMaskString();
    int m_Level;
    void
    CheckboxChange(int id);
    // hold final values
    int final_level;
    int final_mask;

    // Generated message map functions
    //{{AFX_MSG(CLoggingDlg)
    afx_msg void
    OnChangeEditMask();
    afx_msg void
    OnLogAll();
    afx_msg void
    OnLogVmareas();
    afx_msg void
    OnLogAsynch();
    afx_msg void
    OnLogCache();
    afx_msg void
    OnLogDispatch();
    afx_msg void
    OnLogEmit();
    afx_msg void
    OnLogFragment();
    afx_msg void
    OnLogHeap();
    afx_msg void
    OnLogInterp();
    afx_msg void
    OnLogLinks();
    afx_msg void
    OnLogMonitor();
    afx_msg void
    OnLogStats();
    afx_msg void
    OnLogSyscalls();
    afx_msg void
    OnLogThreads();
    afx_msg void
    OnLogTop();
    afx_msg void
    OnLogNone();
    virtual void
    OnOK();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_LOGGINGDLG_H__A05FE00B_E413_49FE_AD2F_2062AE912991__INCLUDED_)
