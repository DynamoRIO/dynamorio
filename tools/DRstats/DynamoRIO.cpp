/* **********************************************************
 * Copyright (c) 2012-2014 Google, Inc.  All rights reserved.
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

// DynamoRIO.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "DynamoRIO.h"

#include "MainFrm.h"
#include "DynamoRIODoc.h"
#include "DynamoRIOView.h"
#include "OptionsDlg.h"
#include "IgnoreDlg.h"
#include "SyswideDlg.h"

#include <assert.h>
#include <process.h> // for system()

#ifdef _DEBUG
#    define new DEBUG_NEW
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define HELP_PATH _T("\\docs\\html\\index.html")

// FIXME: these are duplicated with the installation wizard, so
// that the GUI can set env vars for new users (installer can only
// set for user installing)
#define INITIAL_OPTIONS _T("-stats -loglevel 1")
#define INITIAL_SYSTEMWIDE _T("\\lib\\debug\\dynamorio.dll")
#define INITIAL_IGNORE _T("drinject.exe;DynamoRIO.exe")
//#define INITIAL_IGNORE
//_T("drinject.exe;DynamoRIO.exe;CSRSS.EXE;WINLOGON.EXE;SERVICES.EXE;LSASS.EXE;svchost.exe;SPOOLSV.EXE;taskmgr.exe;jconfigdnt.exe;explorer.exe")

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOApp

BEGIN_MESSAGE_MAP(CDynamoRIOApp, CWinApp)
//{{AFX_MSG_MAP(CDynamoRIOApp)
ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
ON_COMMAND(ID_APP_EXIT, OnAppExit)
#ifndef DRSTATS_DEMO
ON_COMMAND(ID_EDIT_OPTIONS, OnEditOptions)
ON_COMMAND(ID_LIBRARY_RELEASE, OnLibraryRelease)
ON_COMMAND(ID_LIBRARY_PROFILE, OnLibraryProfile)
ON_COMMAND(ID_LIBRARY_DEBUG, OnLibraryDebug)
ON_COMMAND(ID_FILE_RUN, OnFileRun)
ON_COMMAND(ID_FILE_SYSTEMWIDE, OnFileSystemwide)
ON_COMMAND(ID_HELP_HELP, OnHelpHelp)
ON_COMMAND(ID_EDIT_IGNORELIST, OnEditIgnorelist)
#endif
//}}AFX_MSG_MAP
#ifndef DRSTATS_DEMO
// Standard file based document commands
ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
// Standard print setup command
ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
#endif
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOApp construction

CDynamoRIOApp::CDynamoRIOApp()
{
    // Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CDynamoRIOApp object

CDynamoRIOApp theApp;

// FIXME: find better way to have other classes access this one

/* static */ CDynamoRIOView *
CDynamoRIOApp::GetActiveView()
{
    return (CDynamoRIOView *)theApp.m_pMainFrame->GetActiveView();
}

#ifndef DRSTATS_DEMO
/* static */ BOOL
CDynamoRIOApp::SystemwideSet()
{
    return theApp.m_bInjectAll;
}

/*static */ TCHAR *
CDynamoRIOApp::GetDllPath()
{
    return theApp.m_dll_path;
}

/*static */ DLL_TYPE
CDynamoRIOApp::GetDllType()
{
    return theApp.m_dll_type;
}

/*static */ void
CDynamoRIOApp::SetSystemwideSetting(int val)
{
    theApp.WriteProfileInt(_T("Settings"), _T("Confirm Systemwide"), val);
}

/*static */ void
CDynamoRIOApp::AboutToExit()
{
    theApp.PreExit();
}
#endif /* !DRSTATS_DEMO */

/*static */ void
CDynamoRIOApp::SetStatusbarText(TCHAR *txt)
{
    theApp.m_pMainFrame->SetStatusBarText(0, txt);
}

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOApp initialization

