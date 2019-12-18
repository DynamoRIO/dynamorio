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

// ShellInterface.cpp: implementation of the CShellInterface class.
//
//////////////////////////////////////////////////////////////////////

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "ShellInterface.h"

#    include <shlobj.h>
#    include <objbase.h>
#    include <shlwapi.h>
#    include <assert.h>

#    ifdef _DEBUG
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#        define new DEBUG_NEW
#    endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CShellInterface::CShellInterface()
{
}

CShellInterface::~CShellInterface()
{
}

/*static */ void
CShellInterface::Initialize()
{
    if (m_bInitialized)
        return;
    int res = CoInitialize(NULL);
    assert(res == S_OK);
    m_bInitialized = TRUE;
}

/*static */ void
CShellInterface::Uninitialize()
{
    if (!m_bInitialized)
        return;
    CoUninitialize();
    m_bInitialized = FALSE;
}

/*static */ BOOL CShellInterface::m_bInitialized = FALSE;

/*static */ BOOL
CShellInterface::ResolveLinkFile(TCHAR *name, TCHAR *resolution, TCHAR *arguments,
                                 TCHAR *working_dir, HWND hwnd)
{
    assert(m_bInitialized);
    HRESULT hres;
    IShellLink *psl;
    WIN32_FIND_DATA wfd;

    *resolution = 0; // assume failure

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
                            (void **)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile *ppf;

        // Get a pointer to the IPersistFile interface.
        hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);

        if (SUCCEEDED(hres)) {
#    ifndef UNICODE
            WORD wsz[MAX_PATH]; // buffer for Unicode string

            // Ensure that the string consists of Unicode TCHARacters.
            MultiByteToWideChar(CP_ACP, 0, name, -1, wsz, MAX_PATH);
#    else
            TCHAR *wsz = name;
#    endif

            // Load the shortcut.
            hres = ppf->Load(wsz, STGM_READ);

            if (SUCCEEDED(hres)) {
                // Resolve the shortcut.
                hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
                if (SUCCEEDED(hres)) {
                    _tcscpy(resolution, name);
                    // Get the path to the shortcut target.
                    hres = psl->GetPath(resolution, MAX_PATH, (WIN32_FIND_DATA *)&wfd,
                                        SLGP_SHORTPATH);
                    if (!SUCCEEDED(hres))
                        goto cleanup;
                    hres = psl->GetArguments(arguments, MAX_PATH);
                    if (!SUCCEEDED(hres))
                        goto cleanup;
                    hres = psl->GetWorkingDirectory(working_dir, MAX_PATH);
                    if (!SUCCEEDED(hres))
                        goto cleanup;
                }
            }
        cleanup:
            // Release the pointer to IPersistFile.
            ppf->Release();
        }
        // Release the pointer to IShellLink.
        psl->Release();
    }
    return (SUCCEEDED(hres) ? TRUE : FALSE);
}

#    if 0
// from "Shell Links" in MSDN library:
/*
The application-defined ResolveIt function in the following example
resolves a shortcut. Its parameters include a window handle, a pointer
to the path of the shortcut, and the address of a buffer that receives
the new path to the object. The window handle identifies the parent
window for any message boxes that the shell may need to display. For
example, the shell can display a message box if the link is on unshared
media, if network problems occur, if the user needs to insert a floppy
disk, and so on.

The ResolveIt function calls theCoCreateInstance function and assumes
that theCoInitialize function has already been called. Note that
ResolveIt needs to use theIPersistFile interface to store the link
information. IPersistFile is implemented by the IShellLink object. The
link information must be loaded before the path information is
retrieved, which is shown later in the example. Failing to load the link
information causes the calls to the IShellLink::GetPath and
IShellLink::GetDescription member functions to fail.
 */
HRESULT ResolveIt(HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath)
{
    HRESULT hres;
    IShellLink* psl;
    TCHAR szGotPath[MAX_PATH];
    TCHAR szDescription[MAX_PATH];
    WIN32_FIND_DATA wfd;

    *lpszPath = 0; // assume failure

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance(&CLSID_ShellLink, NULL,
                            CLSCTX_INPROC_SERVER, &IID_IShellLink, &psl);
    if (SUCCEEDED(hres)) {
        IPersistFile* ppf;

        // Get a pointer to the IPersistFile interface.
        hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile,
                                           &ppf);
        if (SUCCEEDED(hres)) {
            WORD wsz[MAX_PATH];

            // Ensure that the string is Unicode.
            MultiByteToWideChar(CP_ACP, 0, lpszLinkFile, -1, wsz,
                                MAX_PATH);

            // Load the shortcut.
            hres = ppf->lpVtbl->Load(ppf, wsz, STGM_READ);
            if (SUCCEEDED(hres)) {

                // Resolve the link.
                hres = psl->lpVtbl->Resolve(psl, hwnd, SLR_ANY_MATCH);
                if (SUCCEEDED(hres)) {

                    // Get the path to the link target.
                    hres = psl->lpVtbl->GetPath(psl, szGotPath,
                                                MAX_PATH, (WIN32_FIND_DATA *)&wfd,
                                                SLGP_SHORTPATH );
                    if (!SUCCEEDED(hres))
                        HandleErr(hres); // application-defined function

                    // Get the description of the target.
                    hres = psl->lpVtbl->GetDescription(psl,
                                                       szDescription, MAX_PATH);
                    if (!SUCCEEDED(hres))
                        HandleErr(hres);
                    lstrcpy(lpszPath, szGotPath);
                }
            }
            // Release the pointer to the IPersistFile interface.
            ppf->lpVtbl->Release(ppf);
        }
        // Release the pointer to the IShellLink interface.
        psl->lpVtbl->Release(psl);
    }
    return hres;
}

