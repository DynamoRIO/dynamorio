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

// DynamoRIODoc.cpp : implementation of the CDynamoRIODoc class
//

#include "stdafx.h"
#include <assert.h>
#include <commdlg.h>
#include <imagehlp.h>
#include <direct.h>
#include "DynamoRIO.h"
#include "DynamoRIODoc.h"
#include "ShellInterface.h"
#include "CmdlineDlg.h"

#ifdef _DEBUG
#    define new DEBUG_NEW
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define INJECTOR_SUBPATH _T("\\bin\\drinject.exe")

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIODoc

IMPLEMENT_DYNCREATE(CDynamoRIODoc, CDocument)

BEGIN_MESSAGE_MAP(CDynamoRIODoc, CDocument)
//{{AFX_MSG_MAP(CDynamoRIODoc)
// NOTE - the ClassWizard will add and remove mapping macros here.
//    DO NOT EDIT what you see in these blocks of generated code!
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIODoc construction/destruction

CDynamoRIODoc::CDynamoRIODoc()
{
#ifndef DRSTATS_DEMO
    InitPaths();

    CShellInterface::Initialize();
#endif

    // make sure framework prompts to "save unsaved work"
    SetModifiedFlag(TRUE);
}

#ifndef DRSTATS_DEMO
void
CDynamoRIODoc::InitPaths()
{
    // limit to 200 to give room for rest of injector path
    int len = GetEnvironmentVariable(_T("DYNAMORIO_HOME"), m_dynamorio_home, _MAX_DIR);
    assert(len <= _MAX_DIR && len + _tcslen(INJECTOR_SUBPATH) < MAX_PATH);
    if (len == 0 || len > _MAX_DIR)
        m_dynamorio_home[0] = _T('\0');
    else
        NULL_TERMINATE_BUFFER(m_dynamorio_home);
    _stprintf(m_injector_path, _T("%s%s"), m_dynamorio_home, INJECTOR_SUBPATH);

#    if 0
    // there are issues with privileges to write to Program Files, so we no longer do this:
    _stprintf(m_logs_dir, _T("%s\\logs"), m_dynamorio_home);
#    else
    // instead we do $USERPROFILE\\Application Data\\DynamoRIO
    TCHAR userprof[MAX_PATH];
    CString profile_dir;
    len = GetEnvironmentVariable(_T("USERPROFILE"), userprof, MAX_PATH);
    assert(len <= MAX_PATH);
    if (len == 0 || len > MAX_PATH) {
        // on NT, use $SYSTEMROOT\\Profiles
        len = GetEnvironmentVariable(_T("SYSTEMROOT"), userprof, MAX_PATH);
        assert(len > 0 && len < MAX_PATH);
        profile_dir.Format(_T("%s\\Profiles"), userprof);
    } else {
        profile_dir.Format(_T("%s"), userprof);
    }
    _stprintf(m_logs_dir, _T("%s\\Application Data\\DynamoRIO"), profile_dir);
    if (!SetCurrentDirectory(m_logs_dir)) {
        if (!CreateDirectory(m_logs_dir, NULL)) {
            _stprintf(userprof, _T("Cannot create default working directory %s"),
                      m_logs_dir);
            MessageBox(NULL, userprof, _T("Error"), MB_OK | MYMBFLAGS);
            _stprintf(m_logs_dir, _T("c:\\")); // any better ideas?
        }
    }

#    endif
}
#endif /* !DRSTATS_DEMO */

CDynamoRIODoc::~CDynamoRIODoc()
{
#ifndef DRSTATS_DEMO
    CShellInterface::Uninitialize();
#endif
}

BOOL
CDynamoRIODoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    // TODO: add reinitialization code here
    // (SDI documents will reuse this document)

    // blank title initially
    SetTitle(_T(""));

    return TRUE;
}

BOOL
CDynamoRIODoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    if (!CDocument::OnOpenDocument(lpszPathName))
        return FALSE;

    // initialization that doesn't happen in Serialize
    // for us, that's all of it

    return TRUE;
}