BOOL
CDynamoRIOApp::InitInstance()
{
#ifndef DRSTATS_DEMO
    TCHAR msg[MAX_PATH * 2];
#endif
    BOOL windows_NT;

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    //  of your final executable, you should remove from the following
    //  the specific initialization routines you do not need.

    if (!CheckWindowsVersion(windows_NT)) {
        // abort
        return FALSE;
    }

#if 0 // warning C4996: 'CWinApp::Enable3dControlsStatic': CWinApp::Enable3dControlsStatic
      // is no longer needed. You should remove this call.
#    ifdef _AFXDLL
    Enable3dControls();                     // Call this when using MFC in a shared DLL
#    else
    Enable3dControlsStatic();       // Call this when linking to MFC statically
#    endif
#endif

#ifndef DRSTATS_DEMO
    // Change the registry key under which our settings are stored,
    // including MRU
    SetRegistryKey(L_DYNAMORIO_REGISTRY_KEY);

    LoadStdProfileSettings(12); // Load standard INI file options (including MRU)
#endif

    // Register the application's document templates.  Document templates
    //  serve as the connection between documents, frame windows and views.

    CSingleDocTemplate *pDocTemplate;
    pDocTemplate = new CSingleDocTemplate(
#ifdef DRSTATS_DEMO
        IDR_MAINFRAME_DEMO,
#else
        IDR_MAINFRAME,
#endif
        RUNTIME_CLASS(CDynamoRIODoc),
        RUNTIME_CLASS(CMainFrame), // main SDI frame window
        RUNTIME_CLASS(CDynamoRIOView));
    AddDocTemplate(pDocTemplate);

    // Parse command line for standard shell commands, DDE, file open
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    // Dispatch commands specified on the command line
    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    // enable file manager drag/drop and DDE Execute open
    m_pMainWnd->DragAcceptFiles();

    // The one and only window has been initialized, so show and update it.
    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();

    // I can't find any other way to access the main frame
    m_pMainFrame = (CMainFrame *)m_pMainWnd;

    // I can't figure out how to disable a menu item that has a command
    // handler when this var is set, so I disable it:
    m_pMainFrame->m_bAutoMenuEnable = FALSE;

#ifndef DRSTATS_DEMO
    m_bSystemwideAllowed = TRUE;

    // set the string we'll put into the registry key to inject system-wide
    TCHAR data[1024];
    int len = GetEnvironmentVariable(_T("DYNAMORIO_HOME"), m_dynamorio_home, _MAX_DIR);
    if (len == 0) {
        int res = MessageBox(
            NULL,
            _T("DYNAMORIO_HOME environment variable not found.\n")
            _T("Set all the DynamoRIO environment variables to their default values?\n")
            _T("(Otherwise this GUI cannot operate and must exit.)"),
            _T("DynamoRIO Not Configured for Current User"), MB_YESNO | MYMBFLAGS);
        if (res == IDYES) {
            if (!ConfigureForNewUser())
                return FALSE;
        } else {
            // abort
            return FALSE;
        }
    }

    if (windows_NT) {
        // we don't support systemwide on NT
        // hack: use "confirm systemwide" setting to decide whether to notify user
        if (GetProfileInt(_T("Settings"), _T("Confirm Systemwide"), 1) == 1) {
            MessageBox(NULL,
                       _T("Run All is not supported on Windows NT, it will be disabled"),
                       _T("Notice"), MB_OK | MYMBFLAGS);
            // but then how does user turn off, if can't do Run All?
            // just turn it off now:
            SetSystemwideSetting(0);
        }
        m_bSystemwideAllowed = FALSE; // so key won't be cleared
        DisableSystemwideInject();
    } else {
        assert(len > 0 && len < _MAX_DIR &&
               len + _tcslen(L_INJECT_ALL_DLL_SUBPATH) < MAX_PATH);
        _stprintf(data, _T("%s%s"), m_dynamorio_home, L_INJECT_ALL_DLL_SUBPATH);

        // make sure it exists
        CFile check;
        if (!check.Open(data, CFile::modeRead | CFile::shareDenyNone)) {
#    if 0 // I'm disabling this dialog until we decide to support running apps
            _stprintf(msg, _T("Library %s does not exist"), data);
            MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
#    endif
            m_bSystemwideAllowed = FALSE; // so key won't be cleared
            DisableSystemwideInject();
        } else {
            if (_tcschr(data, _T(' ')) != NULL) {
                // registry key cannot handle spaces in names!
                // must get 8.3 alias -- and some volumes do not support such an alias!
                len = GetShortPathName(data, m_inject_all_value, MAX_PATH);
                if (len == 0) {
                    _stprintf(msg,
                              _T("Cannot find 8.3 alias for space-containing path ")
                              _T("\"%s\"!\nDisabling Run All"),
                              data);
                    MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
                    m_bSystemwideAllowed = FALSE; // so key won't be cleared
                    DisableSystemwideInject();
                }
            } else {
                _tcscpy(m_inject_all_value, data);
            }
            check.Close();
        }
    }

    // find current status of system-wide injection
    if (m_bSystemwideAllowed) {
        HKEY hk;
        unsigned long size = 1024;
        int res = RegOpenKeyEx(INJECT_ALL_HIVE, INJECT_ALL_KEY_L, 0, KEY_READ, &hk);
        assert(res == ERROR_SUCCESS);
        res = RegQueryValueEx(hk, INJECT_ALL_SUBKEY_L, 0, NULL, (LPBYTE)data, &size);
        assert(res == ERROR_SUCCESS);
        RegCloseKey(hk);

        // WARNING: do not use Unicode build!
        // if ever get size==2, it's b/c of Unicode build!
        // Plus, stats viewing doesn't work w/ Unicode build
        if (size > sizeof(TCHAR)) {
            // make sure we're the ones who set this value
            if (_tcscmp(m_inject_all_value, data) != 0) {
                // FIXME: have user notify us of conflict?
                int res = MessageBox(
                    NULL,
                    _T("DynamoRIO's RunAll system-wide injection method is ")
                    _T("being used by some other program.\n")
                    _T("DynamoRIO can attempt to override the other program.\n")
                    _T("Otherwise, system-wide injection will be disabled.\n")
                    _T("Override?"),
                    _T("DynamoRIO Conflict"), MB_YESNO | MYMBFLAGS);
                if (res == IDYES) {
                    // now clear the registry key
                    SetSystemwideInject(_T(""));
                    // if the call fails, callee calls Disable for us
                } else {
                    m_bSystemwideAllowed = FALSE; // so key won't be cleared
                    DisableSystemwideInject();
                }
            } else {
                m_bInjectAll = TRUE;
            }
        } else {
            // empty value: no injection
            m_bInjectAll = FALSE;
        }
    }
    if (m_bSystemwideAllowed) {
        m_pMainWnd->GetMenu()->CheckMenuItem(
            ID_FILE_SYSTEMWIDE,
            MF_BYCOMMAND | ((m_bInjectAll) ? MF_CHECKED : MF_UNCHECKED));
    }

    // make sure preinject dll exists
    if (m_bSystemwideAllowed) {
        CFile check;
        if (!check.Open(m_inject_all_value, CFile::modeRead | CFile::shareDenyNone)) {
#    if 0 // I'm disabling this dialog until we decide to support running apps
            _stprintf(msg, _T("Library %s does not exist!\nDisabling Run All"), m_inject_all_value);
            MessageBox(NULL, msg, _T("DynamoRIO Configuration Error"), MB_OK | MYMBFLAGS);
#    endif
            DisableSystemwideInject();
        } else
            check.Close();
    }

    m_dll_path[0] = _T('\0');

    DisableMissingLibraries(TRUE);

    // now select the previously checked library
    int lib = GetProfileInt(_T("Settings"), _T("Library"), 1);
    int tried = 0;
    while (tried < 3) {
        if (lib == 0 && SwitchLibraries(L_DLLPATH_RELEASE, FALSE)) {
            OnLibraryRelease();
            break;
        } else if (lib == 1 && SwitchLibraries(L_DLLPATH_DEBUG, FALSE)) {
            OnLibraryDebug();
            break;
        } else if (lib == 2 && SwitchLibraries(L_DLLPATH_PROFILE, FALSE)) {
            OnLibraryProfile();
            break;
        }
        lib = (lib + 1) % 3;
        tried++;
    }
    if (tried == 3) {
#    if 0 // Disabling
        MessageBox(NULL, _T("Cannot find any DynamoRIO libraries!\n")
                   _T("Running of applications will be disabled."), _T("Error"), MB_OK | MYMBFLAGS);
#    endif
        // disable Run and libraries
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_FILE_RUN, MF_BYCOMMAND | MF_GRAYED);
        DisableSystemwideInject();
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_RELEASE,
                                              MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_PROFILE,
                                              MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_RELEASE,
                                             MF_BYCOMMAND | MF_UNCHECKED);
        m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_DEBUG,
                                             MF_BYCOMMAND | MF_UNCHECKED);
        m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_PROFILE,
                                             MF_BYCOMMAND | MF_UNCHECKED);
    }

    if (m_bInjectAll) {
        MessageBox(NULL, _T("Run All is already set!"), _T("Warning"), MB_OK | MYMBFLAGS);

        // FIXME: share this code with OnFileSystemwide
        SetEnvVarPermanently(_T("DYNAMORIO_SYSTEMWIDE"), m_dll_path);

        // disable changing the library
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_RELEASE,
                                              MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_PROFILE,
                                              MF_BYCOMMAND | MF_GRAYED);
    }