/* From MSDN's MFC sample:
"SHORTCUT: A SampleThat Manipulates Shortcuts"
*/

HRESULT CreateShortCut::CreateIt (LPCSTR pszShortcutFile, LPSTR pszLink,
                                                                  LPSTR pszDesc)
{
    HRESULT hres;
    IShellLink *psl;

    // Create an IShellLink object and get a pointer to the IShellLink
    // interface (returned from CoCreateInstance).
    hres = CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                             IID_IShellLink, (void **)&psl);
    if (SUCCEEDED (hres)) {
        IPersistFile *ppf;

        // Query IShellLink for the IPersistFile interface for
        // saving the shortcut in persistent storage.
        hres = psl->QueryInterface (IID_IPersistFile, (void **)&ppf);
        if (SUCCEEDED (hres)) {
            WORD wsz [MAX_PATH]; // buffer for Unicode string

            // Set the path to the shortcut target.
            hres = psl->SetPath (pszShortcutFile);

            if (! SUCCEEDED (hres))
                AfxMessageBox ("SetPath failed!");

            // Set the description of the shortcut.
            hres = psl->SetDescription (pszDesc);

            if (! SUCCEEDED (hres))
                AfxMessageBox ("SetDescription failed!");

            // Ensure that the string consists of ANSI TCHARacters.
            MultiByteToWideChar (CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

            // Save the shortcut via the IPersistFile::Save member function.
            hres = ppf->Save (wsz, TRUE);

            if (! SUCCEEDED (hres))
                AfxMessageBox ("Save failed!");

            // Release the pointer to IPersistFile.
            ppf->Release ();
        }
        // Release the pointer to IShellLink.
        psl->Release ();
    }
    return hres;
}


void ResolveShortCut::OnOK ()
{
    TCHAR szFile [MAX_PATH];

    // Get the selected item in the list box.
    DlgDirSelect (szFile, IDC_LIST1);

    // Find out whether it is a LNK file.
    if (_tcsstr (szFile, ".lnk") != NULL)
        // Make the call to ResolveShortcut::ResolveIt here.
        ResolveIt (m_hWnd, szFile);

    CDialog::OnOK ();
}

HRESULT ResolveShortCut::ResolveIt (HWND hwnd, LPCSTR pszShortcutFile)
{
    HRESULT hres;
    IShellLink *psl;
    TCHAR szGotPath [MAX_PATH];
    TCHAR szDescription [MAX_PATH];
    WIN32_FIND_DATA wfd;

    // Get a pointer to the IShellLink interface.
    hres = CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                             IID_IShellLink, (void **)&psl);
    if (SUCCEEDED (hres)) {
        IPersistFile *ppf;

        // Get a pointer to the IPersistFile interface.
        hres = psl->QueryInterface (IID_IPersistFile, (void **)&ppf);

        if (SUCCEEDED (hres)) {
            WORD wsz [MAX_PATH]; // buffer for Unicode string

            // Ensure that the string consists of Unicode TCHARacters.
            MultiByteToWideChar (CP_ACP, 0, pszShortcutFile, -1, wsz,
                                 MAX_PATH);

            // Load the shortcut.
            hres = ppf->Load (wsz, STGM_READ);

            if (SUCCEEDED (hres)) {
                // Resolve the shortcut.
                hres = psl->Resolve (hwnd, SLR_ANY_MATCH);
                if (SUCCEEDED (hres)) {
                    _tcscpy (szGotPath, pszShortcutFile);
                    // Get the path to the shortcut target.
                    hres = psl->GetPath (szGotPath, MAX_PATH,
                                         (WIN32_FIND_DATA *)&wfd, SLGP_SHORTPATH);
                    if (! SUCCEEDED (hres))
                        AfxMessageBox ("GetPath failed!");
                    else
                        AfxMessageBox (szGotPath);
                    // Get the description of the target.
                    hres = psl->GetDescription (szDescription, MAX_PATH);
                    if (! SUCCEEDED (hres))
                        AfxMessageBox ("GetDescription failed!");
                    else
                        AfxMessageBox (szDescription);
                }
            }
            // Release the pointer to IPersistFile.
            ppf->Release ();
        }
        // Release the pointer to IShellLink.
        psl->Release ();
    }
    return hres;
}
#    endif /* 0 */

#endif /* !DRSTATS_DEMO */ /* around whole file */