#ifndef DRSTATS_DEMO
BOOL
CDynamoRIODoc::RunApplication(LPCTSTR lpszPathName)
{
    // application name (may be different than lpszPathName, for shortcuts)
    TCHAR app_name[MAX_PATH];
    // working directory
    TCHAR rundir[_MAX_DIR];
    // entire command line = app_name + arguments
    TCHAR app_cmdline[MAX_PATH * 2];
    // buffer for error messages
    TCHAR msg[MAX_PATH * 3];

    // default working directory
    TCHAR default_rundir[_MAX_DIR];
    _stprintf(default_rundir, _T("%s"), m_logs_dir);

    CString app_args; // will be used to build app_cmdline

    // if app is a shortcut, resolve it now
    int len = _tcslen(lpszPathName);
    TCHAR *last4 = (TCHAR *)lpszPathName + len - 4;
    if (_tcscmp(last4, _T(".lnk")) == 0) {
        TCHAR params[MAX_PATH];
        if (CShellInterface::ResolveLinkFile((TCHAR *)lpszPathName, app_name, params,
                                             rundir,
                                             CDynamoRIOApp::GetActiveView()->m_hWnd)) {
            _stprintf(msg, _T("Resolved link file to %s %s\nin directory %s\n"), app_name,
                      params, rundir);
            MessageBox(NULL, msg, _T("Link File"), MB_OK | MYMBFLAGS);
            app_args.Format(_T("%s"), params);
        } else {
            _stprintf(msg, _T("Failed to resolve link file"));
            MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
            return FALSE;
        }
    } else {
        _stprintf(app_name, _T("%s"), lpszPathName);

        // ask for arguments and working dir
        // pass in default working dir
        CString defwdir(default_rundir);
        CCmdlineDlg dlg(defwdir);
        int res = dlg.DoModal();
        if (res == IDCANCEL)
            return FALSE;
        CString wdir = dlg.GetWorkingDir();
        _tcscpy(rundir, wdir.GetBuffer(0));
        app_args = dlg.GetArguments();
    }

    // construct app_cmdline
    // if passing app name to injector as argument, must quote it
    // if running natively, do not quote it
    if (CDynamoRIOApp::SystemwideSet()) {
        _stprintf(app_cmdline, _T("%s %s"), app_name, app_args);
    } else {
        _stprintf(app_cmdline, _T("\"%s\" %s"), app_name, app_args);
    }

    // if rundir has environment vars, resolve them
    CString dir(rundir);
    CString realdir(_T(""));
    int start = dir.Find(_T('%'));
    int old_end = 0;
    while (start != -1) {
        realdir += dir.Mid(old_end, start - old_end);
        int end = dir.Find(_T('%'), start + 1);
        if (end == -1) {
            realdir += dir.Right(dir.GetLength() - start);
            break;
        }
#    if 0 // debugging stuff
        _stprintf(msg, _T("from %d to %d"), start, end);
        MessageBox(NULL, msg, _T("env var value"), MB_OK);
#    endif
        CString name = dir.Mid(start + 1, end - start - 1);
#    if 0 // debugging stuff
        MessageBox(NULL, name.GetBuffer(0), _T("env var name"), MB_OK);
#    endif
        int len = GetEnvironmentVariable(name.GetBuffer(0), msg, MAX_PATH);
        assert(len <= MAX_PATH);
        if (len == 0 || len > MAX_PATH)
            msg[0] = _T('\0');
#    if 0 // debugging stuff
        MessageBox(NULL, msg, _T("env var value"), MB_OK);
#    endif
        realdir += msg;
        old_end = end + 1;

        start = dir.Find(_T('%'), end + 1);
    }
    if (!realdir.IsEmpty())
        _stprintf(rundir, _T("%s"), realdir.GetBuffer(0));

    // if rundir is empty, use default
    // FIXME: have option to set default?
    if (rundir[0] == _T('\0'))
        _tcscpy(rundir, default_rundir);

    // be robust -- make sure app file exists
    CFile check;
    if (!check.Open(app_name, CFile::modeRead | CFile::shareDenyNone)) {
        _stprintf(msg, _T("Application %s does not exist"), app_name);
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
        return FALSE;
    }
    check.Close();

    // now go to run directory
    if (!SetCurrentDirectory(rundir)) {
        _stprintf(msg, _T("Error changing to working directory %s"), rundir);
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
        return FALSE;
    }

    // now launch application
    TCHAR *launch_name;
    TCHAR launch_cmdline[MAX_PATH * 2];

    if (CDynamoRIOApp::SystemwideSet()) {
        // just launch app natively, systemwide injector will catch it
        launch_name = app_name;
        // note that we want target app name as part of cmd line
        _tcscpy(launch_cmdline, app_cmdline);
    } else {
        // explicitly launch app under injector
        launch_name = m_injector_path;
        // note that we want target app name as part of cmd line
        TCHAR *dll_path = CDynamoRIOApp::GetDllPath();
        _stprintf(launch_cmdline, _T("\"%s\" \"%s\" %s"), m_injector_path, dll_path,
                  app_cmdline);

        // be robust
        if (!check.Open(m_injector_path, CFile::modeRead | CFile::shareDenyNone)) {
            _stprintf(msg, _T("DynamoRIO injector %s does not exist"), m_injector_path);
            MessageBox(NULL, msg, _T("DynamoRIO Configuration Error"), MB_OK | MYMBFLAGS);
            return FALSE;
        }
        check.Close();
        if (!check.Open(dll_path, CFile::modeRead | CFile::shareDenyNone)) {
            _stprintf(msg, _T("DynamoRIO library %s does not exist"), dll_path);
            MessageBox(NULL, msg, _T("DynamoRIO Configuration Error"), MB_OK | MYMBFLAGS);
            return FALSE;
        }
        check.Close();
    }

    assert(_tcslen(rundir) + _tcslen(launch_cmdline) + _tcslen(_T("\nin directory\n")) <
           MAX_PATH * 2);

#    if 0  // I'm disabling this dialog, usually just annoying
    _stprintf(msg, _T("%s\nin directory\n%s"), launch_cmdline, rundir);
    MessageBox(NULL, msg, _T("About to run"), MB_OK | MB_TOPMOST);
#    endif // 0

#    if 0 // I'm taking this out, it requires an extra library and doesn't add much
    // FIXME: do this up above, instead of checking app_name exists?
    PLOADED_IMAGE       li;
#        ifdef UNICODE
    // ImageLoad does not take unicode strings
    char ansi_launch_name[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, launch_name, _tcslen(launch_name),
                        ansi_launch_name, MAX_PATH, NULL, NULL);
    if (li = ImageLoad(ansi_launch_name, NULL))
#        else
    if (li = ImageLoad(launch_name, NULL))
#        endif
    {
        ImageUnload(li);
    } else {
        _stprintf(msg, _T("Failed to load %s"), launch_name);
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
        return FALSE;
    }
#    endif // 0

    // Launch the application process
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    STARTUPINFO mysi;
    GetStartupInfo(&mysi);
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = mysi.hStdInput;
    si.hStdOutput = mysi.hStdOutput;
    si.hStdError = mysi.hStdError;

    /* Must specify TRUE for bInheritHandles so child inherits stdin! */
    if (!CreateProcess(launch_name, launch_cmdline, NULL, NULL, TRUE,
                       DETACHED_PROCESS, // to avoid console creation!
                       NULL, NULL, &si, &pi)) {
        _stprintf(msg, _T("Failed to load %s"), app_name);
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
        return FALSE;
    }

#    if 0
        // FIXME: We have the injector's pid, not the child's pid!
    if (!CDynamoRIOApp::GetActiveView()->SelectProcess(pi.dwProcessId))
        MessageBox(NULL, _T("Failed to select"), _T("Error"),  MB_OK | MYMBFLAGS);
#    endif
    // it takes some time for the new process to start up and its shared memory
    // to become visible, so wait for it, but don't wait forever -- there could be
    // a usage error or the new process may crash
    int i = 0;
    // wait 10ms*200 = 2 seconds
    while (i < 200) {
        // change the <no instances found> entry
        if (CDynamoRIOApp::GetActiveView()->UpdateProcessList())
            break;
        Sleep(10);
        i++;
    }

    // don't call SetTitle -- View sets it to what's being viewed
    // SetTitle(lpszPathName);

    return TRUE;
}
#endif /* !DRSTATS_DEMO */

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIODoc serialization

void
CDynamoRIODoc::Serialize(CArchive &ar)
{
    if (ar.IsStoring()) {
        // TODO: add storing code here
    } else {
        // TODO: add loading code here
    }
}

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIODoc diagnostics

#ifdef _DEBUG
void
CDynamoRIODoc::AssertValid() const
{
    CDocument::AssertValid();
}

void
CDynamoRIODoc::Dump(CDumpContext &dc) const
{
    CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIODoc commands

BOOL
CDynamoRIODoc::SaveModified()
{
#ifndef DRSTATS_DEMO
    CDynamoRIOApp::AboutToExit();
#endif
    return CDocument::SaveModified();
}
