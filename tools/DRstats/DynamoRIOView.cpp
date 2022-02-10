/* **********************************************************
 * Copyright (c) 2013-2020 Google, Inc.  All rights reserved.
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

// DynamoRIOView.cpp : implementation of the CDynamoRIOView class
//

#include "stdafx.h"
#include "DynamoRIO.h"

#include "DynamoRIODoc.h"
#include "DynamoRIOView.h"
#include "DynamoRIO.h" // for global GetActiveView
#include "LoggingDlg.h"
#include <assert.h>

#ifdef _DEBUG
#    define new DEBUG_NEW
#    undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOView

IMPLEMENT_DYNCREATE(CDynamoRIOView, CFormView)

BEGIN_MESSAGE_MAP(CDynamoRIOView, CFormView)
//{{AFX_MSG_MAP(CDynamoRIOView)
#ifndef DRSTATS_DEMO
ON_BN_CLICKED(IDC_CHANGE_LOGGING, OnChangeLogging)
ON_BN_CLICKED(IDC_LOGDIR_EXPLORE, OnLogDirExplore)
#endif
ON_COMMAND(ID_EDIT_COPYSTATS, OnEditCopystats)
ON_WM_VSCROLL()
//}}AFX_MSG_MAP
// Standard printing commands
ON_COMMAND(ID_FILE_PRINT, CFormView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_DIRECT, CFormView::OnFilePrint)
ON_COMMAND(ID_FILE_PRINT_PREVIEW, CFormView::OnFilePrintPreview)
// process list
ON_CBN_SELCHANGE(IDC_PROCESS_LIST, OnSelchangeList)
ON_CBN_DROPDOWN(IDC_PROCESS_LIST, OnDropdownList)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOView construction/destruction

VOID CALLBACK
TimerProc(HWND /*hwnd*/,    // handle of window for timer messages
          UINT /*uMsg*/,    // WM_TIMER message
          UINT_PTR idEvent, // timer identifier
          DWORD /*dwTime*/  // current system time
)
{
    CDynamoRIOView *view = CDynamoRIOApp::GetActiveView();
    if (view == NULL) {
        KillTimer(NULL, idEvent);
        return;
    }
    BOOL cont = view->Refresh();
    if (!cont) {
        KillTimer(NULL, idEvent);
    }
}

void
CDynamoRIOView::ZeroStrings()
{
    m_Exited = _T("");
    m_ClientStats = _T("");
#ifndef DRSTATS_DEMO
    m_LogLevel = _T("0");
    m_LogMask = _T("0x0000");
    m_LogDir = _T("");
#endif
}

CDynamoRIOView::CDynamoRIOView()
    : CFormView(CDynamoRIOView::IDD)
{
    //{{AFX_DATA_INIT(CDynamoRIOView)
    //}}AFX_DATA_INIT
    // TODO: add construction code here

    ZeroStrings();
    m_stats = NULL;
    m_clientMap = NULL;
    m_clientView = NULL;
    m_clientStats = NULL;
    m_list_pos = 0;
    m_selected_pid = 0;
    m_StatsViewLines = 0;

    OSVERSIONINFOW version;
    CDynamoRIOApp::GetWindowsVersion(&version);
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT && version.dwMajorVersion == 4)
        m_windows_NT = TRUE;
    else
        m_windows_NT = FALSE;
}

void
CDynamoRIOView::ClearData()
{
    if (m_stats != NULL) {
        free_dynamorio_stats(m_stats);
        m_stats = NULL;
    }
    if (m_clientMap != NULL) {
        assert(m_clientView != NULL);
        UnmapViewOfFile(m_clientView);
        CloseHandle(m_clientMap);
        m_clientMap = m_clientView = m_clientStats = NULL;
    }
}

CDynamoRIOView::~CDynamoRIOView()
{
    ClearData();
}

void
CDynamoRIOView::DoDataExchange(CDataExchange *pDX)
{
    CFormView::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CDynamoRIOView)
    DDX_Control(pDX, IDC_PROCESS_LIST, m_ProcessList);
    DDX_Control(pDX, IDC_STATS, m_StatsCtl);
    DDX_Control(pDX, IDC_STATS_SCROLLBAR, m_StatsSB);
    DDX_Text(pDX, IDC_CLIENTSTATS, m_ClientStats);
    DDX_Text(pDX, IDC_EXITED, m_Exited);
