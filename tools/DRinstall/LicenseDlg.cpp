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

// LicenseDlg.cpp : implementation file
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1998 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

#include "stdafx.h"
#include "Wizard.h"
#include "LicenseDlg.h"
#include <assert.h>

#ifdef _DEBUG
#    define new DEBUG_NEW
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/* This text is verbatim from the modified PRBA provided by David Howard 4/2/08 */
static TCHAR LICENSE[] =
    _T("")
    _T("NOTICE: BY DOWNLOADING AND INSTALLING, COPYING OR OTHERWISE USING \r\n")
    _T("THE INTERFACES, SOFTWARE AND OTHER MATERIALS ON THIS CD, YOU \r\n")
    _T("AGREE TO BE BOUND BY THE TERMS OF THIS AGREEMENT.  IF YOU DO NOT \r\n")
    _T("AGREE TO THE TERMS OF THIS EULA, YOU MAY NOT DOWNLOAD, INSTALL, \r\n")
    _T("COPY OR USE THE INTERFACES, SOFTWARE OR OTHER MATERIALS ON THIS \r\n")
    _T("CD. \"YOU\" MEANS THE NATURAL PERSON OR THE ENTITY THAT IS \r\n")
    _T("AGREEING TO BE BOUND BY THIS EULA, THEIR EMPLOYEES AND THIRD \r\n")
    _T("PARTY CONTRACTORS THAT PROVIDE SERVICES TO YOU. YOU SHALL BE \r\n")
    _T("LIABLE FOR ANY FAILURE BY SUCH EMPLOYEES AND THIRD PARTY \r\n")
    _T("CONTRACTORS TO COMPLY WITH THE TERMS OF THIS AGREEMENT. \r\n")
    _T("  \r\n")
    _T("1. DEFINITIONS. \r\n")
    _T(" \r\n")
    _T("(a) \"Software\" shall mean the VMware software, in object code only, as set ")
    _T("\r\n")
    _T("forth in Exhibit A attached hereto. \r\n")
    _T(" \r\n")
    _T("(b) \"Documentation\" shall mean the printed or online written reference \r\n")
    _T("material, if any, that may be furnished to Licensee in conjunction with the \r\n")
    _T("Software, including, without limitation, instructions, testing guidelines, \r\n")
    _T("and end user guides. \r\n")
    _T(" \r\n")
    _T("(c) \"Intellectual Property Rights\" shall mean all intellectual property \r\n")
    _T("rights, including, without limitation, patent, copyright, trademark, and \r\n")
    _T("trade secret. \r\n")
    _T(" \r\n")
    _T("(d) \"Open Source Software\" shall mean various open source software \r\n")
    _T("components provided with the Software that are licensed to Licensee under \r\n")
    _T("the terms of the applicable license agreements included with such open \r\n")
    _T("source software components or other materials for the Software. \r\n")
    _T(" \r\n")
    _T("(e) \"Update(s)\" shall mean any modification, error correction, bug fix, \r\n")
    _T("patch or other update to or for the Software. \r\n")
    _T(" \r\n")
    _T("2. LICENSE GRANT, USE AND OWNERSHIP \r\n")
    _T(" \r\n")
    _T("(a) License.  Subject to the terms and conditions of this Agreement, VMware \r\n")
    _T("grants to Licensee a non-exclusive, revocable, non-transferable license \r\n")
    _T("(without the right to sublicense) to use the Software and Documentation \r\n")
    _T("solely for purposes of internal testing and evaluation, as well as \r\n")
    _T("development of Licensee products that communicate with the Software. \r\n")
    _T(" \r\n")
    _T("(b) Feedback. The purpose of this license is the internal testing and \r\n")
    _T("evaluation of the Software by Licensee and development by Licensee of \r\n")
    _T("Licensee products that communicate with the Software.  In furtherance of \r\n")
    _T("this purpose, Licensee shall from time to time provide feedback to VMware \r\n")
    _T("concerning the functionality and performance of the Software including, \r\n")
    _T("without limitation, identifying potential errors and improvements. \r\n")
    _T("Notwithstanding the foregoing, prior to Licensee disclosing to VMware any \r\n")
    _T("information in connection with this Agreement which Licensee considers \r\n")
    _T("proprietary or confidential, Licensee shall obtain VMware's prior written \r\n")
    _T("approval to disclose such information to VMware, and without such prior \r\n")
    _T("written approval from VMware, Licensee shall not disclose any such \r\n")
    _T("information to VMware.  Feedback and other information which is provided by \r\n")
    _T("Licensee to VMware in connection with the Software, Documentation, or this \r\n")
    _T("Agreement may be used by VMware to improve or enhance its products and, \r\n")
    _T("accordingly, VMware shall have a non-exclusive, perpetual, irrevocable, \r\n")
    _T("royalty-free, worldwide right and license to use, reproduce, disclose, \r\n")
    _T("sublicense, modify, make, have made, distribute, sell, offer for sale, \r\n")
    _T("display, perform, create derivative works, permit unmodified binary \r\n")
    _T("distribution and otherwise exploit such feedback and information without \r\n")
    _T("restriction. \r\n")
    _T(" \r\n")
    _T("(c) Restrictions.  Licensee shall not copy or use the Software (including \r\n")
    _T("the Documentation) or disseminate Confidential Information, as defined \r\n")
    _T("below, to any third party except as expressly permitted in this Agreement. \r\n")
    _T("Licensee will not, and will not permit any third party to, sublicense, \r\n")
    _T("rent, copy, modify, create derivative works of, translate, reverse \r\n")
    _T("engineer, decompile, disassemble, or otherwise reduce to human perceivable \r\n")
    _T("form any portion of the Software or Documentation.  In no event shall \r\n")
    _T("Licensee use the Software or Documentation for any commercial purpose \r\n")
    _T("except as expressly set forth in this Agreement.  The Software, \r\n")
    _T("Documentation, and all performance data and test results, including without \r\n")
    _T("limitation, benchmark test results (collectively \"Performance Data\"), \r\n")
    _T("relating to the Software are the Confidential Information of VMware, and \r\n")
    _T("will be treated in accordance with the terms of Section 4 of this \r\n")
    _T("Agreement.  Accordingly, Licensee shall not publish or disclose to any \r\n")
    _T("third party any Performance Data relating to the Software.  Licensee shall \r\n")
    _T("immediately cease all use of the Software and Documentation, upon notice \r\n")
    _T("from VMware. \r\n")
    _T(" \r\n")
    _T("(d) Ownership.  VMware shall own and retain all right, title and interest \r\n")
    _T("in and to the Intellectual Property Rights in the Software, Documentation, \r\n")
    _T("and any derivative works thereof, subject only to the license expressly set \r\n")
    _T("forth in Section 2(a) hereof.  Licensee does not acquire any other rights, \r\n")
    _T("express or implied, in the Software or Documentation.  VMWARE RESERVES ALL \r\n")
    _T("RIGHTS NOT EXPRESSLY GRANTED HEREUNDER. \r\n")
    _T(" \r\n")
    _T("(e) No Support Services.  VMware is under no obligation to support the \r\n")
    _T("Software in any way or to provide any Updates to Licensee.  In the event \r\n")
    _T("VMware, in its sole discretion, supplies any Update to Licensee, such \r\n")
    _T("Update shall be deemed Software hereunder and shall be subject to the terms \r\n")
    _T("and conditions of this Agreement. Upon VMware's release of any Update, \r\n")
    _T("Licensee shall immediately cease all use of the former version of the \r\n")
    _T("Software. \r\n")
    _T(" \r\n")
    _T("(f) Third-Party Software.  The Software enables a computer to run multiple \r\n")
    _T("instances of third-party guest operating systems and application programs. \r\n")
    _T("Licensee acknowledges that Licensee is responsible for obtaining any \r\n")
    _T("licenses necessary to operate any such third-party software, including \r\n")
    _T("guest operating systems. \r\n")
    _T(" \r\n")
    _T("(g) Open Source Software.  The terms and conditions of this Agreement shall \r\n")
    _T("not apply to any Open Source Software accompanying the Software.  Any such \r\n")
    _T("Open Source Software is provided under the terms of the open source license \r\n")
    _T("agreement or copyright notice accompanying such Open Source Software or in \r\n")
    _T("the open source licenses file accompanying the Software. \r\n")
    _T(" \r\n")
    _T("(h) Demonstration. Subject to the terms and conditions of this Agreement, \r\n")
    _T("VMware grants to Licensee a non-exclusive, revocable, non-transferable \r\n")
    _T("license (without the right to sublicense) to use the Software for \r\n")
    _T("demonstration to third parties, provided Licensee (i) has received prior \r\n")
    _T("written authorization from VMware for same, (ii) has had such third parties \r\n")
    _T("first sign a confidentiality agreement that contains nondisclosure \r\n")
    _T("restrictions substantially similar to those set forth in this Agreement, \r\n")
    _T("(iii) includes in such confidentiality agreement an acknowledgement that \r\n")
    _T("VMware does not promise or guarantee that features, functionality and/or \r\n")
    _T("modules in the Software will be included in any generally available version \r\n")
    _T("of the Software, or will be marketed separately for additional fees, and \r\n")
    _T("(iv) conducts such demonstration solely on Licensee's hardware and such \r\n")
    _T("hardware remains at all times in Licensee's possession and control. \r\n")
    _T(" \r\n")
    _T("3. TERM AND TERMINATION. This Agreement is effective as of the Effective \r\n")
    _T("Date and will continue for a one (1) year period (\"Initial Term\"), unless \r\n")
    _T("amended to establish a later expiration date (\"Subsequent Term\") by a \r\n")
    _T("written agreement signed by both parties, or until terminated as provided \r\n")
    _T("in this Agreement.  Either party may terminate this Agreement at any time \r\n")
    _T("for any reason or no reason by providing the other party advance written \r\n")
    _T("notice thereof.  Upon any expiration or termination of this Agreement, the \r\n")
    _T("rights and licenses granted to Licensee under this Agreement shall \r\n")
    _T("immediately terminate, and Licensee shall immediately cease using, and will \r\n")
    _T("return to VMware (or, at VMware's request, destroy), the Software, \r\n")
    _T("Documentation and all other tangible items in Licensee's possession or \r\n")
    _T("control that are proprietary to or contain Confidential Information.  The \r\n")
    _T("rights and obligations of the parties set forth in Sections 1, 2(b) 2(c), \r\n")
    _T("2(d), 2(e), 3, 4, 5, 6 and 7 shall survive termination or expiration of \r\n")
    _T("this Agreement for any reason. \r\n")
    _T(" \r\n")
    _T("4. CONFIDENTIALITY. \"Confidential Information\" shall mean all trade \r\n")
    _T("secrets, know-how, inventions, techniques, processes, algorithms, software \r\n")
    _T("programs, hardware, schematics, planned product features, functionality, \r\n")
    _T("methodology, performance and software source documents relating to the \r\n")
    _T("Software, and other information provided by VMware, whether disclosed \r\n")
    _T("orally, in writing, or by examination or inspection, other than information \r\n")
    _T("which Licensee can demonstrate (i) was already known to Licensee, other \r\n")
    _T("than under an obligation of confidentiality, at the time of disclosure; \r\n")
    _T("(ii) was generally available in the public domain at the time of disclosure \r\n")
    _T("to Licensee; (iii) became generally available in the public domain after \r\n")
    _T("disclosure other than through any act or omission of Licensee; (iv) was \r\n")
    _T("subsequently lawfully disclosed to Licensee by a third party without any \r\n")
    _T("obligation of confidentiality; or (v) was independently developed by \r\n")
    _T("Licensee without use of or reference to any information or materials \r\n")
    _T("disclosed by VMware or its suppliers.  Confidential Information shall \r\n")
    _T("include without limitation the Software, Documentation, Performance Data, \r\n")
    _T("any Updates, information relating to VMware products, product roadmaps, and \r\n")
    _T("other technical, business, financial and product development plans, \r\n")
    _T("forecasts and strategies.  Licensee shall not use any Confidential \r\n")
    _T("Information for any purpose other than as expressly authorized under this \r\n")
    _T("Agreement.  Except as otherwise set forth in this Agreement, in no event \r\n")
    _T("shall Licensee use the Software, Documentation or any other Confidential \r\n")
    _T("Information to develop, manufacture, market, sell, or distribute any \r\n")
    _T("product or service.  Licensee shall limit dissemination of Confidential \r\n")
    _T("Information to its employees who have a need to know such Confidential \r\n")
    _T("Information for purposes expressly authorized under this Agreement.  Except \r\n")
    _T("as otherwise set forth in this Agreement, in no event shall Licensee \r\n")
    _T("disclose any Confidential Information to any third party. Without limiting \r\n")
    _T("the foregoing, Licensee shall use at least the same degree of care that it \r\n")
    _T("uses to prevent the disclosure of its own confidential information of like \r\n")
    _T("importance, but in no event less than reasonable care, to prevent the \r\n")
    _T("disclosure of Confidential Information. \r\n")
    _T(" \r\n")
    _T("5. LIMITATION OF LIABILITY.  IT IS UNDERSTOOD THAT THE SOFTWARE, \r\n")
    _T("DOCUMENTATION AND ANY UPDATES ARE PROVIDED WITHOUT CHARGE FOR THE \r\n")
    _T("PURPOSES OF THIS AGREEMENT ONLY.  ACCORDINGLY, THE TOTAL \r\n")
    _T("LIABILITY OF VMWARE AND ITS SUPPLIERS ARISING OUT OF OR RELATED \r\n")
    _T("TO THIS AGREEMENT SHALL NOT EXCEED $100.  IN NO EVENT SHALL \r\n")
    _T("VMWARE OR ITS SUPPLIERS HAVE LIABILITY FOR ANY INDIRECT, \r\n")
    _T("INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES (INCLUDING, WITHOUT \r\n")
    _T("LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS \r\n")
    _T("INTERRUPTION, OR LOSS OF BUSINESS INFORMATION), HOWEVER CAUSED \r\n")
    _T("AND ON ANY THEORY OF LIABILITY, EVEN IF VMWARE OR ITS SUPPLIERS \r\n")
    _T("HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.  THESE \r\n")
    _T("LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL \r\n")
    _T("PURPOSE OF ANY LIMITED REMEDY. \r\n")
    _T(" \r\n")
    _T("6. WARRANTY DISCLAIMER.  IT IS UNDERSTOOD THAT THE SOFTWARE, \r\n")
    _T("DOCUMENTATION, AND ANY UPDATES MAY CONTAIN ERRORS AND ARE \r\n")
    _T("PROVIDED FOR THE PURPOSES OF THIS AGREEMENT ONLY.  THE SOFTWARE, \r\n")
    _T("DOCUMENTATION, AND ANY UPDATES ARE PROVIDED \"AS IS\" WITHOUT \r\n")
    _T("WARRANTY OF ANY KIND, WHETHER EXPRESS, IMPLIED, STATUTORY, OR \r\n")
    _T("OTHERWISE. VMWARE AND ITS SUPPLIERS SPECIFICALLY DISCLAIM ALL \r\n")
    _T("IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT, AND \r\n")
    _T("FITNESS FOR A PARTICULAR PURPOSE.  \r\n")
    _T("Licensee acknowledges that VMware has not publicly \r\n")
    _T("announced the availability of the Software and/or Documentation, that \r\n")
    _T("VMware has not promised or guaranteed to Licensee that such Software and/or \r\n")
    _T("Documentation will be announced or made available to anyone in the future, \r\n")
    _T("that VMware has no express or implied obligation to Licensee to announce or \r\n")
    _T("introduce the Software and/or Documentation, and that VMware may not \r\n")
    _T("introduce a product similar or compatible with the Software and/or \r\n")
    _T("Documentation. Accordingly, Licensee acknowledges that any research or \r\n")
    _T("development that it performs regarding the Software or any product \r\n")
    _T("associated with the Software is done entirely at Licensee's own risk. \r\n")
    _T("Specifically, the Software may contain features, functionality or modules \r\n")
    _T("that may not be included in the generally available version of the Software \r\n")
    _T("and/or Documentation, or that may be marketed separately for additional \r\n")
    _T("fees. \r\n")
    _T(" \r\n")
    _T("7. OTHER PROVISIONS \r\n")
    _T(" \r\n")
    _T("(a) Governing Law.  This Agreement, and all disputes arising out of or \r\n")
    _T("related thereto, shall be governed by and construed under the laws of the \r\n")
    _T("State of California without reference to conflict of laws principles.  All \r\n")
    _T("such disputes shall be subject to the exclusive jurisdiction of the state \r\n")
    _T("and federal courts located in Santa Clara County, California, and the \r\n")
    _T("parties agree and submit to the personal and exclusive jurisdiction and \r\n")
    _T("venue of these courts. \r\n")
    _T(" \r\n")
    _T("(b) Assignment.  Licensee shall not assign this Agreement or any rights or \r\n")
    _T("obligations hereunder, directly or indirectly, by operation of law, merger, \r\n")
    _T("acquisition of stock or assets, or otherwise, without the prior written \r\n")
    _T("consent of VMware.  Subject to the foregoing, this Agreement shall inure to \r\n")
    _T("the benefit of and be binding upon the parties and their respective \r\n")
    _T("successors and permitted assigns. \r\n")
    _T(" \r\n")
    _T("(c) Export Regulations.  Licensee understands that VMware is subject to \r\n")
    _T("regulation by the U.S. government and its agencies, which prohibit export \r\n")
    _T("or diversion of certain technical products and information to certain \r\n")
    _T("countries and individuals.  Licensee warrants that it will comply in all \r\n")
    _T("respects with all export and re-export restrictions applicable to the \r\n")
    _T("technology and documentation provided hereunder. \r\n")
    _T(" \r\n")
    _T("(d) Entire Agreement.  This is the entire agreement between the parties \r\n")
    _T("relating to the subject matter hereof and all other terms are rejected. \r\n")
    _T("This Agreement supersedes all previous communications, representations, \r\n")
    _T("understandings and agreements, either oral or written, between the parties \r\n")
    _T("with respect to said subject matter.  The terms of this Agreement supersede \r\n")
    _T("any VMware end user license agreement that may accompany the Software \r\n")
    _T("and/or Documentation.  No waiver or modification of this Agreement shall be \r\n")
    _T("valid unless made in a writing signed by both parties.  The waiver of a \r\n")
    _T("breach of any term hereof shall in no way be construed as a waiver of any \r\n")
    _T("term or other breach hereof.  If any provision of this Agreement is held by \r\n")
    _T("a court of competent jurisdiction to be contrary to law the remaining \r\n")
    _T("provisions of this Agreement shall remain in full force and effect. \r\n")
    _T(" \r\n")
    _T("(e) Notices. All notices must be sent by (a) registered or certified mail, \r\n")
    _T("return receipt requested, (b) reputable overnight air courier, (c) \r\n")
    _T("facsimile with a confirmation copy sent by registered or certified mail, \r\n")
    _T("return receipt requested, or (d) served personally.  Notices are effective \r\n")
    _T("immediately when served personally, five (5) days after posting if sent by \r\n")
    _T("registered or certified mail, two (2) days after being sent by overnight \r\n")
    _T("courier, or one (1) day after being transmitted by facsimile.  Notices to \r\n")
    _T("either party shall be directed to the party's address set forth in this \r\n")
    _T("Agreement.  Either party may change its address for notification under this \r\n")
    _T("Agreement, by notifying the other party in accordance with this Section. \r\n")
    _T(" \r\n");