#endif /*! DRSTATS_DEMO */

    return TRUE;
}

/*static*/ BOOL
CDynamoRIOApp::GetWindowsVersion(OSVERSIONINFOW *version)
{
    /* i#1418: GetVersionEx is just plain broken on win8.1+ so we use the Rtl version */
    typedef DWORD NTSTATUS;
    typedef NTSTATUS(NTAPI * RtlGetVersion_t)(OSVERSIONINFOW * info);
#define NT_SUCCESS(res) ((res) >= 0)
    RtlGetVersion_t RtlGetVersion;
    NTSTATUS res;
    HANDLE ntdll_handle = GetModuleHandle(_T("ntdll.dll"));
    if (ntdll_handle == NULL)
        return FALSE;
    RtlGetVersion =
        (RtlGetVersion_t)GetProcAddress((HMODULE)ntdll_handle, "RtlGetVersion");
    if (RtlGetVersion == NULL)
        return FALSE;
    version->dwOSVersionInfoSize = sizeof(*version);
    res = RtlGetVersion(version);
    return NT_SUCCESS(res);
}

BOOL
CDynamoRIOApp::CheckWindowsVersion(BOOL &windows_NT)
{
    // make sure we're on an NT-based system
    windows_NT = FALSE;
    TCHAR bad_os[64];
    OSVERSIONINFOW version;
    BOOL res = GetWindowsVersion(&version);
    assert(res);
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        // WinNT or descendents: rather than continually update
        // the list of known versions here we assume they're all ok.
        // XXX i#1419: if we ever check for win8 or higher we'll want to switch
        // to RtlGetVersion().
        if (version.dwMajorVersion == 4) {
            // Windows NT
            windows_NT = TRUE;
        } else {
            // 2K or later
        }
        return TRUE;
    } else if (version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        /* Win95 or Win98 */
        unsigned ver_high = (version.dwBuildNumber >> 24) & 0xff;
        unsigned ver_low = (version.dwBuildNumber >> 16) & 0xff;
        if (ver_low >= 90 || ver_high >= 5)
            _stprintf(bad_os, _T("Windows ME"));
        else if (ver_low >= 10 && ver_low < 90)
            _stprintf(bad_os, _T("Windows 98"));
        else if (ver_low < 5)
            _stprintf(bad_os, _T("Windows 31 / WfWg"));
        else if (ver_low < 10)
            _stprintf(bad_os, _T("Windows 98"));
        else
            assert(false);
    } else {
        /* Win32S on Windows 3.1 */
        _stprintf(bad_os, _T("Win32s"));
    }
    TCHAR msg[128];
    _stprintf(msg, _T("DynamoRIO does not support %s"), bad_os);
    MessageBox(m_pMainFrame->m_hWnd, msg, _T("Fatal Error"), MB_OK | MYMBFLAGS);
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog {
public:
    CAboutDlg();

    // Dialog Data
    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
protected:
    virtual void
    DoDataExchange(CDataExchange *pDX); // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    //{{AFX_MSG(CAboutDlg)
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg()
    : CDialog(CAboutDlg::IDD)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

void
CAboutDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
//{{AFX_MSG_MAP(CAboutDlg)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void
CDynamoRIOApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOApp message handlers

CDocument *
CDynamoRIOApp::OpenDocumentFile(LPCTSTR
#ifndef DRSTATS_DEMO /* avoid warning */
                                    lpszFileName
#endif
)
{
#ifndef DRSTATS_DEMO
    RunNewApp(lpszFileName);
#endif
    return m_pMainFrame->GetActiveView()->GetDocument();
}

#ifndef DRSTATS_DEMO

static TCHAR szFilter[] = _T("Executable Files (*.exe)|*.exe|All Files (*.*)|*.*||");

void
CDynamoRIOApp::OnFileRun()
{
    CFileDialog fileDlg(TRUE, _T(".exe"), NULL,
                        // hide the "open as read-only" checkbox
                        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
                        szFilter);
    int res = fileDlg.DoModal();
    if (res == IDCANCEL)
        return;
    CString file = fileDlg.GetPathName();
    RunNewApp(file);
}

BOOL
CDynamoRIOApp::RunNewApp(LPCTSTR lpszFileName)
{
    m_pMainFrame->SetStatusBarText(0, lpszFileName);
    AddToRecentFileList(lpszFileName);
    CDynamoRIODoc *doc = (CDynamoRIODoc *)m_pMainFrame->GetActiveView()->GetDocument();
    doc->RunApplication(lpszFileName);
    return TRUE;
}

void
CDynamoRIOApp::OnFileSystemwide()
{
    assert(m_bSystemwideAllowed);

    if (!m_bInjectAll &&
        GetProfileInt(_T("Settings"), _T("Confirm Systemwide"), 1) == 1) {
        // confirm with user the gravity of this setting
        CSyswideDlg dlg;
        int res = dlg.DoModal();
        if (res == IDCANCEL)
            return;
    }

    TCHAR *val;
    if (m_bInjectAll) {
        m_bInjectAll = FALSE;
        val = _T("");

        // enable changing existing libraries
        DisableMissingLibraries(FALSE);
    } else {
        m_bInjectAll = TRUE;
        val = m_inject_all_value;

        // point DYNAMORIO_SYSTEMWIDE to the currently selected library
        SetEnvVarPermanently(_T("DYNAMORIO_SYSTEMWIDE"), m_dll_path);

        // disable changing the library
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_RELEASE,
                                              MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_GRAYED);
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_PROFILE,
                                              MF_BYCOMMAND | MF_GRAYED);
    }
    if (!SetSystemwideInject(val))
        return;
    m_pMainWnd->GetMenu()->CheckMenuItem(
        ID_FILE_SYSTEMWIDE, MF_BYCOMMAND | ((m_bInjectAll) ? MF_CHECKED : MF_UNCHECKED));
}

