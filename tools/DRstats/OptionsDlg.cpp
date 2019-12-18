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

// OptionsDlg.cpp : implementation file
//

#include "configure.h"

#ifndef DRSTATS_DEMO /* around whole file */

#    include "stdafx.h"
#    include "DynamoRIO.h"
#    include "OptionsDlg.h"
#    include "LoggingDlg.h"
#    include <assert.h>

#    ifdef _DEBUG
#        define new DEBUG_NEW
#        undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#    endif

enum {
    HOT_THRESHOLD = 0,
    INSTRLIBNAME,
    CACHE_BB_MAX,
    CACHE_TRACE_MAX,
    LOGLEVEL,
    LOGMASK,
    PROF_COUNTS,
    PROF_PCS,
    NOASYNCH,
    NOLINK,
    NULLCALLS,
    STATS,

    NOTIFY,
    TRACEDUMP_TEXT,

    TRACEDUMP_BINARY,
    TRACEDUMP_ORIGINS,
};
static const TCHAR *names[] = {
    _T("-hot_threshold"),
    _T("-client_lib"),
    _T("-cache_bb_max"),
    _T("-cache_trace_max"),
    _T("-loglevel"),
    _T("-logmask"),
    _T("-prof_counts"),
    _T("-prof_pcs"),
    _T("-noasynch"),
    _T("-nolink"),
    _T("-nullcalls"),
    _T("-stats"),
    _T("-notify"),

    _T("-tracedump_text"),
    _T("-tracedump_binary"),

    _T("-tracedump_origins"),
};
// numeric values are always positive!  (no leading - assumed)
enum value_type { NOVALUE = 0, NUM_DECIMAL, NUM_HEX, STRING };
static const value_type hasvalue[] = {
    NUM_DECIMAL, STRING,  NUM_DECIMAL, NUM_DECIMAL, NUM_DECIMAL,
    NUM_HEX, // logmask
    NOVALUE,     NOVALUE,
    NOVALUE, // noasynch
    NOVALUE,     NOVALUE,
    NOVALUE, // stats
    NOVALUE, // notify
    NOVALUE, // tracedump_text
    NOVALUE,

    NOVALUE,

};
static const int checkboxes[] = {
    IDC_HOT_THRESHOLD,
    IDC_INSTRLIBNAME,
    IDC_CACHE_BB_MAX,
    IDC_CACHE_TRACE_MAX,
    IDC_LOGLEVEL,
    IDC_LOGMASK,
    IDC_PROF_COUNTS,
    IDC_PROF_PCS,
    IDC_NOASYNCH,
    IDC_NOLINK,
    IDC_NULLCALLS,
    IDC_STATS,
    IDC_NOTIFY,

    IDC_TRACEDUMP_TEXT,
    IDC_TRACEDUMP_BINARY,

    IDC_TRACEDUMP_ORIGINS,
};
static const BOOL ok_with_release[] = {
    TRUE,  TRUE, TRUE, TRUE,
    FALSE, // loglevel
    FALSE, // logmask
    TRUE,  TRUE, TRUE, TRUE, TRUE,
    FALSE, // stats
    FALSE, // notify

    TRUE,  TRUE,

    TRUE,
};
static const int num_options = sizeof(checkboxes) / sizeof(int);
static CString *values[num_options];

/////////////////////////////////////////////////////////////////////////////
// COptionsDlg dialog

COptionsDlg::COptionsDlg(CWnd *pParent /*=NULL*/)
    : CDialog(COptionsDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(COptionsDlg)
    m_opstring = _T("");
    m_HotThreshold = _T("50");
    m_InstrLibName = _T("");
    m_CacheBBMax = _T("0");
    m_CacheTraceMax = _T("0");
    m_LogLevel = _T("0");
    m_LogMask = _T("0x3FFF");
    //}}AFX_DATA_INIT
    values[HOT_THRESHOLD] = &m_HotThreshold;
    values[INSTRLIBNAME] = &m_InstrLibName;
    values[CACHE_BB_MAX] = &m_CacheBBMax;
    values[CACHE_TRACE_MAX] = &m_CacheTraceMax;
    values[LOGLEVEL] = &m_LogLevel;
    values[LOGMASK] = &m_LogMask;

    // rest of initialization goes in OnInitDialog, once controls are built
}

