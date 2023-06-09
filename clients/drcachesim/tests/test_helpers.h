/* **********************************************************
 * Copyright (c) 2023 Google, Inc.  All rights reserved.
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
 * * Neither the name of Google, Inc. nor the names of its contributors may be
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

/* helpers.h: utilities for tests. */

#ifndef _TEST_HELPERS_H_
#define _TEST_HELPERS_H_ 1

#include <stdio.h>

#ifdef WINDOWS
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#        include <windows.h>
#    endif
#    ifdef DEBUG
#        include <crtdbg.h>
#    endif
#    include <stdlib.h>

// We use the same controls as in suite/tests/tools.h to disable popups.

LONG WINAPI
console_exception_filter(struct _EXCEPTION_POINTERS *pExceptionInfo);

static inline void
disable_popups()
{
    // Set the global unhandled exception filter to the exception filter
    SetUnhandledExceptionFilter(console_exception_filter);

    // Avoid pop-up messageboxes in tests.
    if (!IsDebuggerPresent()) {
#    ifdef DEBUG
        // Set for _CRT_{WARN,ERROR,ASSERT}.
        for (int i = 0; i < _CRT_ERRCNT; i++) {
            _CrtSetReportMode(i, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
            _CrtSetReportFile(i, _CRTDBG_FILE_STDERR);
        }
#    endif
        // Configure assert() and _wassert() in release build. */
        _set_error_mode(_OUT_TO_STDERR);
    }
}

#else
static inline void
disable_popups()
{
    // Nothing to do.
}
#endif

#endif /* _TEST_HELPERS_H_ */