BOOL
CDynamoRIOApp::SetSystemwideInject(TCHAR *val)
{
    assert(m_bSystemwideAllowed);
    HKEY hk;
    // must have administrative privilegs to write this key!
    int res = RegOpenKeyEx(INJECT_ALL_HIVE, INJECT_ALL_KEY_L, 0, KEY_WRITE, &hk);
    if (res != ERROR_SUCCESS) {
        MessageBox(
            NULL,
            _T("DynamoRIO's system-wide injection method requires administrative ")
            _T("privileges.\n")
            _T("You must restart this program with such privileges to use this feature."),
            _T("Lack of Privileges"), MB_OK | MYMBFLAGS);
        // now disable systemwide injection
        m_bSystemwideAllowed = FALSE; // prevent infinite loop
        DisableSystemwideInject();
        return FALSE;
    }
    res =
        RegSetValueEx(hk, INJECT_ALL_SUBKEY_L, 0, REG_SZ, (LPBYTE)val, _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);
    RegCloseKey(hk);
    return TRUE;
}

void
CDynamoRIOApp::DisableSystemwideInject()
{
    if (m_bSystemwideAllowed) {
        // clear key
        SetSystemwideInject(_T(""));
    }
    // disable systemwide altogether
    m_bSystemwideAllowed = FALSE;
    m_bInjectAll = FALSE;
    // disable checkbox, but allow editing ignore list
    m_pMainWnd->GetMenu()->EnableMenuItem(ID_FILE_SYSTEMWIDE, MF_BYCOMMAND | MF_GRAYED);
    m_pMainFrame->SetStatusBarText(0, _T("Disabled system-wide injection"));
}