void
COptionsDlg::DoDataExchange(CDataExchange *pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(COptionsDlg)
    DDX_Control(pDX, IDOK, m_OKButton);
    DDX_Text(pDX, IDC_OPTIONS_EDIT, m_opstring);
    DDX_Text(pDX, IDC_EDIT_HOT_THRESHOLD, m_HotThreshold);
    DDX_Text(pDX, IDC_EDIT_INSTRLIBNAME, m_InstrLibName);
    DDX_Text(pDX, IDC_EDIT_CACHE_BB_MAX, m_CacheBBMax);
    DDX_Text(pDX, IDC_EDIT_CACHE_TRACE_MAX, m_CacheTraceMax);
    DDX_Text(pDX, IDC_EDIT_LOGLEVEL, m_LogLevel);
    DDX_Text(pDX, IDC_EDIT_LOGMASK, m_LogMask);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(COptionsDlg, CDialog)
//{{AFX_MSG_MAP(COptionsDlg)
ON_BN_CLICKED(IDC_HOT_THRESHOLD, OnHotThreshold)
ON_BN_CLICKED(IDC_BROWSE_INSTRLIBNAME, OnBrowseInstrlibname)
ON_BN_CLICKED(IDC_INSTRLIBNAME, OnInstrlibname)
ON_BN_CLICKED(IDC_LOGGING_BUTTON, OnLoggingButton)
ON_EN_CHANGE(IDC_OPTIONS_EDIT, OnChangeOptionsEdit)
ON_BN_CLICKED(IDC_CACHE_BB_MAX, OnCacheBBMax)
ON_BN_CLICKED(IDC_CACHE_TRACE_MAX, OnCacheTraceMax)
ON_BN_CLICKED(IDC_LOGLEVEL, OnLoglevel)
ON_BN_CLICKED(IDC_LOGMASK, OnLogmask)
ON_BN_CLICKED(IDC_PROF_PCS, OnProfPcs)
ON_BN_CLICKED(IDC_PROF_COUNTS, OnProfCounts)
ON_BN_CLICKED(IDC_STATS, OnStats)
ON_BN_CLICKED(IDC_NOTIFY, OnNotify)

ON_BN_CLICKED(IDC_NULLCALLS, OnNullcalls)
ON_BN_CLICKED(IDC_NOLINK, OnNolink)
ON_BN_CLICKED(IDC_NOASYNCH, OnNoasynch)
ON_BN_CLICKED(IDC_TRACEDUMP_TEXT, OnTraceDumpText)
ON_BN_CLICKED(IDC_TRACEDUMP_BINARY, OnTraceDumpBinary)

ON_BN_CLICKED(IDC_TRACEDUMP_ORIGINS, OnTraceDumpOrigins)

ON_EN_CHANGE(IDC_EDIT_INSTRLIBNAME, OnChangeEditInstrlibname)
ON_BN_CLICKED(IDC_SET_PERMANENT, OnSetPermanent)
ON_EN_CHANGE(IDC_EDIT_CACHE_BB_MAX, OnChangeEditCacheBbMax)
ON_EN_CHANGE(IDC_EDIT_CACHE_TRACE_MAX, OnChangeEditCacheTraceMax)
ON_EN_CHANGE(IDC_EDIT_HOT_THRESHOLD, OnChangeEditHotThreshold)
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionsDlg message handlers

BOOL
COptionsDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // get current options string
    TCHAR path[MAX_PATH];
    int len = GetEnvironmentVariable(_T("DYNAMORIO_OPTIONS"), path, MAX_PATH);
    assert(len <= MAX_PATH);
    if (len > 0 && len <= MAX_PATH)
        m_opstring = path; // makes new storage, right?
    UpdateData(FALSE);     // FALSE means set controls

    // set controls based on opstring
    if (!CheckOpstring()) {
        MessageBox(_T("Invalid DYNAMORIO_OPTIONS string!\nThis dialog ")
                   _T("may not work properly with it."),
                   _T("Error"), MB_OK | MYMBFLAGS);
    }

    // now disable appropriate options

    // always disabled, only there so can see all options
    DisableCheckbox(IDC_PROF_PCS);
    DLL_TYPE dll_type = CDynamoRIOApp::GetDllType();
    if (dll_type != DLL_PROFILE) {
        DisableCheckbox(IDC_PROF_COUNTS);
    }
    if (dll_type == DLL_RELEASE) {
        int i;
        for (i = 0; i < num_options; i++) {
            if (!ok_with_release[i]) {
                DisableCheckbox(checkboxes[i]);
            }
        }
    }

    UpdateData(FALSE); // FALSE means set controls

    return TRUE;
}

