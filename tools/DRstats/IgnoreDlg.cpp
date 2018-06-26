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

// IgnoreDlg.cpp : implementation file
//

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "IgnoreDlg.h"
#    include <assert.h>

#    ifdef _DEBUG
#        define new DEBUG_NEW
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    endif

/////////////////////////////////////////////////////////////////////////////
// CIgnoreDlg dialog

CIgnoreDlg::CIgnoreDlg(CWnd *pParent /*=NULL*/)
    : CDialog(CIgnoreDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CIgnoreDlg)
    m_IgnoreList = _T("");
    //}}AFX_DATA_INIT
}

void
CIgnoreDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CIgnoreDlg)
    DDX_Text(pDX, IDC_IGNORELIST, m_IgnoreList);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CIgnoreDlg, CDialog)
//{{AFX_MSG_MAP(CIgnoreDlg)
ON_BN_CLICKED(IDC_SET_PERMANENT, OnSetPermanent)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CIgnoreDlg message handlers

BOOL
CIgnoreDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    TCHAR path[MAX_PATH];
    DWORD len = GetEnvironmentVariable(_T("DYNAMORIO_IGNORE"), path, MAX_PATH);
    assert(len < MAX_PATH);
    if (len > 0 && len < MAX_PATH)
        m_IgnoreList = path; // makes new storage, right?
    UpdateData(FALSE);       // FALSE means set controls

    return TRUE;
}

void
CIgnoreDlg::OnOK()
{
    UpdateData(TRUE); // read from controls
    BOOL res = SetEnvironmentVariable(_T("DYNAMORIO_IGNORE"), m_IgnoreList);
    assert(res);
    CDialog::OnOK();
}

void
CIgnoreDlg::OnSetPermanent()
{
    // it takes a while to broadcast the "we've changed env vars" message,
    // so set a wait cursor
    HCURSOR prev_cursor = SetCursor(LoadCursor(0, IDC_WAIT));

    TCHAR msg[MAX_PATH];
    HKEY hk;
    DWORD res;

    // current user only
    res = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hk);
    if (res != ERROR_SUCCESS) {
        _stprintf(msg, _T("Error writing to HKEY_CURRENT_USER\\Environment"));
        MessageBox(msg, _T("Error"), MB_OK | MYMBFLAGS);
    }

    UpdateData(TRUE); // get values from controls
    TCHAR *val = m_IgnoreList.GetBuffer(0);
    res = RegSetValueEx(hk, _T("DYNAMORIO_IGNORE"), 0, REG_SZ, (LPBYTE)val,
                        (DWORD)_tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);

    RegCloseKey(hk);

    // tell system that we've changed environment (else won't take effect until
    // user logs out and back in)
    DWORD_PTR dwReturnValue;
    // Code I copied this from used an ANSI string...I'm leaving
    // it like that FIXME
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "Environment",
                       SMTO_ABORTIFHUNG, 5000, &dwReturnValue);

    SetCursor(prev_cursor);

    // set local env var too, and avoid questions of ability to
    // cancel the permanent operation
    OnOK();
}

#endif /* !DRSTATS_DEMO */ /* around whole file */