void
CDynamoRIOApp::OnEditOptions()
{
    COptionsDlg ops;
    ops.DoModal();
}

void
CDynamoRIOApp::OnEditIgnorelist()
{
    CIgnoreDlg dlg;
    dlg.DoModal();
}

void
CDynamoRIOApp::DisableMissingLibraries(BOOL notify)
{
    BOOL release_ok, debug_ok, profile_ok;
    if (SwitchLibraries(L_DLLPATH_RELEASE, notify)) {
        release_ok = TRUE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_RELEASE, MF_BYCOMMAND);
    } else {
        release_ok = FALSE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_RELEASE,
                                              MF_BYCOMMAND | MF_GRAYED);
    }
    if (SwitchLibraries(L_DLLPATH_DEBUG, notify)) {
        debug_ok = TRUE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND);
    } else {
        debug_ok = FALSE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_GRAYED);
    }
    if (SwitchLibraries(L_DLLPATH_PROFILE, notify)) {
        profile_ok = TRUE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_PROFILE, MF_BYCOMMAND);
    } else {
        profile_ok = FALSE;
        m_pMainWnd->GetMenu()->EnableMenuItem(ID_LIBRARY_PROFILE,
                                              MF_BYCOMMAND | MF_GRAYED);
    }
}

BOOL
CDynamoRIOApp::SwitchLibraries(TCHAR *newdllpath, BOOL notify)
{
    // first see if new dll exists
    CFile check;
    TCHAR msg[MAX_PATH * 2];
    TCHAR file[MAX_PATH];
    assert(_tcslen(m_dynamorio_home) + _tcslen(newdllpath) < MAX_PATH);
    _stprintf(file, _T("%s%s"), m_dynamorio_home, newdllpath);
    if (!check.Open(file, CFile::modeRead | CFile::shareDenyNone)) {
        if (notify) {
            _stprintf(msg, _T("Library %s does not exist"), file);
#    if 0 // I'm disabling this dialog until we decide to support running apps
            MessageBox(NULL, msg, _T("DynamoRIO Configuration Error"), MB_OK | MYMBFLAGS);
#    endif
        }
        return FALSE;
    }
    check.Close();

    // set dll pointer for single-app injection
    _tcscpy(m_dll_path, file);

    return TRUE;
}