void
COptionsDlg::OnOK()
{
    UpdateData(TRUE); // TRUE means read from controls

    // set options string
    BOOL res = SetEnvironmentVariable(_T("DYNAMORIO_OPTIONS"), m_opstring);
    assert(res);

    CDialog::OnOK();
}

void
COptionsDlg::OnChangeOptionsEdit()
{
    if (CheckOpstring()) {
        m_OKButton.EnableWindow(TRUE);
    } else {
        m_OKButton.EnableWindow(FALSE);
    }
}

static BOOL
is_quote(TCHAR ch)
{
    return (ch == _T('\'') || ch == _T('\"') || ch == _T('`'));
}

static BOOL
is_whitespace(TCHAR ch)
{
    return (ch == _T(' ') || ch == _T('\t') || ch == _T('\n') || ch == _T('\r'));
}

// returns the space delimited or quote-delimited word
// starting at strpos in the string str
// maximum word length: MAX_PATH*2
static BOOL
getword(TCHAR *str, TCHAR **strpos, TCHAR *result)
{
    result[0] = _T('\0');
    int i = 0;
    TCHAR *pos = *strpos;
    TCHAR quote = _T('\0');

    if (pos < str || pos >= str + _tcslen(str))
        return FALSE;

    if (*pos == _T('\0'))
        return FALSE; /* no more words */

    /* eat leading spaces */
    while (is_whitespace(*pos)) {
        pos++;
    }

    /* extract the word */
    if (is_quote(*pos)) {
        quote = *pos;
        pos++; /* don't include surrounding quotes in word */
    }
    while (*pos) {
        if (quote != _T('\0')) {
            /* if quoted, only end on matching quote */
            if (*pos == quote) {
                pos++; /* include the quote */
                break;
            }
        } else {
            /* if not quoted, end on whitespace */
            if (is_whitespace(*pos))
                break;
        }
        result[i++] = *pos;
        pos++;
        assert(i < MAX_PATH * 2);
    }
    if (i == 0)
        return NULL; /* no more words */

    result[i] = _T('\0');
    *strpos = pos;

    return TRUE;
}

// returns the beginning and ending positions of the
// parameter names[id] in the string str, including its value,
// if it has one (based on has_value[id]).
// beginning position includes leading whitespace!
// start is inclusive, end is exclusive
static BOOL
find_param(TCHAR *str, int id, int &start, int &end)
{
    TCHAR *pos = str;
    TCHAR *prev_pos = str;
    TCHAR word[MAX_PATH * 2];
    while (true) {
        if (!getword(str, &pos, word))
            break;
        if (_tcscmp(word, names[id]) == 0) {
            if (hasvalue[id] != NOVALUE) {
                TCHAR *last_pos = pos;
                if (!getword(str, &pos, word)) {
                    // don't return FALSE, just return empty value
                } else if ((hasvalue[id] == NUM_DECIMAL || hasvalue[id] == NUM_HEX) &&
                           word[0] == '-') {
                    // assumption: no numeric value begins with - (no negative numbers)
                    pos = last_pos;
                }
            }
            start = (int)(prev_pos - str);
            end = (int)(pos - str);
            return TRUE;
        }
        prev_pos = pos;
    }
    return FALSE;
}

