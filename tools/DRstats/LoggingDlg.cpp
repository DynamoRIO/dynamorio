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

// LoggingDlg.cpp : implementation file
//

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "LoggingDlg.h"
#    include <assert.h>

#    ifdef _DEBUG
#        define new DEBUG_NEW
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    endif

// FIXME: duplicate information with utils.h...
enum {
    LOG_NONE = 0,
    LOG_STATS,
    LOG_TOP,
    LOG_THREADS,
    LOG_SYSCALLS,
    LOG_ASYNCH,
    LOG_INTERP,
    LOG_EMIT,
    LOG_LINKS,
    LOG_CACHE,
    LOG_FRAGMENT,
    LOG_DISPATCH,
    LOG_MONITOR,
    LOG_HEAP,
    LOG_VMAREAS,
    LOG_ALL,
};
static const int checkboxes[] = {
    IDC_LOG_NONE,     IDC_LOG_STATS,  IDC_LOG_TOP,      IDC_LOG_THREADS,
    IDC_LOG_SYSCALLS, IDC_LOG_ASYNCH, IDC_LOG_INTERP,   IDC_LOG_EMIT,
    IDC_LOG_LINKS,    IDC_LOG_CACHE,  IDC_LOG_FRAGMENT, IDC_LOG_DISPATCH,
    IDC_LOG_MONITOR,  IDC_LOG_HEAP,   IDC_LOG_VMAREAS,  IDC_LOG_ALL,
};
static const int masks[] = {
    0x00000000, // LOG_NONE
    0x00000001, // LOG_STATS
    0x00000002, // LOG_TOP
    0x00000004, // LOG_THREADS
    0x00000008, // LOG_SYSCALLS
    0x00000010, // LOG_ASYNCH
    0x00000020, // LOG_INTERP
    0x00000040, // LOG_EMIT
    0x00000080, // LOG_LINKS
    0x00000100, // LOG_CACHE
    0x00000200, // LOG_FRAGMENT
    0x00000400, // LOG_DISPATCH
    0x00000800, // LOG_MONITOR
    0x00001000, // LOG_HEAP
    0x00002000, // LOG_VMAREAS
    0x00003fff, // LOG_ALL
};
static const int num_options = sizeof(checkboxes) / sizeof(int);

/////////////////////////////////////////////////////////////////////////////
// CLoggingDlg dialog

CLoggingDlg::CLoggingDlg(CWnd *pParent /*=NULL*/)
    : CDialog(CLoggingDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CLoggingDlg)
    m_Mask = _T("3FFF");
    //}}AFX_DATA_INIT
    m_Level = 1;
}

CLoggingDlg::CLoggingDlg(int level, int mask, CWnd *pParent /*=NULL*/)
    : CDialog(CLoggingDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CLoggingDlg)
    //}}AFX_DATA_INIT
    if ((mask & 0xcfff0000) != 0) {
        ::MessageBox(NULL, _T("Mask must be between 0x0000 and 0x3fff"), _T("Warning"),
                     MB_OK | MYMBFLAGS);
        mask = 0x3fff;
    }
    m_Mask.Format(_T("%04X"), mask);
    m_Level = level;
}

void
CLoggingDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CLoggingDlg)
    DDX_Control(pDX, IDOK, m_OKButton);
    DDX_Control(pDX, IDC_VERBOSITY, m_Verbosity);
    DDX_Text(pDX, IDC_EDIT_MASK, m_Mask);
    DDV_MaxChars(pDX, m_Mask, 4);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CLoggingDlg, CDialog)
//{{AFX_MSG_MAP(CLoggingDlg)
ON_EN_CHANGE(IDC_EDIT_MASK, OnChangeEditMask)
ON_BN_CLICKED(IDC_LOG_ALL, OnLogAll)
ON_BN_CLICKED(IDC_LOG_VMAREAS, OnLogVmareas)
ON_BN_CLICKED(IDC_LOG_ASYNCH, OnLogAsynch)
ON_BN_CLICKED(IDC_LOG_CACHE, OnLogCache)
ON_BN_CLICKED(IDC_LOG_DISPATCH, OnLogDispatch)
ON_BN_CLICKED(IDC_LOG_EMIT, OnLogEmit)
ON_BN_CLICKED(IDC_LOG_FRAGMENT, OnLogFragment)
ON_BN_CLICKED(IDC_LOG_HEAP, OnLogHeap)
ON_BN_CLICKED(IDC_LOG_INTERP, OnLogInterp)
ON_BN_CLICKED(IDC_LOG_LINKS, OnLogLinks)
ON_BN_CLICKED(IDC_LOG_MONITOR, OnLogMonitor)
ON_BN_CLICKED(IDC_LOG_STATS, OnLogStats)
ON_BN_CLICKED(IDC_LOG_SYSCALLS, OnLogSyscalls)
ON_BN_CLICKED(IDC_LOG_THREADS, OnLogThreads)
ON_BN_CLICKED(IDC_LOG_TOP, OnLogTop)
ON_BN_CLICKED(IDC_LOG_NONE, OnLogNone)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLoggingDlg message handlers