void
CDynamoRIOApp::OnLibraryRelease()
{
    if (!SwitchLibraries(L_DLLPATH_RELEASE, TRUE))
        return;

    m_dll_type = DLL_RELEASE;
    COptionsDlg::CheckOptionsVersusDllType(m_dll_type);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_RELEASE, MF_BYCOMMAND | MF_CHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_PROFILE, MF_BYCOMMAND | MF_UNCHECKED);
}

void
CDynamoRIOApp::OnLibraryDebug()
{
    if (!SwitchLibraries(L_DLLPATH_DEBUG, TRUE))
        return;

    m_dll_type = DLL_DEBUG;
    COptionsDlg::CheckOptionsVersusDllType(m_dll_type);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_RELEASE, MF_BYCOMMAND | MF_UNCHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_CHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_PROFILE, MF_BYCOMMAND | MF_UNCHECKED);
}

void
CDynamoRIOApp::OnLibraryProfile()
{
    if (!SwitchLibraries(L_DLLPATH_PROFILE, TRUE))
        return;

    m_dll_type = DLL_PROFILE;
    COptionsDlg::CheckOptionsVersusDllType(m_dll_type);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_RELEASE, MF_BYCOMMAND | MF_UNCHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
    m_pMainWnd->GetMenu()->CheckMenuItem(ID_LIBRARY_PROFILE, MF_BYCOMMAND | MF_CHECKED);
}