static void
expand_ws_quotes(CString str, int &start, int &end)
{
    // start is inclusive, end is exclusive

    // remove preceding quote, if present
    while (start > 0 && is_quote(str.GetAt(start - 1))) {
        start--;
    }
    // now remove preceding spaces
    while (start > 0 && is_whitespace(str.GetAt(start - 1))) {
        start--;
    }
    // remove following quote, if present
    while (end < str.GetLength() && is_quote(str.GetAt(end))) {
        end++;
    }
    // remove following spaces beyond a single space
    while (end + 1 < str.GetLength() &&
           (is_whitespace(str.GetAt(end)) || is_quote(str.GetAt(end))) &&
           (is_whitespace(str.GetAt(end + 1)) || is_quote(str.GetAt(end + 1)))) {
        end++;
    }
    // remove following space if at start
    if (start == 0 && end < str.GetLength() &&
        (is_whitespace(str.GetAt(end)) || is_quote(str.GetAt(end)))) {
        end++;
    }
}

// FIXME: share parsing code with non-static routines?
/*static*/ BOOL
COptionsDlg::CheckOptionsVersusDllType(DLL_TYPE dll_type)
{
    TCHAR msg[MAX_PATH];
    // this is independent of dialog box -- so grab string separately
    int len = GetEnvironmentVariable(_T("DYNAMORIO_OPTIONS"), msg, MAX_PATH);
    if (len == 0 || len > MAX_PATH)
        msg[0] = _T('\0');
    CString opstring = msg;

    // now parse string
    int i;
    TCHAR param[MAX_PATH];
    TCHAR value[MAX_PATH];
    TCHAR *pos = opstring.GetBuffer(0);
    TCHAR *prev_pos = pos;
    while (TRUE) {
        if (!getword(opstring.GetBuffer(0), &pos, param))
            break;
        for (i = 0; i < num_options; i++) {
            if (_tcscmp(param, names[i]) == 0) {
                if (hasvalue[i] != NOVALUE) {
                    // grab next space-delimited word
                    getword(opstring.GetBuffer(0), &pos, value);
                } else
                    value[0] = _T('\0');
                if ((dll_type == DLL_RELEASE && !ok_with_release[i]) ||
                    (dll_type != DLL_PROFILE && i == PROF_COUNTS)) {
                    _stprintf(msg,
                              _T("Option \"%s%s%s\" is incompatible with the selected ")
                              _T("library.\n")
                              _T("Remove it?  Incompatible options cause failure.\n"),
                              param, (hasvalue[i] != NOVALUE) ? _T(" ") : _T(""), value);
                    int res =
                        ::MessageBox(NULL, msg, _T("Confirmation"), MB_YESNO | MYMBFLAGS);
                    if (res == IDYES) {
                        int start = (prev_pos - opstring.GetBuffer(0));
                        int end = (pos - opstring.GetBuffer(0));
                        BOOL ok;
                        expand_ws_quotes(opstring, start, end);
                        opstring = opstring.Left(start) +
                            opstring.Right(opstring.GetLength() - end);
                        // update pos
                        pos = opstring.GetBuffer(0) + start;

                        ok = SetEnvironmentVariable(_T("DYNAMORIO_OPTIONS"), opstring);
                        assert(ok);
                    }
                }
            }
        }
        prev_pos = pos;
    }
    return TRUE;
}