#ifndef DRSTATS_DEMO
    DDX_Text(pDX, IDC_LOGLEVEL_VALUE, m_LogLevel);
    DDX_Text(pDX, IDC_LOGMASK_VALUE, m_LogMask);
    DDX_Text(pDX, IDC_LOGDIR, m_LogDir);
#endif
    //}}AFX_DATA_MAP
}

BOOL
CDynamoRIOView::PreCreateWindow(CREATESTRUCT &cs)
{
    // TODO: Modify the Window class or styles here by modifying
    //  the CREATESTRUCT cs

    return CFormView::PreCreateWindow(cs);
}

void
CDynamoRIOView::OnInitialUpdate()
{
    CFormView::OnInitialUpdate();
    GetParentFrame()->RecalcLayout();
    ResizeParentToFit();
    OnDropdownList();
    // 100 flashes too much with long stats list
    //      ::SetTimer(NULL, NULL, 100, TimerProc);
    ::SetTimer(NULL, NULL, 200, TimerProc);
}

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOView printing

BOOL
CDynamoRIOView::OnPreparePrinting(CPrintInfo *pInfo)
{
    // default preparation
    return DoPreparePrinting(pInfo);
}

void
CDynamoRIOView::OnBeginPrinting(CDC * /*pDC*/, CPrintInfo * /*pInfo*/)
{
    // TODO: add extra initialization before printing
}

void
CDynamoRIOView::OnEndPrinting(CDC * /*pDC*/, CPrintInfo * /*pInfo*/)
{
    // TODO: add cleanup after printing
}

void
CDynamoRIOView::OnPrint(CDC * /*pDC*/, CPrintInfo * /*pInfo*/)
{
    // TODO: add customized printing code here
}

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOView diagnostics

#ifdef _DEBUG
void
CDynamoRIOView::AssertValid() const
{
    CFormView::AssertValid();
}

void
CDynamoRIOView::Dump(CDumpContext &dc) const
{
    CFormView::Dump(dc);
}

CDynamoRIODoc *
CDynamoRIOView::GetDocument() // non-debug version is inline
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDynamoRIODoc)));
    return (CDynamoRIODoc *)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDynamoRIOView message handlers

/////////////////////////////////////////////////////////////////////////////

BOOL
pw_callback_under_dr(process_info_t *pi, void **param)
{
    TCHAR *resstr;
    TCHAR reschar;
    int res;
    TCHAR buf[MAXIMUM_PATH];
    DWORD version;
    BOOL under_dr;
    CDynamoRIOView *view = (CDynamoRIOView *)param;

    version = 0;
    res = under_dynamorio_ex(pi->ProcessID, &version);
    switch (res) {
    case DLL_PROFILE:
        resstr = _T("SC profile");
        reschar = _T('P');
        break;
    case DLL_RELEASE:
        resstr = _T("SC release");
        reschar = _T('R');
        break;
    case DLL_DEBUG:
        resstr = _T("SC debug");
        reschar = _T('D');
        break;
    case DLL_CUSTOM:
        resstr = _T("SC custom");
        reschar = _T('C');
        break;
    case DLL_NONE:
        resstr = _T("native");
        reschar = _T('N');
        break;
    case DLL_UNKNOWN:
    default: resstr = _T("<error>"); reschar = _T('?');
    }
    under_dr = !(res == DLL_NONE || res == DLL_UNKNOWN);
    if (under_dr) {
        _stprintf(buf, _T("%5d  %c   %") UNICODE_PRINTF, pi->ProcessID, reschar,
                  pi->ProcessName);
        view->m_ProcessList.InsertString(view->m_list_pos, buf);
        view->m_ProcessList.SetItemData(view->m_list_pos, pi->ProcessID);
        view->m_list_pos++;
    }
    return TRUE;
}

void
CDynamoRIOView::EnumerateInstances()
{
    // clear all old data
    ClearData();
    m_ProcessList.ResetContent();

    m_list_pos = 0;
    process_walk(&pw_callback_under_dr, (void **)this);

    // now insert the 0th entry, the default selection
    if (m_ProcessList.GetCount() > 0) {
        m_ProcessList.InsertString(0, _T("<select an instance>"));
    } else {
        m_ProcessList.InsertString(0, _T("<no instances to view>"));
    }
    m_ProcessList.SetItemData(0, 0);
    m_ProcessList.SetCurSel(0);
    CString pname;
    m_ProcessList.GetLBText(m_ProcessList.GetCurSel(), pname);
    GetDocument()->SetTitle(pname);
}