void
CDynamoRIOApp::OnHelpHelp()
{
    TCHAR helppath[MAX_PATH];
    _stprintf(helppath, _T("%s%s"), m_dynamorio_home, HELP_PATH);

    TCHAR cwd[MAX_PATH];
    int cwd_res = GetCurrentDirectory(MAX_PATH, cwd);
    assert(cwd_res > 0);

    // launch browser on documentation file!
    HINSTANCE res =
        ShellExecute(m_pMainWnd->m_hWnd, _T("open"), helppath, NULL, cwd, SW_SHOWNORMAL);
    // FIXME: I get back 2 == SE_ERR_FNF == file not found, but netscape finds and
    // displays it fine...
    if ((int)res <= 32) {
        TCHAR msg[MAX_PATH * 2];
        _stprintf(msg, _T("Error browsing help document %s"), helppath);
        MessageBox(m_pMainWnd->m_hWnd, msg, _T("Error"), MB_OK | MYMBFLAGS);
    }
}

// MessageBox crashes in ExitInstance so I added this PreExit routine, called
// using the framework's notion of "save unsaved data?" for CDocument
void
CDynamoRIOApp::PreExit()
{
    BOOL ok = TRUE; // libraries may have been disabled
    if (_tcsstr(m_dll_path, L_DLLPATH_RELEASE) != NULL)
        ok = WriteProfileInt(_T("Settings"), _T("Library"), 0);
    else if (_tcsstr(m_dll_path, L_DLLPATH_DEBUG) != NULL)
        ok = WriteProfileInt(_T("Settings"), _T("Library"), 1);
    else if (_tcsstr(m_dll_path, L_DLLPATH_PROFILE) != NULL)
        ok = WriteProfileInt(_T("Settings"), _T("Library"), 2);
    assert(ok);

    if (m_bSystemwideAllowed && m_bInjectAll) {
        // FIXME: MessageBox crashes...try to get this code into
        // MFC's auto-framework for saving unsaved documents!
        int res = MessageBox(NULL, // m_pMainWnd is NULL now!
                             _T("Run All is currently set.  Turn it off?"),
                             _T("Confirmation"), MB_YESNO | MYMBFLAGS);
        if (res == IDYES) {
            // clear key
            SetSystemwideInject(_T(""));
        }
    }
}

void
CDynamoRIOApp::SetEnvVarPermanently(TCHAR *var, TCHAR *val)
{
    // it takes a while to broadcast the "we've changed env vars" message,
    // so set a wait cursor
    HCURSOR prev_cursor = SetCursor(LoadCursor(IDC_WAIT));

    TCHAR msg[MAX_PATH];
    HKEY hk;
    int res;

    // current user only
    res = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hk);
    if (res != ERROR_SUCCESS) {
        _stprintf(msg, _T("Error writing to HKEY_CURRENT_USER\\Environment"));
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
    }

    res = RegSetValueEx(hk, var, 0, REG_SZ, (LPBYTE)val, _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);

    RegCloseKey(hk);

    // tell system that we've changed environment (else won't take effect until
    // user logs out and back in)
    DWORD dwReturnValue;
    // Code I copied this from used an ANSI string...I'm leaving
    // it like that FIXME
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "Environment",
                       SMTO_ABORTIFHUNG, 5000, &dwReturnValue);

    // set local var too
    BOOL ok = SetEnvironmentVariable(var, val);
    assert(ok);

    SetCursor(prev_cursor);
}

