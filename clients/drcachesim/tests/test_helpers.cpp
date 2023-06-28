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
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifdef WINDOWS
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#        include <windows.h>
#    endif
#    ifdef DEBUG
#        include <crtdbg.h>
#    endif
#    include <stdio.h>
#    include <stdlib.h>
#endif

namespace dynamorio {
namespace drmemtrace {

#ifdef WINDOWS
// We use the same controls as in suite/tests/tools.h to disable popups.

static LONG WINAPI
console_exception_filter(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
    fprintf(stderr, "ERROR: Unhandled exception 0x%x caught\n",
            pExceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

void
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
void
disable_popups()
{
    // Nothing to do.
}
#endif

#ifndef NO_HELPER_MAIN
// The test implements this.
int
test_main(int argc, const char *argv[]);
#endif

} // namespace drmemtrace
} // namespace dynamorio

#ifndef NO_HELPER_MAIN
int
main(int argc, const char *argv[])
{
    dynamorio::drmemtrace::disable_popups();
    return dynamorio::drmemtrace::test_main(argc, argv);
}
#endif
