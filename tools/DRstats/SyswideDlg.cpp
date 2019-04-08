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

// SyswideDlg.cpp : implementation file
//

#include "configure.h"

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "SyswideDlg.h"
#    include <assert.h>

#    ifdef _DEBUG
#        define new DEBUG_NEW
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    endif

/////////////////////////////////////////////////////////////////////////////
// CSyswideDlg dialog

CSyswideDlg::CSyswideDlg(CWnd *pParent /*=NULL*/)
    : CDialog(CSyswideDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CSyswideDlg)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
}

void
CSyswideDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CSyswideDlg)
    // NOTE: the ClassWizard will add DDX and DDV calls here
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSyswideDlg, CDialog)
//{{AFX_MSG_MAP(CSyswideDlg)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

////////////////////////////////////////////////////////////////////////////
// CSyswideDlg message handlers

void
CSyswideDlg::OnOK()
{
    CButton *check = (CButton *)GetDlgItem(IDC_NOT_AGAIN);
    assert(check != NULL);
    if (check->GetCheck() == 1) {
        CDynamoRIOApp::SetSystemwideSetting(0);
    }

    CDialog::OnOK();
}

#endif /* !DRSTATS_DEMO */ /* around whole file */
