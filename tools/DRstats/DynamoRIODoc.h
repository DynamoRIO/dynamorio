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

// DynamoRIODoc.h : interface of the CDynamoRIODoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_DYNAMORIODOC_H__832A85FF_15EB_41D0_86C5_BE2CDD746729__INCLUDED_)
#    define AFX_DYNAMORIODOC_H__832A85FF_15EB_41D0_86C5_BE2CDD746729__INCLUDED_

#    if _MSC_VER > 1000
#        pragma once
#    endif // _MSC_VER > 1000

#    define DYNAMORIO_SHARED_MEMORY_KEY "DynamoRIOStatistics"

class CDynamoRIODoc : public CDocument {
protected: // create from serialization only
    CDynamoRIODoc();
    DECLARE_DYNCREATE(CDynamoRIODoc)

    // Attributes
public:
    // Operations
public:
    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CDynamoRIODoc)
public:
    virtual BOOL
    OnNewDocument();
    virtual void
    Serialize(CArchive &ar);

protected:
    virtual BOOL
    SaveModified();
    //}}AFX_VIRTUAL

    // Implementation
public:
#    ifndef DRSTATS_DEMO
    void
    InitPaths();
    TCHAR m_dynamorio_home[_MAX_DIR];
    TCHAR m_injector_path[MAX_PATH];
    TCHAR m_logs_dir[MAX_PATH];
#    endif

    BOOL
    OnOpenDocument(LPCTSTR lpszPathName);
    BOOL
    RunApplication(LPCTSTR lpszPathName);
    virtual ~CDynamoRIODoc();
#    ifdef _DEBUG
    virtual void
    AssertValid() const;
    virtual void
    Dump(CDumpContext &dc) const;
#    endif

protected:
    // Generated message map functions
protected:
    //{{AFX_MSG(CDynamoRIODoc)
    // NOTE - the ClassWizard will add and remove member functions here.
    //    DO NOT EDIT what you see in these blocks of generated code !
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the
// previous line.

#endif // !defined(AFX_DYNAMORIODOC_H__832A85FF_15EB_41D0_86C5_BE2CDD746729__INCLUDED_)