void
CDynamoRIOView::OnSelchangeList()
{
    TCHAR buf[128];

    ClearData();

    m_selected_pid = (DWORD)m_ProcessList.GetItemData(m_ProcessList.GetCurSel());

    // find the client stats shared memory that corresponds to this process
    HANDLE statsCountMap = OpenFileMapping(FILE_MAP_READ, FALSE,
                                           m_windows_NT ? TEXT(CLIENT_SHMEM_KEY_NT)
                                                        : TEXT(CLIENT_SHMEM_KEY));
    if (statsCountMap != NULL) {
        PVOID statsCountView = MapViewOfFile(statsCountMap, FILE_MAP_READ, 0, 0, 0);
        assert(statsCountView != NULL);
        int statsCount = *((int *)statsCountView);
        int num = 0;
        // see flakiness comments above on main stats count: we go to +20 as workaround
        while (num < statsCount + 20) {
            _stprintf(buf, _T("%") ASCII_PRINTF _T(".%03d"),
                      m_windows_NT ? CLIENT_SHMEM_KEY_NT : CLIENT_SHMEM_KEY, num);
            m_clientMap = OpenFileMapping(FILE_MAP_READ, FALSE, buf);
            if (m_clientMap != NULL) {
                m_clientView = MapViewOfFile(m_clientMap, FILE_MAP_READ, 0, 0, 0);
                assert(m_clientView != NULL);
                m_clientStats = (client_stats *)m_clientView;
                if (m_clientStats->pid == m_selected_pid)
                    break;
                UnmapViewOfFile(m_clientView);
                CloseHandle(m_clientMap);
                m_clientMap = m_clientView = m_clientStats = NULL;
            }
            num++;
        }
        UnmapViewOfFile(statsCountView);
        CloseHandle(statsCountMap);
    }

    m_StatsSB.SetScrollPos(0);

    Refresh();
}

void
CDynamoRIOView::OnDropdownList()
{
    EnumerateInstances();
}

BOOL
CDynamoRIOView::SelectProcess(int pid)
{
    EnumerateInstances();

    TCHAR prefix[16];
    _stprintf(prefix, _T("%5d"), pid);
    int num = m_ProcessList.FindString(-1, prefix);
    if (num == CB_ERR)
        return FALSE;
    m_ProcessList.SetCurSel(num);
    OnSelchangeList();

    return TRUE;
}

BOOL
CDynamoRIOView::UpdateProcessList()
{
    EnumerateInstances();
    CString first;
    m_ProcessList.GetLBText(0, first);
    if (first.Compare(_T("<select an instance>")) != 0)
        return FALSE;
    // Incredibly, VC6.0 says "error unreachable code" if I use an else here
    return TRUE;
}

#ifdef X64
/* For x64 we don't go all the way to %19 (would have to resize the window or
 * really shrink the names).  Instead we add 3 more digits which gets us
 * up to hundreds of billions, and we shrunk stat names by 3 to 47.
 */
/* SZFC is 2 literals => _T only gets first, so we manually construct: */
#    define CLIENT_STAT_PFMT _T("%13") _T(INT64_FORMAT) _T("u")
#    define DR_STAT_PFMT _T("%10") _T(INT64_FORMAT) _T("u")
#else
/* 13 is beyond 32-bit reach but this lines everything up and is consistent w/ x64 */
#    define CLIENT_STAT_PFMT _T("%13") _T(SZFC)
#    define DR_STAT_PFMT _T("%10") _T(SZFC)
#endif

uint
CDynamoRIOView::PrintStat(TCHAR *c, uint i, BOOL filter)
{
    if (filter) {
#if 0
        // Filter out persisted cache stat for now
        // FIXME: have "show 0 values" checkbox
        // FIXME: need to count up stats that match filter and use that
        // for scrollbar max, else get flicker and empty space at bottom
        if (strncmp(m_stats->stats[i].name, "Persisted caches",
                    strlen("Persisted caches")) == 0)
            return 0;
#endif
    }
    return _stprintf(c, _T("%*.*") ASCII_PRINTF _T(" = ") DR_STAT_PFMT _T("\r\n"),
                     (int)BUFFER_SIZE_ELEMENTS(m_stats->stats[i].name),
                     (int)BUFFER_SIZE_ELEMENTS(m_stats->stats[i].name),
                     m_stats->stats[i].name, m_stats->stats[i].value);
}

