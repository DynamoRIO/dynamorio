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

#include "stdafx.h"
#include "Wizard.h"
#include "CopyDlg.h"
#include <assert.h>
#include <direct.h> // for _chdir
#include "WizSheet.h"
#include "ShellInterface.h"

#ifdef _DEBUG
#    define new DEBUG_NEW
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// for CGO 2008 tutorial we create a zip file from data in our data section
static const unsigned char zipdata[] = {
#include "zipdump.h"
};
#define ZIPNAME _T("DynamoRIO.zip")

// we copy the tree rooted at DynamoRIO into here, so only specify parent dir:
#define DEFAULT_DIR _T("VMware\\DynamoRIO")

// MB of space we take up
#define DISK_SPACE_REQUIRED 14

/////////////////////////////////////////////////////////////////////////////
// CCopyDlg property page

IMPLEMENT_DYNCREATE(CCopyDlg, CPropertyPage)

CCopyDlg::CCopyDlg()
    : CPropertyPage(CCopyDlg::IDD, 0)
{
    //{{AFX_DATA_INIT(CCopyDlg)
    m_SpaceAvailable = _T("");
    m_SpaceRequired = _T("");
    m_Target = _T("");
    //}}AFX_DATA_INIT

    m_DefaultDir[0] = _T('\0');
    m_psp.dwFlags |= PSP_HIDEHEADER;
    m_psp.dwFlags &= ~(PSP_HASHELP);
}

CCopyDlg::~CCopyDlg()
{
}

void
CCopyDlg::DoDataExchange(CDataExchange *pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CCopyDlg)
    DDX_Control(pDX, IDC_TARGET, m_TargetEdit);
    DDX_Text(pDX, IDC_SPACE_AVAILABLE, m_SpaceAvailable);
    DDX_Text(pDX, IDC_SPACE_REQUIRED, m_SpaceRequired);
    DDX_Text(pDX, IDC_TARGET, m_Target);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CCopyDlg, CPropertyPage)
//{{AFX_MSG_MAP(CCopyDlg)
ON_BN_CLICKED(IDC_BROWSE, OnBrowse)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCopyDlg message handlers

BOOL
CCopyDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    m_SpaceRequired.Format(_T("%8d"), DISK_SPACE_REQUIRED);
    ULARGE_INTEGER userbytes, totalbytes, freebytes;
    // WARNING: GetDiskFreeSpaceEx is not available on NT < 4.0
    if (!GetDiskFreeSpaceEx(_T("C:\\"), &userbytes, &totalbytes, &freebytes))
        assert(false);
    __int64 bytes = (freebytes.QuadPart) / (1024 * 1024);
    m_SpaceAvailable.Format(_T("%8I64u"), bytes);

    int res = GetCurrentDirectory(MAX_PATH, m_CWD);
    assert(res > 0);

    TCHAR drive[8];
    int len = GetEnvironmentVariable(_T("SYSTEMDRIVE"), drive, 7);
    if (len == 0 || len > 7)
        _stprintf(drive, _T("C:"));

    TCHAR buf[MAX_PATH];
    len = GetEnvironmentVariable(_T("PROGRAMFILES"), buf, MAX_PATH);
    if (len == 0 || len > MAX_PATH) {
        // NT doesn't have this env var...so just hardcode it
        _stprintf(buf, _T("%s\\Program Files"), drive);
        len = _tcslen(buf);
    }
    if (!SetCurrentDirectory(buf)) {
        // directory does not exist or is not accessible!
        _stprintf(buf, _T("%s\\"), drive);
        if (!SetCurrentDirectory(buf)) {
            assert(FALSE);
        }
    }

    assert(len + _tcslen(DEFAULT_DIR) + 1 < MAX_PATH);
    _stprintf(m_DefaultDir, _T("%s\\%s"), buf, DEFAULT_DIR);
    m_Target.Format(_T("%s"), m_DefaultDir);

    // must go back to wd
    if (!SetCurrentDirectory(m_CWD)) {
        assert(FALSE);
    }

    UpdateData(FALSE);

    return TRUE;
}

BOOL
CCopyDlg::OnSetActive()
{
    CPropertySheet *pSheet = (CPropertySheet *)GetParent();
    ASSERT_KINDOF(CPropertySheet, pSheet);
    // our installer doesn't support Back!
    //      pSheet->SetWizardButtons( PSWIZB_BACK | PSWIZB_NEXT);
    pSheet->SetWizardButtons(PSWIZB_NEXT);
    return CPropertyPage::OnSetActive();
}

LRESULT
CCopyDlg::OnWizardNext()
{
    if (!CopyFiles()) {
        // select entire directory
        m_TargetEdit.SetSel(0, -1);
        m_TargetEdit.SetFocus();
        // return to this same wizard page
        // (we assume problem is with target dir)
        return -1;
    }

    // need to communicate install dir to later pages
    // use parent Sheet
    CWizardSheet *pSheet = (CWizardSheet *)GetParent();
    // add the DynamoRIO on
    pSheet->m_InstallDir.Format(_T("%s\\DynamoRIO"), m_Target);

    return CPropertyPage::OnWizardNext();
}

