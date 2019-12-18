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

#if !defined(AFX_OPTIONSDLG_H__0BDD6058_E38D_4CEA_805C_DE47B633EEF9__INCLUDED_)
#    define AFX_OPTIONSDLG_H__0BDD6058_E38D_4CEA_805C_DE47B633EEF9__INCLUDED_

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000
// OptionsDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionsDlg dialog

class COptionsDlg : public CDialog {
    // Construction
public:
    BOOL
    CheckLibraryExists(TCHAR *libname, BOOL notify);
    static BOOL
    CheckOptionsVersusDllType(DLL_TYPE dll_type);
    COptionsDlg(CWnd *pParent = NULL); // standard constructor
    BOOL
    OnInitDialog();

    // Dialog Data
    //{{AFX_DATA(COptionsDlg)
    enum { IDD = IDD_OPTIONS };
    CButton m_OKButton;
    CString m_opstring;
    CString m_HotThreshold;
    CString m_InstrLibName;
    CString m_CacheBBMax;
    CString m_CacheTraceMax;
    CString m_LogLevel;
    CString m_LogMask;
    //}}AFX_DATA

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(COptionsDlg)
protected:
    virtual void
    DoDataExchange(CDataExchange *pDX); // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    BOOL
    UpdateValue(int param);
    void
    DisableCheckbox(int id);
    BOOL
    CheckOpstring();
    void
    RemoveOption(int param);
    void
    CheckOption(int param);

    // Generated message map functions
    //{{AFX_MSG(COptionsDlg)
    virtual void
    OnOK();
    afx_msg void
    OnHotThreshold();
    afx_msg void
    OnBrowseInstrlibname();
    afx_msg void
    OnInstrlibname();
    afx_msg void
    OnLoggingButton();
    afx_msg void
    OnChangeOptionsEdit();
    afx_msg void
    OnCacheBBMax();
    afx_msg void
    OnCacheTraceMax();
    afx_msg void
    OnLoglevel();
    afx_msg void
    OnLogmask();
    afx_msg void
    OnProfPcs();
    afx_msg void
    OnProfCounts();
    afx_msg void
    OnStats();
    afx_msg void
    OnNotify();
    afx_msg void
    OnNullcalls();
    afx_msg void
    OnNolink();
    afx_msg void
    OnNoasynch();
    afx_msg void
    OnTraceDumpText();
    afx_msg void
    OnTraceDumpBinary();
    afx_msg void
    OnTraceDumpOrigins();
    afx_msg void
    OnChangeEditInstrlibname();
    afx_msg void
    OnSetPermanent();
    afx_msg void
    OnChangeEditCacheBbMax();
    afx_msg void
    OnChangeEditCacheTraceMax();
    afx_msg void
    OnChangeEditHotThreshold();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_OPTIONSDLG_H__0BDD6058_E38D_4CEA_805C_DE47B633EEF9__INCLUDED_)
