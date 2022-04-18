/* **********************************************************
 * Copyright (c) 2008 VMware, Inc.  All rights reserved.
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

#ifndef _LICENSE_DLG_H_
#define _LICENSE_DLG_H_

#if _MSC_VER > 1000
#    pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// LicEdit

class LicEdit : public CEdit {
    void
    OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
    void
    OnEnVscroll();
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CLicenseDlg dialog

class CLicenseDlg : public CPropertyPage {
    DECLARE_DYNCREATE(CLicenseDlg)

    // Construction
public:
    BOOL
    CheckWindowsVersion();
    BOOL
    OnInitDialog();
    CLicenseDlg();
    ~CLicenseDlg();

    // Dialog Data
    //{{AFX_DATA(CLicenseDlg)
    enum { IDD = IDD_LICENSE };
    LicEdit m_LicenseEdit;
    CString m_License;
    //}}AFX_DATA

    // Overrides
    // ClassWizard generate virtual function overrides
    //{{AFX_VIRTUAL(CLicenseDlg)
public:
    virtual BOOL
    OnSetActive();

protected:
    virtual void
    DoDataExchange(CDataExchange *pDX); // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    BOOL m_FirstTime;
    void
    UpdateScrollbar();
    // Generated message map functions
    //{{AFX_MSG(CLicenseDlg)
    afx_msg void
    OnAgree();
    afx_msg void
    OnDisagree();
    afx_msg void
    OnUnderstand();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the
// previous line.

#endif // _LICENSE_DLG_H_