BOOL
CDynamoRIOApp::ConfigureForNewUser()
{
    // Need to find location of dynamorio install
    BOOL ok = FALSE;
    TCHAR msg[MAX_PATH * 2];
    int res;
    // First attempt: look at gui's executable path
    GetModuleFileName(NULL, m_dynamorio_home, MAX_PATH);
    TCHAR *sub = _tcsstr(m_dynamorio_home, _T("\\bin\\DynamoRIO.exe"));
    if (sub != NULL) {
        *sub = _T('\0');
        _stprintf(msg, _T("Is this the location of the DynamoRIO installation?\n%s\n"),
                  m_dynamorio_home);
        res = MessageBox(NULL, msg, _T("Confirmation"), MB_YESNO | MYMBFLAGS);
        if (res == IDNO)
            ok = FALSE;
        else
            ok = TRUE;
    }
    if (!ok) {
        // Second attempt: read Start Menu's link files
        // FIXME: code that up

        // Last resort: ask user to locate directory
        BROWSEINFO bi;
        bi.hwndOwner = m_pMainFrame->m_hWnd;
        bi.pidlRoot = NULL;
        bi.pszDisplayName = m_dynamorio_home;
        bi.lpszTitle = _T("Locate Root of DynamoRIO Installation");
        bi.ulFlags = 0;
        bi.lpfn = NULL;
        bi.lParam = NULL;
        bi.iImage = 0;
        ITEMIDLIST *id = SHBrowseForFolder(&bi);
        if (id == NULL) // cancelled
            return FALSE;
        SHGetPathFromIDList(id, m_dynamorio_home);
    }

    // it takes a while to broadcast the "we've changed env vars" message,
    // so set a wait cursor
    HCURSOR prev_cursor = SetCursor(LoadCursor(IDC_WAIT));

    HKEY hk;

    // current user only
    res = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hk);
    if (res != ERROR_SUCCESS) {
        _stprintf(msg, _T("Error writing to HKEY_CURRENT_USER\\Environment"));
        MessageBox(NULL, msg, _T("Error"), MB_OK | MYMBFLAGS);
    }

    // DYNAMORIO_HOME
    TCHAR *val = m_dynamorio_home;
    res =
        RegSetValueEx(hk, _T("DYNAMORIO_HOME"), 0, REG_SZ, (LPBYTE)val, _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);
    // set local var too
    ok = SetEnvironmentVariable(_T("DYNAMORIO_HOME"), val);
    assert(ok);

    // DYNAMORIO_OPTIONS
    val = INITIAL_OPTIONS;
    res = RegSetValueEx(hk, _T("DYNAMORIO_OPTIONS"), 0, REG_SZ, (LPBYTE)val,
                        _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);
    // set local var too
    ok = SetEnvironmentVariable(_T("DYNAMORIO_OPTIONS"), val);
    assert(ok);

    // DYNAMORIO_SYSTEMWIDE
    TCHAR buf[MAX_PATH];
    _stprintf(buf, _T("%s%s"), m_dynamorio_home, INITIAL_SYSTEMWIDE);
    val = buf;
    res = RegSetValueEx(hk, _T("DYNAMORIO_SYSTEMWIDE"), 0, REG_SZ, (LPBYTE)val,
                        _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);
    // set local var too
    ok = SetEnvironmentVariable(_T("DYNAMORIO_SYSTEMWIDE"), val);
    assert(ok);

    // DYNAMORIO_IGNORE
    val = INITIAL_IGNORE;
    res = RegSetValueEx(hk, _T("DYNAMORIO_IGNORE"), 0, REG_SZ, (LPBYTE)val,
                        _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);
    // set local var too
    ok = SetEnvironmentVariable(_T("DYNAMORIO_IGNORE"), val);
    assert(ok);

    RegCloseKey(hk);

    // tell system that we've changed environment (else won't take effect until
    // user logs out and back in)
    DWORD dwReturnValue;
    // Code I copied this from used an ANSI string...I'm leaving
    // it like that FIXME
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "Environment",
                       SMTO_ABORTIFHUNG, 5000, &dwReturnValue);

    // FIXME: Doc keeps its own internal var w/ home, must update here,
    // better to have Doc grab ours!
    CDynamoRIODoc *doc = (CDynamoRIODoc *)m_pMainFrame->GetActiveView()->GetDocument();
    doc->InitPaths();

    SetCursor(prev_cursor);

    return TRUE;
}

#endif /* !DRSTATS_DEMO */

void
CDynamoRIOApp::OnAppExit()
{
    // Same as double-clicking on main window close box.
    ASSERT(AfxGetMainWnd() != NULL);
    AfxGetMainWnd()->SendMessage(WM_CLOSE);
}

int
CDynamoRIOApp::ExitInstance()
{
    // no longer need to do anything special -- PreExit does it now
    return CWinApp::ExitInstance();
}
