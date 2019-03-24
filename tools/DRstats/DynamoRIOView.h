/* **********************************************************
 * Copyright (c) 2013 Google, Inc.  All rights reserved.
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

// DynamoRIOView.h : interface of the CDynamoRIOView class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DYNAMORIOVIEW_H__93D53B0D_7233_4E40_8240_C733CC5C6DF8__INCLUDED_)
#    define AFX_DYNAMORIOVIEW_H__93D53B0D_7233_4E40_8240_C733CC5C6DF8__INCLUDED_

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000

extern "C" {
#    include "share.h"
#    include "processes.h"
};

/* client stats -- FIXME: put this stuff in shared header file, for
 * now duplicated here and in client
 */
#    include "afxwin.h"
/* We use "Local\" to avoid needing to be admin on Vista+.  This limits
 * viewing to processes in the same session.
 * Prefixes are not supported on NT.
 */
#    define CLIENT_SHMEM_KEY_NT "DynamoRIO_Client_Statistics"
#    define CLIENT_SHMEM_KEY "Local\\DynamoRIO_Client_Statistics"
#    define CLIENTSTAT_NAME_MAX_LEN 47

/* we allocate this struct in the shared memory: */
typedef struct _client_stats {
    uint num_stats;
    bool exited;
    process_id_t pid;
    /* num_stats strings, each STAT_NAME_MAX_LEN chars, followed by
     * num_stats values of type stats_int_t
     */
    char data[CLIENTSTAT_NAME_MAX_LEN];
} client_stats;

class CDynamoRIOView : public CFormView {
protected: // create from serialization only
    CDynamoRIOView();
    DECLARE_DYNCREATE(CDynamoRIOView)

public:
    //{{AFX_DATA(CDynamoRIOView)
    enum {
#    ifdef DRSTATS_DEMO
        IDD = IDD_DRSTATS_DEMO_FORM
#    else
        IDD = IDD_DYNAMORIO_FORM
#    endif
    };
    //}}AFX_DATA

    // Attributes
public:
    CDynamoRIODoc *
    GetDocument();
    BOOL
    SelectProcess(int pid);

    // Operations
public:
    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CDynamoRIOView)
public:
    virtual BOOL
    PreCreateWindow(CREATESTRUCT &cs);

protected:
    virtual void
    DoDataExchange(CDataExchange *pDX); // DDX/DDV support
    virtual void
    OnInitialUpdate(); // called first time after construct
    virtual BOOL
    OnPreparePrinting(CPrintInfo *pInfo);
    virtual void
    OnBeginPrinting(CDC *pDC, CPrintInfo *pInfo);
    virtual void
    OnEndPrinting(CDC *pDC, CPrintInfo *pInfo);
    virtual void
    OnPrint(CDC *pDC, CPrintInfo *pInfo);
    //}}AFX_VIRTUAL

    // Implementation
public:
    BOOL
    UpdateProcessList();
    BOOL
    Refresh();
    virtual ~CDynamoRIOView();
#    ifdef _DEBUG
    virtual void
    AssertValid() const;
    virtual void
    Dump(CDumpContext &dc) const;
#    endif

protected:
    void
    ZeroStrings();
    void
    ClearData();
    void
    EnumerateInstances();
    uint
    PrintStat(TCHAR *c, uint i, BOOL filter);
    uint
    PrintClientStats(TCHAR *c, TCHAR *max);

    dr_statistics_t *m_stats;
    CComboBox m_ProcessList;
    HANDLE m_clientMap;
    PVOID m_clientView;
    client_stats *m_clientStats;
    DWORD m_list_pos;
    DWORD m_selected_pid;
    BOOL m_windows_NT;

    // statistics
    CString m_Exited;
#    ifndef DRSTATS_DEMO
    CString m_LogLevel;
    CString m_LogMask;
    CString m_LogDir;
#    endif
    CEdit m_StatsCtl;
    CScrollBar m_StatsSB;
    CSliderCtrl m_StatsSlider;
    DWORD m_StatsViewLines;
    CString m_ClientStats;

    // Generated message map functions
protected:
    //{{AFX_MSG(CDynamoRIOView)
    afx_msg void
    OnSelchangeList();
    afx_msg void
    OnDropdownList();
#    ifndef DRSTATS_DEMO
    afx_msg void
    OnChangeLogging();
    afx_msg void
    OnLogDirExplore();
#    endif
    afx_msg void
    OnEditCopystats();
    afx_msg void
    OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
    friend BOOL
    pw_callback_under_dr(process_info_t *pi, void **param);
};

#    ifndef _DEBUG // debug version in DynamoRIOView.cpp
inline CDynamoRIODoc *
CDynamoRIOView::GetDocument()
{
    return (CDynamoRIODoc *)m_pDocument;
}
#    endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_DYNAMORIOVIEW_H__93D53B0D_7233_4E40_8240_C733CC5C6DF8__INCLUDED_)