/////////////////////////////////////////////////////////////////////////////
// CLicenseDlg property page

IMPLEMENT_DYNCREATE(CLicenseDlg, CPropertyPage)

CLicenseDlg::CLicenseDlg()
    : CPropertyPage(CLicenseDlg::IDD)
{
    //{{AFX_DATA_INIT(CLicenseDlg)
    m_License = _T("");
    //}}AFX_DATA_INIT
    m_psp.dwFlags |= PSP_HIDEHEADER;
    m_psp.dwFlags &= ~(PSP_HASHELP);
}

CLicenseDlg::~CLicenseDlg()
{
}

void
CLicenseDlg::DoDataExchange(CDataExchange *pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CLicenseDlg)
    DDX_Control(pDX, IDC_LICENSE, m_LicenseEdit);
    DDX_Text(pDX, IDC_LICENSE, m_License);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CLicenseDlg, CPropertyPage)
//{{AFX_MSG_MAP(CLicenseDlg)
ON_BN_CLICKED(IDC_AGREE, OnAgree)
ON_BN_CLICKED(IDC_DISAGREE, OnDisagree)
ON_BN_CLICKED(IDC_UNDERSTAND, OnUnderstand)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLicenseDlg message handlers

BOOL
CLicenseDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_FirstTime = TRUE;

    m_License.Format(_T("%s"), LICENSE);
    m_LicenseEdit.SetSel(0, 0);
    UpdateData(FALSE); // write to screen

    /* I had a hard time setting up the scroll-to-bottom-before-can-accept
     * functionality: I tried using a separate CScrollBar widget, like I ended
     * up doing for the DRstats viewer, and I got that working for scrolling
     * with the mouse (had to manually scroll the license text via LineScroll(),
     * and set the scroll parameters after calculating using a RECT for the
     * CEdit dimensions),
     * but I never was able to intercept keyboard scrolling, even trying:
        ON_WM_KEYUP()
        ON_WM_KEYDOWN()
        ON_EN_UPDATE(IDC_LICENSE, OnUpdateLicense)
     * But I found a hint online that all you have to do is subclass CEdit, so that's
     * what I do now, using LicEdit.
     */

    return TRUE;
}

