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
#include "wizard.h"
#include "ShellInterface.h"
#include "assert.h"

#include <shlobj.h>
#include <objbase.h>
#include <shlwapi.h>

#ifdef _DEBUG
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CShellInterface::CShellInterface()
{
}

CShellInterface::~CShellInterface()
{
}

/*static */ BOOL CShellInterface::m_bInitialized = FALSE;

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

/*static*/ BOOL
CShellInterface::CreateLinkFile(LPCTSTR pszShortcutFile, LPTSTR pszLink, LPTSTR pszDesc)
{
    HRESULT hres;
    IShellLink *psl;

    // Create an IShellLink object and get a pointer to the IShellLink
    // interface (returned from CoCreateInstance).
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
                            (void **)&psl);
    if (SUCCEEDED(hres)) {
        IPersistFile *ppf;

        // Query IShellLink for the IPersistFile interface for
        // saving the shortcut in persistent storage.
        hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
        if (SUCCEEDED(hres)) {
            // Set the path to the shortcut target.
            hres = psl->SetPath(pszShortcutFile);

            if (!SUCCEEDED(hres))
                AfxMessageBox(_T("SetPath failed!"));

            // Set the description of the shortcut.
            hres = psl->SetDescription(pszDesc);

            if (!SUCCEEDED(hres))
                AfxMessageBox(_T("SetDescription failed!"));

#ifndef UNICODE
            // Ensure that the string consists of ANSI characters.
            WORD wsz[MAX_PATH]; // buffer for Unicode string
            MultiByteToWideChar(CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);
            // Save the shortcut via the IPersistFile::Save member function.
            hres = ppf->Save(wsz, TRUE);
#else
            // Save the shortcut via the IPersistFile::Save member function.
            hres = ppf->Save(pszLink, TRUE);
#endif

            if (!SUCCEEDED(hres))
                AfxMessageBox(_T("Save failed!"));

            // Release the pointer to IPersistFile.
            ppf->Release();
        }
        // Release the pointer to IShellLink.
        psl->Release();
    }
    return (SUCCEEDED(hres));
}

/*static*/ BOOL
CShellInterface::CopyDir(LPCTSTR from, LPCTSTR to, HWND hwnd)
{
    assert(_tcslen(from) < MAX_PATH - 1);
    assert(_tcslen(to) < MAX_PATH - 1);
    // strings must end with pair of nulls
    TCHAR myfrom[MAX_PATH + 2];
    _tcscpy(myfrom, from);
    myfrom[_tcslen(myfrom) + 1] = _T('\0');
    TCHAR myto[MAX_PATH + 2];
    _tcscpy(myto, to);
    myto[_tcslen(myto) + 1] = _T('\0');
    SHFILEOPSTRUCT fileop;
    fileop.hwnd = hwnd;
    fileop.wFunc = FO_COPY;
    fileop.pFrom = myfrom;
    fileop.pTo = myto;
    fileop.fFlags = FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_SILENT;
    int res = SHFileOperation(&fileop);
    return (res == 0);
}

/*static*/ BOOL
CShellInterface::DeleteFile(LPCTSTR name, HWND hwnd)
{
    assert(_tcslen(name) < MAX_PATH - 1);
    // string must end with pair of nulls
    TCHAR myname[MAX_PATH + 2];
    _tcscpy(myname, name);
    myname[_tcslen(myname) + 1] = _T('\0');
    SHFILEOPSTRUCT fileop;
    fileop.hwnd = hwnd;
    fileop.wFunc = FO_DELETE;
    fileop.pFrom = myname;
    fileop.pTo = NULL;
    fileop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    int res = SHFileOperation(&fileop);
    return (res == 0);
}

#if 0
/* From MSDN's MFC sample:
   "SHORTCUT: A SampleThat Manipulates Shortcuts"
   except that sample had ansi strings
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
                AfxMessageBox (_T("SetPath failed!"));

            // Set the description of the shortcut.
            hres = psl->SetDescription (pszDesc);

            if (! SUCCEEDED (hres))
                AfxMessageBox (_T("SetDescription failed!"));

            // Ensure that the string consists of ANSI characters.
            MultiByteToWideChar (CP_ACP, 0, pszLink, -1, wsz, MAX_PATH);

            // Save the shortcut via the IPersistFile::Save member function.
            hres = ppf->Save (wsz, TRUE);

            if (! SUCCEEDED (hres))
                AfxMessageBox (“Save failed!”);

            // Release the pointer to IPersistFile.
            ppf->Release ();
        }
        // Release the pointer to IShellLink.
        psl->Release ();
    }
    return hres;
}
#endif