uint
CDynamoRIOView::PrintClientStats(TCHAR *c, TCHAR *max)
{
    if (m_clientStats == NULL)
        return 0;
#define CLIENTSTAT_NAME_MAX_SHOW CLIENTSTAT_NAME_MAX_LEN
    uint i;
    TCHAR *start = c;
    char(*names)[CLIENTSTAT_NAME_MAX_LEN] =
        (char(*)[CLIENTSTAT_NAME_MAX_LEN])m_clientStats->data;
    stats_int_t *vals = (stats_int_t *)((char *)m_clientStats->data +
                                        m_clientStats->num_stats *
                                            CLIENTSTAT_NAME_MAX_LEN * sizeof(char));
    /* account for struct alignment */
    vals = (stats_int_t *)ALIGN_FORWARD(vals, sizeof(stats_int_t));
    for (i = 0; i < m_clientStats->num_stats; i++) {
        if (c >= max - CLIENTSTAT_NAME_MAX_SHOW * 2 - 3)
            break;
        c += _stprintf(c, _T("%*.*") ASCII_PRINTF _T(" = ") CLIENT_STAT_PFMT _T("\r\n"),
                       CLIENTSTAT_NAME_MAX_SHOW, CLIENTSTAT_NAME_MAX_SHOW, names[i],
                       vals[i]);
        assert(c < max);
    }
    return (uint)(c - start);
}

// FIXME: resize stats boxes w/ dialog resize:
// http://www.codeguru.com/forum/showthread.php?t=79384

BOOL
CDynamoRIOView::Refresh()
{
    if (m_selected_pid <= 0) {
        ZeroStrings();
        return TRUE;
    }
    // We have to grab new stats every refresh
    dr_statistics_t *new_stats = get_dynamorio_stats(m_selected_pid);
    if (new_stats == NULL) {
        if (m_stats != NULL) {
            // Leave the stats for an exited process in place, for viewing
            // and copying
        } else
            return TRUE;
    } else {
        if (m_stats != NULL)
            free_dynamorio_stats(m_stats);
        m_stats = new_stats;
    }

    uint i;
#define MAX_VISIBLE_STATS 75
#define STATS_BUFSZ \
    (MAX_VISIBLE_STATS * (sizeof(single_stat_t) * 2 /*cover %10u, \
                                                                       etc.*/))
    TCHAR buf[STATS_BUFSZ];
    TCHAR *c = buf;
    buf[0] = _T('\0');

    // If I just use a CString hooked up via DDX, re-setting the
    // text causes the top of the visible box to show the top of
    // the stats string; plus the scrollbar goes to the top,
    // unless held in place; probably any reset of the string
    // does an auto-reset of the display and scrollbar position.
    // To have all the text in there do this:
    //      int scroll_pos = m_StatsCtl.GetScrollPos(SB_VERT);
    //      m_StatsCtl.SetWindowText(buf);
    //      m_StatsCtl.LineScroll(scroll_pos);
    // But then there's a lot of flickering: too much.
    // We only put visible text lines in the CEdit box, to reduce the
    // flicker.  It's too hard to do it w/ the CEdit's own scrollbar,
    // which sets itself based on the actual text, so we have a
    // separate one.

    // HACK: I don't know how to find out how many lines will
    // fit w/o putting actual text in there
    if (m_StatsViewLines == 0) {
        for (i = 0; i < m_stats->num_stats &&
             i < MAX_VISIBLE_STATS /* can't be more than this */;
             i++) {
            if (c >= &buf[STATS_BUFSZ - STAT_NAME_MAX_LEN * 2])
                break;
            c += PrintStat(c, i, FALSE /*no filter*/);
            assert(c < &buf[STATS_BUFSZ - 1]);
        }
        m_StatsCtl.SetWindowText(buf);
        UpdateData(FALSE); // write to screen
        // I tried having only one screenful of string there, and
        // setting the scrollbar range to be larger, but it doesn't
        // seem to support that.
        RECT rect;
        m_StatsCtl.GetRect(&rect);
        CPoint pos(rect.right, rect.bottom);
        m_StatsViewLines = HIWORD(m_StatsCtl.CharFromPos(pos));
        assert(m_StatsViewLines > 0);
        m_StatsSB.SetScrollRange(0, m_stats->num_stats - 1, TRUE /*redraw*/);
        c = buf;

        SCROLLINFO info;
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_PAGE;
        info.nPage = m_StatsViewLines;
        m_StatsSB.SetScrollInfo(&info);
    }

    int scroll_pos = m_StatsSB.GetScrollPos();
    DWORD shown = 0;
    uint printed;
    for (i = scroll_pos; i < m_stats->num_stats && shown < m_StatsViewLines; i++) {
        if (c >= &buf[STATS_BUFSZ - STAT_NAME_MAX_LEN * 2])
            break;
        printed = PrintStat(c, i, TRUE /*filter*/);
        c += printed;
        assert(c < &buf[STATS_BUFSZ - 1]);
        if (printed > 0)
            shown++;
    }
    m_StatsCtl.SetWindowText(buf);
    // num_stats could have changed so update
    m_StatsSB.SetScrollRange(0, m_stats->num_stats - 1, TRUE /*redraw*/);

    if (new_stats == NULL)
        m_Exited = _T("  Exited"); // line right end up with Running
    else
        m_Exited = _T("Running");
#ifndef DRSTATS_DEMO
    m_LogLevel.Format(_T("%d"), m_stats->loglevel);
    m_LogMask.Format(_T("0x%05X"), m_stats->logmask);
    m_LogDir.Format(_T("%") ASCII_PRINTF, m_stats->logdir);
#endif

    if (m_clientStats != NULL) {
#define CLIENTSTATS_BUFSZ USHRT_MAX
        TCHAR clientstats_buf[CLIENTSTATS_BUFSZ];
        PrintClientStats(clientstats_buf, &clientstats_buf[CLIENTSTATS_BUFSZ - 1]);
        m_ClientStats.Format(_T("%s"), clientstats_buf);
    } else
        m_ClientStats.Format(_T(""));

    UpdateData(FALSE); // write to screen
    return TRUE;
}