// examines m_opstring, sets checkboxes and edit boxes from it, and returns
// false if an error is found (doesn't finish reading string if error is found)
BOOL
COptionsDlg::CheckOpstring()
{
    UpdateData(TRUE); // TRUE means read from controls

    // now parse string
    TCHAR param[MAX_PATH];
    TCHAR value[MAX_PATH];
    TCHAR *pos = m_opstring.GetBuffer(0);
    TCHAR *prev_pos;
    BOOL valid = TRUE;
    BOOL matched_any;
    BOOL match[num_options];
    int i;
    for (i = 0; i < num_options; i++)
        match[i] = FALSE;
    DLL_TYPE dll_type = CDynamoRIOApp::GetDllType();
    while (TRUE) {
        if (!getword(m_opstring.GetBuffer(0), &pos, param))
            break;
        matched_any = FALSE;
        for (i = 0; i < num_options; i++) {
            if (_tcscmp(param, names[i]) == 0) {
                CButton *button = (CButton *)GetDlgItem(checkboxes[i]);
                assert(button != NULL);
                if (button->GetCheck() == 0)
                    button->SetCheck(1);
                match[i] = TRUE;
                matched_any = TRUE;
                if (hasvalue[i] != NOVALUE) {
                    prev_pos = pos;
                    if (!getword(m_opstring.GetBuffer(0), &pos, value)) {
                        match[i] = FALSE;
                        valid = FALSE;
                        *values[i] = _T("");
                    } else {
                        *values[i] = value;
                        if (i == INSTRLIBNAME) {
                            if (!CheckLibraryExists(value, FALSE)) {
                                match[i] = FALSE;
                                valid = FALSE;
                            }
                        }
                        if (hasvalue[i] == NUM_DECIMAL || hasvalue[i] == NUM_HEX) {
                            if (value[0] == _T('-')) {
                                // value is missing, don't claim next param as value!
                                // (for string (non-numeric) values, go ahead and do so)
                                pos = prev_pos;
                                *values[i] = _T("");
                                match[i] = FALSE;
                                valid = FALSE;
                                break;
                            }
                            int j;
                            int len = _tcslen(value);
                            for (j = 0; j < len; j++) {
                                if ((hasvalue[i] == NUM_DECIMAL &&
                                     !_istdigit(value[j])) ||
                                    (hasvalue[i] == NUM_HEX &&
                                     ((j == 0 && value[j] != _T('0')) ||
                                      (j == 1 && value[j] != _T('x') &&
                                       value[j] != _T('X')) ||
                                      (j > 1 && !_istxdigit(value[j]))))) {
                                    match[i] = FALSE;
                                    valid = FALSE;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    match[i] = TRUE;
                    matched_any = TRUE;
                }
                if (match[i]) {
                    // if made it this far, now check vs. dll type
                    if ((dll_type == DLL_RELEASE && !ok_with_release[i]) ||
                        (dll_type != DLL_PROFILE && i == PROF_COUNTS)) {
                        // leave it checked
                        valid = FALSE;
                    }
                }
                break;
            }
        }
        if (!matched_any)
            valid = FALSE;
    }
    for (i = 0; i < num_options; i++) {
        if (!match[i]) {
            CButton *button = (CButton *)GetDlgItem(checkboxes[i]);
            assert(button != NULL);
            if (button->GetCheck() > 0)
                button->SetCheck(0);
        }
    }

    UpdateData(FALSE); // FALSE means set controls
    return valid;
}

// common code for all the checkboxes
void
COptionsDlg::CheckOption(int param)
{
    CButton *button = (CButton *)GetDlgItem(checkboxes[param]);
    assert(button != NULL);
    BOOL has_value = (hasvalue[param] != NOVALUE);
    CString *value = values[param];
    assert(!has_value || value != NULL);

    UpdateData(TRUE); // TRUE means read from controls
    int start, end;
    BOOL found = find_param(m_opstring.GetBuffer(0), param, start, end);
    int checked = button->GetCheck();
    if (checked) {
        if (has_value && value->IsEmpty()) {
            button->SetCheck(0);
            return;
        }
        if (found) {
            // occurs if things get out of sync...try to recover
            RemoveOption(param);
            OnChangeOptionsEdit();
        }
        if (!m_opstring.IsEmpty() &&
            m_opstring.GetAt(m_opstring.GetLength() - 1) != _T(' ')) {
            m_opstring += CString(_T(" "));
        }
        m_opstring += CString(names[param]);
        if (has_value) {
            // if has spaces, put quotes around
            CString val = *value;
            if (val.Find(_T(' '), 0) != -1)
                val.Format(_T("\"%s\""), *value);
            m_opstring += CString(_T(" ")) + val;
        }
    } else {
        assert(found);
        RemoveOption(param);
    }
    UpdateData(FALSE); // FALSE means set controls
}

void
COptionsDlg::RemoveOption(int param)
{
    UpdateData(TRUE); // TRUE means read from controls
    int start, end;
    BOOL found = find_param(m_opstring.GetBuffer(0), param, start, end);
    if (found) {
        expand_ws_quotes(m_opstring, start, end);
        m_opstring =
            m_opstring.Left(start) + m_opstring.Right(m_opstring.GetLength() - end);
        UpdateData(FALSE); // FALSE means set controls
    }
}

BOOL
COptionsDlg::UpdateValue(int param)
{
    if (hasvalue[param] == NOVALUE)
        return FALSE;

    UpdateData(TRUE); // TRUE means read from controls

    CString *value = values[param];
    // if has spaces, put quotes around
    CString newval = *value;
    if (newval.Find(_T(' '), 0) != -1)
        newval.Format(_T("\"%s\""), *value);

    int start, end;
    BOOL found = find_param(m_opstring.GetBuffer(0), param, start, end);
    if (!found)
        return FALSE;
    TCHAR *buf = m_opstring.GetBuffer(0);
    TCHAR *pos = buf + start;
    TCHAR val[MAX_PATH];
    getword(buf, &pos, val); // param
    TCHAR *val_pos = pos;
    getword(buf, &pos, val); // value
    int val_start = (val_pos - buf);
    expand_ws_quotes(m_opstring, val_start, end);
    m_opstring = m_opstring.Left(val_start) + CString(_T(" ")) + newval +
        m_opstring.Right(m_opstring.GetLength() - end);
    UpdateData(FALSE); // FALSE means set controls
    return TRUE;
}

static TCHAR szFilter[] =
    _T("Dynamic-Linked Libraries (*.dll)|*.dll|All Files (*.*)|*.*||");

void
COptionsDlg::OnBrowseInstrlibname()
{
    CFileDialog fileDlg(TRUE, _T(".dll"), NULL,
                        // hide the "open as read-only" checkbox
                        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
                        szFilter);
    int res = fileDlg.DoModal();
    if (res == IDCANCEL)
        return;
    CButton *button = (CButton *)GetDlgItem(checkboxes[INSTRLIBNAME]);
    assert(button != NULL);
    if (button->GetCheck() > 0 && _tcscmp(m_InstrLibName, fileDlg.GetPathName()) != 0) {
        m_InstrLibName = fileDlg.GetPathName();
        UpdateData(FALSE); // FALSE means set controls
        RemoveOption(INSTRLIBNAME);
        CheckOption(INSTRLIBNAME);
    } else {
        m_InstrLibName = fileDlg.GetPathName();
        UpdateData(FALSE); // FALSE means set controls
    }
}

void
COptionsDlg::OnInstrlibname()
{
    UpdateData(TRUE); // TRUE means read from controls
    if (CheckLibraryExists(m_InstrLibName.GetBuffer(0), TRUE))
        CheckOption(INSTRLIBNAME);
    else {
        CButton *button = (CButton *)GetDlgItem(IDC_INSTRLIBNAME);
        button->SetCheck(0);
    }
}

void
COptionsDlg::OnChangeEditInstrlibname()
{
    if (UpdateValue(INSTRLIBNAME)) {
        OnChangeOptionsEdit();
    }
}

BOOL
COptionsDlg::CheckLibraryExists(TCHAR *libname, BOOL notify)
{
    // make sure file exists
    TCHAR msg[MAX_PATH * 2];
    CFile check;
    if (!check.Open(libname, CFile::modeRead | CFile::shareDenyNone)) {
        if (notify) {
            _stprintf(msg, _T("Library %s does not exist"), libname);
            MessageBox(msg, _T("Error"), MB_OK | MYMBFLAGS);
        }
        return FALSE;
    }
    check.Close();
    return TRUE;
}

void
COptionsDlg::OnLoggingButton()
{
    UpdateData(TRUE); // get values from controls
    int level = _ttoi(m_LogLevel);
    if (level < 0)
        level = 0;
    if (level > 4)
        level = 4;
    int mask;
    int res = _stscanf(m_LogMask, _T("%X"), &mask);
    if (res <= 0 || res == EOF)
        mask = 0;
    CLoggingDlg dlg(level, mask);
    res = dlg.DoModal();
    if (res == IDCANCEL)
        return;
    m_LogLevel.Format(_T("%d"), dlg.GetLevel());
    m_LogMask.Format(_T("0x%04X"), dlg.GetMask());
    UpdateData(FALSE); // write to controls

#    if 1
    UpdateValue(LOGLEVEL);
    UpdateValue(LOGMASK);
#    else
    // FIXME: change in place...for now we just remove and re-add
    CButton *button = (CButton *)GetDlgItem(checkboxes[LOGLEVEL]);
    assert(button != NULL);
    if (button->GetCheck() > 0) {
        RemoveOption(LOGLEVEL);
        CheckOption(LOGLEVEL);
    }
    button = (CButton *)GetDlgItem(checkboxes[LOGMASK]);
    assert(button != NULL);
    if (button->GetCheck() > 0) {
        RemoveOption(LOGMASK);
        CheckOption(LOGMASK);
    }
#    endif
}

void
COptionsDlg::DisableCheckbox(int id)
{
    CButton *button = (CButton *)GetDlgItem(id);
    assert(button != NULL);
    button->EnableWindow(FALSE);
}

void
COptionsDlg::OnHotThreshold()
{
    CheckOption(HOT_THRESHOLD);
}
void
COptionsDlg::OnCacheBBMax()
{
    CheckOption(CACHE_BB_MAX);
}
void
COptionsDlg::OnCacheTraceMax()
{
    CheckOption(CACHE_TRACE_MAX);
}
void
COptionsDlg::OnLoglevel()
{
    CheckOption(LOGLEVEL);
}
void
COptionsDlg::OnLogmask()
{
    CheckOption(LOGMASK);
}
void
COptionsDlg::OnProfPcs()
{
    CheckOption(PROF_PCS);
}
void
COptionsDlg::OnStats()
{
    CheckOption(STATS);
}
void
COptionsDlg::OnNullcalls()
{
    CheckOption(NULLCALLS);
}
void
COptionsDlg::OnNolink()
{
    CheckOption(NOLINK);
}
void
COptionsDlg::OnNoasynch()
{
    CheckOption(NOASYNCH);
}
void
COptionsDlg::OnTraceDumpOrigins()
{
    CheckOption(TRACEDUMP_ORIGINS);
}

// remind to pick trace dump too

void
COptionsDlg::OnProfCounts()
{

    CButton *button = (CButton *)GetDlgItem(IDC_NOTIFY);

    assert(button != NULL);

    if (button->GetCheck() == 1) {

        CButton *text = (CButton *)GetDlgItem(IDC_TRACEDUMP_TEXT);

        assert(text != NULL);

        CButton *binary = (CButton *)GetDlgItem(IDC_TRACEDUMP_BINARY);

        assert(binary != NULL);

        if (text->GetCheck() == 0 && binary->GetCheck() == 0) {

            ::MessageBox(NULL,
                         _T("Count profiling results are only visible in a trace dump.\n")

                         _T("Don't forget to select either a text or binary trace dump."),

                         _T("Reminder"), MB_OK | MYMBFLAGS);
        }
    }

    CheckOption(PROF_COUNTS);
}

// mutually exclusive
void
COptionsDlg::OnTraceDumpText()
{

    CButton *text = (CButton *)GetDlgItem(IDC_TRACEDUMP_TEXT);

    assert(text != NULL);

    CButton *binary = (CButton *)GetDlgItem(IDC_TRACEDUMP_BINARY);

    assert(binary != NULL);

    if (text->GetCheck() == 1 && binary->GetCheck() == 1) {

        ::MessageBox(NULL, _T("Trace dump must be either text or binary, not both"),

                     _T("Mutually Exclusive"), MB_OK | MYMBFLAGS);

        text->SetCheck(0);

    } else {

        CheckOption(TRACEDUMP_TEXT);
    }
}

void
COptionsDlg::OnTraceDumpBinary()
{

    CButton *text = (CButton *)GetDlgItem(IDC_TRACEDUMP_TEXT);

    assert(text != NULL);

    CButton *binary = (CButton *)GetDlgItem(IDC_TRACEDUMP_BINARY);

    assert(binary != NULL);

    if (text->GetCheck() == 1 && binary->GetCheck() == 1) {

        ::MessageBox(NULL, _T("Trace dump must be either text or binary, not both"),

                     _T("Mutually Exclusive"), MB_OK | MYMBFLAGS);

        binary->SetCheck(0);

    } else {

        CheckOption(TRACEDUMP_BINARY);
    }
}

// ask for confirmation since stderr sticky issue

// in fact, could not even let them set notify within GUI,

// but would be nice if env var brought in from cmd line showed up

void
COptionsDlg::OnNotify()
{

    TCHAR msg[MAX_PATH];

    CButton *button = (CButton *)GetDlgItem(IDC_NOTIFY);

    assert(button != NULL);

    if (button->GetCheck() == 1) {

        _stprintf(msg,
                  _T("Printing to stderr can cause unexpected failures.\n")

                  _T("Are you sure you want to set this option?\n"));

        int res = ::MessageBox(NULL, msg, _T("Confirmation"), MB_YESNO | MYMBFLAGS);

        if (res == IDYES) {

            CheckOption(NOTIFY);

        } else {

            button->SetCheck(0);
        }

    } else {

        CheckOption(NOTIFY);
    }
}

// set env var not just for this process but permanently for this user
void
COptionsDlg::OnSetPermanent()
{
    // it takes a while to broadcast the "we've changed env vars" message,
    // so set a wait cursor
    HCURSOR prev_cursor = SetCursor(LoadCursor(0, IDC_WAIT));

    TCHAR msg[MAX_PATH];
    HKEY hk;
    int res;

    // current user only
    res = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Environment"), 0, KEY_WRITE, &hk);
    if (res != ERROR_SUCCESS) {
        _stprintf(msg, _T("Error writing to HKEY_CURRENT_USER\\Environment"));
        MessageBox(msg, _T("Error"), MB_OK | MYMBFLAGS);
    }

    UpdateData(TRUE); // get values from controls
    TCHAR *val = m_opstring.GetBuffer(0);
    res = RegSetValueEx(hk, _T("DYNAMORIO_OPTIONS"), 0, REG_SZ, (LPBYTE)val,
                        _tcslen(val) + 1);
    assert(res == ERROR_SUCCESS);

    RegCloseKey(hk);

    // tell system that we've changed environment (else won't take effect until
    // user logs out and back in)
    DWORD dwReturnValue;
    // Code I copied this from used an ANSI string...I'm leaving
    // it like that FIXME
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "Environment",
                       SMTO_ABORTIFHUNG, 5000, &dwReturnValue);

    SetCursor(prev_cursor);

    // set local env var too, and avoid questions of ability to
    // cancel the permanent operation
    OnOK();
}

void
COptionsDlg::OnChangeEditCacheBbMax()
{
    if (UpdateValue(CACHE_BB_MAX)) {
        OnChangeOptionsEdit();
    }
}

void
COptionsDlg::OnChangeEditCacheTraceMax()
{
    if (UpdateValue(CACHE_TRACE_MAX)) {
        OnChangeOptionsEdit();
    }
}

void
COptionsDlg::OnChangeEditHotThreshold()
{
    if (UpdateValue(HOT_THRESHOLD)) {
        OnChangeOptionsEdit();
    }
}

#endif /* !DRSTATS_DEMO */ /* around whole file */