BOOL
CLicenseDlg::OnSetActive()
{
    if (!CheckWindowsVersion()) {
        CPropertyPage::EndDialog(IDCANCEL);
    }

    CPropertySheet *pSheet = (CPropertySheet *)GetParent();
    ASSERT_KINDOF(CPropertySheet, pSheet);
    if (m_FirstTime) {
        m_FirstTime = FALSE;

        //  Disable everything until the user scrolls the license text
        CButton *radio_all = (CButton *)GetDlgItem(IDC_DISAGREE);
        radio_all->SetCheck(1);
        CButton *understand = (CButton *)GetDlgItem(IDC_UNDERSTAND);
        understand->SetCheck(BST_UNCHECKED);
        OnUnderstand();
        understand->EnableWindow(FALSE);
        pSheet->SetWizardButtons(0);
        UpdateData(FALSE); // write to screen
    } else {
        pSheet->SetWizardButtons(PSWIZB_NEXT);
    }
    return CPropertyPage::OnSetActive();
}

BOOL
CLicenseDlg::CheckWindowsVersion()
{
    // make sure we're on an NT-based system
    TCHAR bad_os[64];
    OSVERSIONINFO version;
    version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    int res = GetVersionEx(&version);
    assert(res != 0);
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        // WinNT or descendents: rather than continually update
        // the list of known versions here we assume they're all ok.
        if (version.dwMajorVersion == 4) {
            // Windows NT
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
    MessageBox(msg, _T("Fatal Error"), MB_OK | MYMBFLAGS);
    return FALSE;
}

void
CLicenseDlg::OnAgree()
{
    CPropertySheet *pSheet = (CPropertySheet *)GetParent();
    ASSERT_KINDOF(CPropertySheet, pSheet);
    // enable Next
    pSheet->SetWizardButtons(PSWIZB_NEXT);
}

void
CLicenseDlg::OnDisagree()
{
    CPropertySheet *pSheet = (CPropertySheet *)GetParent();
    ASSERT_KINDOF(CPropertySheet, pSheet);
    // disable Next
    pSheet->SetWizardButtons(0);
}

void
CLicenseDlg::OnUnderstand()
{
    CPropertySheet *pSheet = (CPropertySheet *)GetParent();
    ASSERT_KINDOF(CPropertySheet, pSheet);
    CButton *button = (CButton *)GetDlgItem(IDC_UNDERSTAND);
    if (button->GetCheck()) {
        // enable Radio Buttons
        GetDlgItem(IDC_AGREE)->EnableWindow(TRUE);
        GetDlgItem(IDC_DISAGREE)->EnableWindow(TRUE);
    } else {
        // disable Next and Radio Buttons
        pSheet->SetWizardButtons(0);
        // disable Radio Buttons
        GetDlgItem(IDC_AGREE)->EnableWindow(FALSE);
        GetDlgItem(IDC_DISAGREE)->EnableWindow(FALSE);
    }
}

/////////////////////////////////////////////////////////////////////////////
// LicEdit

BEGIN_MESSAGE_MAP(LicEdit, CEdit)
//{{AFX_MSG_MAP(LicEdit)
ON_WM_VSCROLL()
ON_CONTROL_REFLECT(EN_VSCROLL, OnEnVscroll)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void
LicEdit::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    // TODO: Add your message handler code here and/or call default
    SCROLLINFO info;
    info.cbSize = sizeof(SCROLLINFO);
    GetScrollInfo(SB_VERT, &info, SIF_RANGE | SIF_POS | SIF_PAGE);

    if (info.nPos + info.nPage > (unsigned int)info.nMax) {
        // If user scrolls to bottom, enable moving on
        GetParent()->GetDlgItem(IDC_UNDERSTAND)->EnableWindow(TRUE);
    }

    CEdit::OnVScroll(nSBCode, nPos, pScrollBar);
}

void
LicEdit::OnEnVscroll()
{
    SCROLLINFO info;
    info.cbSize = sizeof(SCROLLINFO);
    GetScrollInfo(SB_VERT, &info, SIF_RANGE | SIF_POS | SIF_PAGE);

    if (info.nPos + info.nPage > (unsigned int)info.nMax) {
        // If user scrolls to bottom, enable moving on
        GetParent()->GetDlgItem(IDC_UNDERSTAND)->EnableWindow(TRUE);
    }
}