BOOL
CCopyDlg::CopyFiles()
{
    TCHAR msg[MAX_PATH * 2];
    TCHAR cmd[MAX_PATH * 2];

    UpdateData(TRUE); // get target dir string

    // make the target directory
    // have to build each new directory one at a time
    CString newdir;
    int pos = 0;
    int slash = m_Target.Find('\\', pos);

    // first see if need to clean out existing dir
    if (SetCurrentDirectory(m_Target)) {
        _stprintf(msg,
                  _T("Directory %s already exists.\n")
                  _T("Continuing will delete all its existing files.\nContinue?"),
                  m_Target);
        int res = MessageBox(msg, _T("Confirmation"), MB_OKCANCEL | MYMBFLAGS);
        if (res == IDCANCEL)
            return FALSE;
        // get out of the dir
        BOOL ok = SetCurrentDirectory(m_CWD);
        assert(ok);
        _stprintf(cmd, _T("%s"), m_Target);
        if (!CShellInterface::DeleteFile(cmd, GetParent()->m_hWnd)) {
            _stprintf(msg, _T("Error removing existing directory %s"), m_Target);
            MessageBox(msg, _T("Error Deleting Files"), MB_OK | MYMBFLAGS);
            return FALSE;
        }
    }

    while (TRUE) {
        if (slash == -1)
            newdir = m_Target;
        else
            newdir = m_Target.Mid(0, slash);
        if (!SetCurrentDirectory(newdir)) {
            _stprintf(msg, _T("Create directory %s?"), newdir);
            int res = MessageBox(msg, _T("Confirmation"), MB_OKCANCEL | MYMBFLAGS);
            if (res == IDCANCEL)
                return FALSE;
            if (!CreateDirectory(newdir, NULL)) {
                _stprintf(msg, _T("Could not create directory %s"), newdir);
                MessageBox(msg, _T("Error Copying Files"), MB_OK | MYMBFLAGS);
                return FALSE;
            }
        }
        if (slash == -1)
            break;
        pos = slash + 1;
        slash = m_Target.Find('\\', pos);
    }

    // now copy the files
#if 1 /* CGO 2008 tutorial: create zip file */
    DWORD written;
    BOOL success;
    TCHAR to[MAX_PATH];
    _stprintf(to, _T("%s\\%s"), m_Target, ZIPNAME);
    HANDLE h = CreateFile(to,                    // file to open
                          GENERIC_WRITE,         // no read access needed
                          0,                     // no sharing
                          NULL,                  // default security
                          CREATE_ALWAYS,         // clobber existing file
                          FILE_ATTRIBUTE_NORMAL, // normal file
                          NULL);                 // no attr. template
    if (h == NULL)
        success = FALSE;
    else {
        success = WriteFile(h, zipdata, sizeof(zipdata), &written, NULL);
        CloseHandle(h);
    }
    if (!success || written != sizeof(zipdata)) {
        _stprintf(msg, _T("Error copying file to %s"), to);
        MessageBox(msg, _T("Error Copying Files"), MB_OK | MYMBFLAGS);
        return FALSE;
    }
#else
    TCHAR from[MAX_PATH];
    TCHAR to[MAX_PATH];
#    if 0
    _stprintf(from, _T("%s"), _T("c:\\iye\\rio\\install\\DynamoRIO"));
    //_stprintf(from, _T("%s"), _T("d:\\bruening\\dynamo\\install\\DynamoRIO"));
#    else
    _stprintf(from, _T("%s\\DynamoRIO"), m_CWD);
#    endif
    _stprintf(to, _T("%s"), m_Target);
    if (!CShellInterface::CopyDir(from, to, GetParent()->m_hWnd)) {
        _stprintf(msg, _T("Error copying files from %s to %s"), from, to);
        MessageBox(msg, _T("Error Copying Files"), MB_OK | MYMBFLAGS);
        return FALSE;
    }
#endif

    return TRUE;
}

void
CCopyDlg::OnBrowse()
{
    TCHAR folder[MAX_PATH];
    BROWSEINFO bi;
    bi.hwndOwner = m_hWnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = folder;
    bi.lpszTitle = _T("Select folder to install into");
    bi.ulFlags = 0;
    bi.lpfn = NULL;
    bi.lParam = NULL;
    bi.iImage = 0;
    ITEMIDLIST *id = SHBrowseForFolder(&bi);
    if (id == NULL) // cancelled
        return;
    SHGetPathFromIDList(id, folder);
    m_Target.Format(_T("%s"), folder);
    UpdateData(FALSE);
}
