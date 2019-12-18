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

// CmdlineDlg.cpp : implementation file
//

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "CmdlineDlg.h"

#    ifdef _DEBUG
#        define new DEBUG_NEW
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    endif

/////////////////////////////////////////////////////////////////////////////
// CCmdlineDlg dialog

CCmdlineDlg::CCmdlineDlg(CWnd *pParent /*=NULL*/)
    : CDialog(CCmdlineDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CCmdlineDlg)
    m_CmdLine = _T("");
    m_WorkingDir = _T("");
    //}}AFX_DATA_INIT
}

CCmdlineDlg::CCmdlineDlg(CString wdir, CWnd *pParent /*=NULL*/)
    : CDialog(CCmdlineDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CCmdlineDlg)
    m_CmdLine = _T("");
    //}}AFX_DATA_INIT
    m_WorkingDir = wdir;
}

void
CCmdlineDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CCmdlineDlg)
    DDX_Text(pDX, IDC_CMDLINE, m_CmdLine);
    DDX_Text(pDX, IDC_WORKING_DIR, m_WorkingDir);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CCmdlineDlg, CDialog)
//{{AFX_MSG_MAP(CCmdlineDlg)
ON_BN_CLICKED(IDC_WORKING_DIR_BROWSE, OnWorkingDirBrowse)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCmdlineDlg message handlers

BOOL
CCmdlineDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    UpdateData(FALSE); // write to controls
    return TRUE;
}

static TCHAR szFilter[] = _T("Directories (*)|*|All Files (*.*)|*.*||");

void
CCmdlineDlg::OnWorkingDirBrowse()
{
    TCHAR folder[MAX_PATH];
    BROWSEINFO bi;
    bi.hwndOwner = m_hWnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = folder;
    bi.lpszTitle = _T("Select Working Directory");
    bi.ulFlags = 0;
    bi.lpfn = NULL;
    bi.lParam = NULL;
    bi.iImage = 0;
#    if _MSC_VER <= 1400 /* VS2005- */
    LPITEMIDLIST id = SHBrowseForFolder(&bi);
#    else
    PIDLIST_ABSOLUTE id = SHBrowseForFolder(&bi);
#    endif
    if (id == NULL) // cancelled
        return;
    SHGetPathFromIDList(id, folder);
    m_WorkingDir.Format(_T("%s"), folder);
    UpdateData(FALSE);
}

CString
CCmdlineDlg::GetWorkingDir()
{
    return m_WorkingDir;
}

CString
CCmdlineDlg::GetArguments()
{
    return m_CmdLine;
}

void
CCmdlineDlg::OnOK()
{
    CDialog::OnOK();
}

#endif /* !DRSTATS_DEMO */ /* around whole file */