BOOL
CLoggingDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    VerifyMaskString();

    m_Verbosity.SetCurSel(m_Level);

    return TRUE;
}

// returns -1 on error
int
CLoggingDlg::GetMaskValue()
{
    UpdateData(TRUE); // get values from controls
    if (m_Mask.IsEmpty())
        return 0;
    int mask;
    int res = _stscanf(m_Mask, _T("%X"), &mask);
    // sscanf will return 3 and success on "3ugh", so manually scan it
    if (!_istxdigit(m_Mask.GetAt(0)) ||
        (m_Mask.GetLength() > 1 && !_istxdigit(m_Mask.GetAt(1))) ||
        (m_Mask.GetLength() > 2 && !_istxdigit(m_Mask.GetAt(2))) ||
        (m_Mask.GetLength() > 3 && !_istxdigit(m_Mask.GetAt(3)))) {
        return -1;
    }
    if (res <= 0 || res == EOF)
        return -1;
    else
        return mask;
}

BOOL
CLoggingDlg::SetMaskValue(int mask)
{
    m_Mask.Format(_T("%04X"), mask);
    UpdateData(FALSE); // write values to controls
    return TRUE;
}

void
CLoggingDlg::VerifyMaskString()
{
    int mask = GetMaskValue();
    if (mask == -1)
        mask = 0;
    int i;
    for (i = 1; i < num_options - 1; i++) { // skip ALL and NONE
        CButton *button = (CButton *)GetDlgItem(checkboxes[i]);
        assert(button != NULL);
        if ((mask & masks[i]) != 0) {
            if (button->GetCheck() == 0)
                button->SetCheck(1);
        } else {
            if (button->GetCheck() != 0)
                button->SetCheck(0);
        }
    }
}

void
CLoggingDlg::OnChangeEditMask()
{
    UpdateData(TRUE); // get values from controls
    int mask = GetMaskValue();
    if (mask < 0 || mask > masks[LOG_ALL]) {
        m_OKButton.EnableWindow(FALSE);
    } else {
        m_OKButton.EnableWindow(TRUE);
        VerifyMaskString();
    }
}

void
CLoggingDlg::CheckboxChange(int id)
{
    CButton *button = (CButton *)GetDlgItem(checkboxes[id]);
    assert(button != NULL);
    int mask = GetMaskValue();
    if (mask == -1) {
        // don't allow checkbox change
        button->SetCheck(1 - button->GetCheck());
        return;
    }
    if (button->GetCheck() > 0) {
        mask |= masks[id];
    } else {
        mask &= ~(masks[id]);
    }
    SetMaskValue(mask);
}

void
CLoggingDlg::OnLogVmareas()
{
    CheckboxChange(LOG_VMAREAS);
}
void
CLoggingDlg::OnLogAsynch()
{
    CheckboxChange(LOG_ASYNCH);
}
void
CLoggingDlg::OnLogCache()
{
    CheckboxChange(LOG_CACHE);
}
void
CLoggingDlg::OnLogDispatch()
{
    CheckboxChange(LOG_DISPATCH);
}
void
CLoggingDlg::OnLogEmit()
{
    CheckboxChange(LOG_EMIT);
}
void
CLoggingDlg::OnLogFragment()
{
    CheckboxChange(LOG_FRAGMENT);
}
void
CLoggingDlg::OnLogHeap()
{
    CheckboxChange(LOG_HEAP);
}
void
CLoggingDlg::OnLogInterp()
{
    CheckboxChange(LOG_INTERP);
}
void
CLoggingDlg::OnLogLinks()
{
    CheckboxChange(LOG_LINKS);
}
void
CLoggingDlg::OnLogMonitor()
{
    CheckboxChange(LOG_MONITOR);
}
void
CLoggingDlg::OnLogStats()
{
    CheckboxChange(LOG_STATS);
}
void
CLoggingDlg::OnLogSyscalls()
{
    CheckboxChange(LOG_SYSCALLS);
}
void
CLoggingDlg::OnLogThreads()
{
    CheckboxChange(LOG_THREADS);
}
void
CLoggingDlg::OnLogTop()
{
    CheckboxChange(LOG_TOP);
}

void
CLoggingDlg::OnLogAll()
{
    SetMaskValue(masks[LOG_ALL]);
    VerifyMaskString();
    m_OKButton.EnableWindow(TRUE);
}

void
CLoggingDlg::OnLogNone()
{
    SetMaskValue(masks[LOG_NONE]);
    VerifyMaskString();
    m_OKButton.EnableWindow(TRUE);
}

int
CLoggingDlg::GetLevel()
{
    return final_level;
}

int
CLoggingDlg::GetMask()
{
    return final_mask;
}

void
CLoggingDlg::OnOK()
{
    UpdateData(TRUE); // get values from controls

    final_mask = GetMaskValue();
    if (final_mask == -1)
        final_mask = 0;

    final_level = m_Verbosity.GetCurSel();

    CDialog::OnOK();
}

#endif /* !DRSTATS_DEMO */ /* around whole file */