#ifndef DRSTATS_DEMO
void
CDynamoRIOView::OnChangeLogging()
{
    if (m_stats == NULL) {
        MessageBox(_T("No instance is selected"), _T("Error"), MB_OK | MYMBFLAGS);
        return;
    }
    if (m_stats->loglevel == 0) {
        MessageBox(_T("If the application began with log level 0, its logging\n")
                   _T("cannot be changed.\n"),
                   _T("Notification"), MB_OK | MYMBFLAGS);
        return;
    }
    CLoggingDlg dlg(m_stats->loglevel, m_stats->logmask);
    int res = dlg.DoModal();
    if (res == IDCANCEL)
        return;

    int level = dlg.GetLevel();
    int mask = dlg.GetMask();

    // FIXME: need to write via drmarker
    m_stats->loglevel = level;
    m_stats->logmask = mask;
    m_LogLevel.Format(_T("%d"), m_stats->loglevel);
    m_LogMask.Format(_T("0x%04X"), m_stats->logmask);
    UpdateData(FALSE); // write to screen
}

void
CDynamoRIOView::OnLogDirExplore()
{
    if (m_stats == NULL) {
        MessageBox(_T("No instance is selected"), _T("Error"), MB_OK | MYMBFLAGS);
        return;
    }

    UpdateData(TRUE); // get data from controls

    if (m_LogDir.Find(_T("<none")) == 0) {
        MessageBox(_T("There is no log dir because the loglevel was 0 when the ")
                   _T("application started"),
                   _T("Error"), MB_OK | MYMBFLAGS);
        return;
    }

    HINSTANCE res =
        ShellExecute(m_hWnd, _T("explore"), m_LogDir, NULL, NULL, SW_SHOWNORMAL);
    if ((int)res <= 32) {
        TCHAR msg[MAX_PATH * 2];
        _stprintf(msg, _T("Error exploring %s"), m_LogDir);
        MessageBox(msg, _T("Error"), MB_OK | MYMBFLAGS);
    }
}
#endif /* DRSTATS_DEMO */

void
CDynamoRIOView::OnEditCopystats()
{
    if (m_ProcessList.GetCurSel() == 0) {
        MessageBox(_T("No instance selected"), _T("Error"), MB_OK | MYMBFLAGS);
        return;
    }
    if (!OpenClipboard()) {
        MessageBox(_T("Error opening clipboard"), _T("Error"), MB_OK | MYMBFLAGS);
        return;
    }
    EmptyClipboard();

#define CLIPBOARD_BUFSZ USHRT_MAX
    TCHAR buf[CLIPBOARD_BUFSZ];
    TCHAR *pos = buf;

    if (m_selected_pid > 0) {
        if (m_stats != NULL) {
            pos += _stprintf(pos, _T("Process id                  = " PIDFMT "\r\n"),
                             m_stats->process_id);
            pos += _stprintf(
                pos, _T("Process name                = %") ASCII_PRINTF _T("\r\n"),
                m_stats->process_name);
            pos += _stprintf(pos, _T("Status                      = %s\r\n"),
                             m_Exited.GetBuffer(0));
#ifndef DRSTATS_DEMO
            pos += _stprintf(pos, _T("Log mask                    = %s\r\n"),
                             m_LogMask.GetBuffer(0));
            pos += _stprintf(pos, _T("Log level                   = %s\r\n"),
                             m_LogLevel.GetBuffer(0));
            pos += _stprintf(pos, _T("Log file                    = %s\r\n"),
                             m_LogDir.GetBuffer(0));
#endif
            pos += _stprintf(pos, _T("\r\nSTATS\r\n"));
            uint i;
            for (i = 0; i < m_stats->num_stats; i++) {
                if (pos >= &buf[STATS_BUFSZ - STAT_NAME_MAX_LEN * 2])
                    break;
                // FIXME: Filter here too
                pos += PrintStat(pos, i, TRUE /*filter*/);
                assert(pos < &buf[STATS_BUFSZ - 1]);
            }
        } else {
            CString pname;
            m_ProcessList.GetLBText(m_ProcessList.GetCurSel(), pname);
            _stprintf(pos, _T("%s"), pname.GetString());
            pos += _tcslen(pos);
        }
    }
    if (m_clientStats != NULL) {
        pos += PrintClientStats(pos, &buf[STATS_BUFSZ - 1]);
    }

    size_t len = _tcslen(buf);

    // Allocate a global memory object for the text.
    HGLOBAL hglbCopy = GlobalAlloc(GMEM_DDESHARE, (len + 1) * sizeof(TCHAR));
    if (hglbCopy == NULL) {
        CloseClipboard();
        return;
    }

    // Lock the handle and copy the text to the buffer.
    LPTSTR lptstrCopy = (LPTSTR)GlobalLock(hglbCopy);
    memcpy(lptstrCopy, buf, (len + 1) * sizeof(TCHAR));
    lptstrCopy[len] = (TCHAR)0; // null TCHARacter
    GlobalUnlock(hglbCopy);

    // Place the handle on the clipboard.
    SetClipboardData(
#ifdef UNICODE
        CF_UNICODETEXT,
#else
        CF_TEXT,
#endif
        hglbCopy);

    CloseClipboard();
}

void
CDynamoRIOView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    int minpos;
    int maxpos;
    if (pScrollBar == NULL)
        return;
    pScrollBar->GetScrollRange(&minpos, &maxpos);
    maxpos = pScrollBar->GetScrollLimit();
    int curpos = pScrollBar->GetScrollPos();

    // Determine the new position
    switch (nSBCode) {
    case SB_TOP: // Scroll to far top.
        curpos = minpos;
        break;

    case SB_BOTTOM: // Scroll to far bottom.
        curpos = maxpos;
        break;

    case SB_ENDSCROLL: // End scroll.
        break;

    case SB_LINEUP: // Scroll up.
        if (curpos > minpos)
            curpos--;
        break;

    case SB_LINEDOWN: // Scroll down.
        if (curpos < maxpos)
            curpos++;
        break;

    case SB_PAGEUP: // Scroll one page up.
    {
        // Get the page size.
        SCROLLINFO info;
        pScrollBar->GetScrollInfo(&info, SIF_ALL);
        if (curpos > minpos)
            curpos = max(minpos, curpos - (int)info.nPage);
    } break;

    case SB_PAGEDOWN: // Scroll one page down.
    {
        // Get the page size.
        SCROLLINFO info;
        pScrollBar->GetScrollInfo(&info, SIF_ALL);
        if (curpos < maxpos)
            curpos = min(maxpos, curpos + (int)info.nPage);
    } break;

    case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
        curpos = nPos;     // of the scroll box at the end of the drag operation.
        break;

    case SB_THUMBTRACK: // Drag scroll box to specified position. nPos is the
        curpos = nPos;  // position that the scroll box has been dragged to.
        break;
    }

    // Set the new position of the thumb (scroll box).
    pScrollBar->SetScrollPos(curpos);

    CView::OnVScroll(nSBCode, nPos, pScrollBar);

    // FIXME: we could try to more smoothly scroll w/ the last-copied m_stats
    // instead of getting all new values
    Refresh();
}
